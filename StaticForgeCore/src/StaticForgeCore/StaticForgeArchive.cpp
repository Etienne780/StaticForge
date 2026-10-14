#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "StaticForgeArchive.h"
#include "Internal/InternalHelpers.h"
#include "Internal/MmapFile.h"
#include "Internal/LZ4Compression.h"

namespace StaticForge {

	StaticForgeArchive::StaticForgeArchive()
		: ErrorSupport(HAS_HEADER)
		, m_mmap(std::make_unique<Internal::MmapFile>())
		, m_compressor(std::make_unique<Internal::LZ4Compression>(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER)) {
	}

	StaticForgeArchive::~StaticForgeArchive() {
		CloseFileStream();
	}

	bool StaticForgeArchive::OpenFileStream() {
		m_stream.open(m_path, std::ios::binary | std::ios::in);
		return m_stream.is_open();
	}

	void StaticForgeArchive::CloseFileStream() {
		m_stream.close();
	}

	bool StaticForgeArchive::IsFileStreamOpen() const {
		return m_stream.is_open();
	}

	bool StaticForgeArchive::LoadAsset(const std::string& key, std::vector<std::byte>* outData) {
		uint64_t k = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		auto* entry = GetIndexEntry(k);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(entry, outData, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetAt(size_t fileIndex, std::vector<std::byte>* outData) {
		if (fileIndex >= m_indexEntries.size()) {
			AddError("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(m_indexEntries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntry(&m_indexEntries[fileIndex], outData, &error)) {
			AddError("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::Contains(const std::string& key) const {
		uint64_t k = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		return m_hashNameToEntry.count(k) != 0;
	}

	bool StaticForgeArchive::GetAssetSize(const std::string& key, uint64_t* outSize) const {
		uint64_t k = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		auto it = m_hashNameToEntry.find(k);
		if (it == m_hashNameToEntry.end() || it->second >= m_indexEntries.size())
			return false;

		*outSize = m_indexEntries[it->second].fileSize;
		return true;
	}

	bool StaticForgeArchive::VerifyAll() {
		if (!IsFileStreamOpen() && !OpenFileStream()) {
			AddError("Failed to verify, archive file '" + m_path.u8string() + "' could not be opened");
			return false;
		}

		// read in chunks, so large assets don't have to be loaded completely
		constexpr size_t chunkSize = 1024 * 1024;
		std::vector<char> buffer(chunkSize);

		bool allValid = true;
		for (size_t i = 0; i < m_indexEntries.size(); i++) {
			const auto& entry = m_indexEntries[i];
			const uint64_t storedSize = entry.compressedFileSize != 0 ? entry.compressedFileSize : entry.fileSize;

			m_stream.clear();
			m_stream.seekg(static_cast<std::streamoff>(m_header.dataOffset + entry.fileOffset), std::ios::beg);

			uint32_t checksum = 2166136261u;
			uint64_t remaining = storedSize;
			while (remaining > 0 && m_stream) {
				size_t readSize = static_cast<size_t>(std::min<uint64_t>(remaining, chunkSize));
				m_stream.read(buffer.data(), static_cast<std::streamsize>(readSize));
				checksum = Internal::FNV1a(buffer.data(), static_cast<size_t>(m_stream.gcount()), checksum);
				remaining -= static_cast<uint64_t>(m_stream.gcount());
			}

			if (remaining == 0 && checksum == entry.checksum)
				continue;

			allValid = false;
			std::string name = GetName(i);
			if (name.empty()) {
				std::ostringstream hash;
				hash << std::hex << std::setw(16) << std::setfill('0') << entry.hashName;
				name = "hash " + hash.str();
			}
			AddError("Asset '" + name + "' is damaged (" + (remaining != 0 ? "data is missing" : "checksum mismatch") + ")");
		}

		return allValid;
	}

	bool StaticForgeArchive::OpenMapped() {
		std::string error;
		if (!m_mmap->Open(m_path, &error)) {
			AddError("Failed to open map: " + error);
			return false;
		}
		return true;
	}

	void StaticForgeArchive::CloseMapped() {	
		m_mmap->Close();
	}

	bool StaticForgeArchive::IsMapped() const {
		return m_mmap->IsOpen();
	}

	AssetView StaticForgeArchive::GetAssetMapped(const std::string& key) {
		if (!IsMapped()) {
			AddError("Failed to get asset mapped for key '" + key + "', archive is not mapped");
			return {};
		}

		uint64_t k = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		auto* entry = GetIndexEntry(k);
		if (!entry) {
			AddError("Failed to get asset mapped, entry with key '" + key + "' not found");
			return {};
		}

		if (entry->compressedFileSize != 0) {
			AddError("Failed to get asset mapped for key '" + key
				+ "', asset is compressed - use LoadAsset() instead");
			return {};
		}

		const uint64_t absOffset = m_header.dataOffset + entry->fileOffset;
		const auto* src = reinterpret_cast<const std::byte*>(m_mmap->At(absOffset, entry->fileSize));
		if (!src) {
			AddError("Entry with key '" + key + "' is out of bounds"
				+ " (offset=" + std::to_string(absOffset)
				+ ", size=" + std::to_string(entry->fileSize)
				+ ", map size=" + std::to_string(m_mmap->Size()) + ")");
			return {};
		}

		AssetView view{};
		view.data = src;
		view.size = entry->fileSize;
		return view;
	}
	
	bool StaticForgeArchive::LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData) {
		if (!IsMapped()) {
			AddError("Failed to get asset mapped for key '" + key + "', archive is not mapped (use OpenMapped() before this call)");
			return false;
		}
		
		uint64_t k = Internal::HashFilename(Internal::NormalizeFilepathSlashes(key));
		auto* entry = GetIndexEntry(k);
		if (!entry) {
			AddError("Failed to load asset mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntryMapped(entry, outData, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::StoresNames() const {
		return m_header.nameTableHeaderOffset != 0;
	}

	const StaticForgePath& StaticForgeArchive::GetPath() const {
		return m_path;
	}

	uint64_t StaticForgeArchive::GetVersion() const {
		return m_header.version;
	}

	size_t StaticForgeArchive::GetFileCount() const {
		return m_indexEntries.size();
	}

	uint64_t StaticForgeArchive::GetIndexOffset() const {
		return m_header.indexOffset;
	}

	uint64_t StaticForgeArchive::GetIndexSize() const {
		return m_header.indexSize;
	}

	uint64_t StaticForgeArchive::GetDataOffset() const {
		return m_header.dataOffset;
	}

	std::string StaticForgeArchive::GetName(size_t index) const {
		if (!StoresNames())
			return {};

		auto it = m_indexToName.find(index);
		if (it == m_indexToName.end())
			return {};
		
		return it->second;
	}

	// the index getters return 0 for an invalid index
	uint64_t StaticForgeArchive::GetHashName(size_t index) const {
		return index < m_indexEntries.size() ? m_indexEntries[index].hashName : 0;
	}

	uint64_t StaticForgeArchive::GetFileOffset(size_t index) const {
		return index < m_indexEntries.size() ? m_indexEntries[index].fileOffset : 0;
	}

	uint64_t StaticForgeArchive::GetFileSize(size_t index) const {
		return index < m_indexEntries.size() ? m_indexEntries[index].fileSize : 0;
	}

	uint64_t StaticForgeArchive::GetFilePadding(size_t index) const {
		return index < m_indexEntries.size() ? m_indexEntries[index].filePadding : 0;
	}

	bool StaticForgeArchive::LoadEntry(
		Internal::StaticForgeIndexEntry* entry,
		std::vector<std::byte>* outData,
		std::string* errorOut
	) {
		if (!IsFileStreamOpen()) {
			if (!OpenFileStream()) {
				*errorOut = "Failed to open archive file at '" + m_path.u8string() + "'";
				return false;
			}
		}

		const bool isCompressed = entry->compressedFileSize != 0;
		const uint64_t readSize = isCompressed ? entry->compressedFileSize : entry->fileSize;
		const uint64_t start = m_header.dataOffset + entry->fileOffset;

		std::vector<std::byte> raw(readSize);

		// prevents a previous failed load from breaking all following loads
		m_stream.clear();
		m_stream.seekg(static_cast<std::streamoff>(start), std::ios::beg);
		if (!m_stream) {
			*errorOut = "Failed to seek at offset " + std::to_string(start);
			return false;
		}

		m_stream.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(readSize));
		if (!m_stream) {
			*errorOut = "Failed to read " + std::to_string(readSize) + " bytes at offset " + std::to_string(start);
			return false;
		}

		if (!VerifyChecksum(raw.data(), readSize, entry->checksum, errorOut))
			return false;

		if (isCompressed) {
			std::vector<std::byte> allDecompressed;
			// fileSize comes from the archive, so only reserve what the block sizes allow
			allDecompressed.reserve(static_cast<size_t>(std::min<uint64_t>(entry->fileSize, MAX_COMPRESSION_BLOCK_SIZE)));

			size_t pos = 0;
			while (pos < raw.size()) {
				// read Framing-Header
				if (pos + sizeof(uint32_t) * 2 > raw.size()) {
					*errorOut = "Compressed block framing error at pos " + std::to_string(pos);
					return false;
				}

				uint32_t compSize32 = 0;
				uint32_t origSize32 = 0;
				std::memcpy(&compSize32, raw.data() + pos, sizeof(uint32_t));
				std::memcpy(&origSize32, raw.data() + pos + sizeof(uint32_t), sizeof(uint32_t));
				if (!Internal::IsLittleEndian()) {
					compSize32 = Internal::SwapEndian(compSize32);
					origSize32 = Internal::SwapEndian(origSize32);
				}
				pos += sizeof(uint32_t) * 2;

				if (pos + compSize32 > raw.size()) {
					*errorOut = "Compressed block overflows data at pos " + std::to_string(pos);
					return false;
				}

				if (origSize32 > MAX_COMPRESSION_BLOCK_SIZE || allDecompressed.size() + origSize32 > entry->fileSize) {
					*errorOut = "Compressed block at pos " + std::to_string(pos) + " has an invalid size (archive is corrupted)";
					return false;
				}

				auto block = m_compressor->Decompress(
					raw.data() + pos,
					static_cast<size_t>(compSize32),
					static_cast<size_t>(origSize32)
				);
				pos += compSize32;

				if (!m_compressor->IsValid()) {
					*errorOut = "Decompression failed at block (pos=" + std::to_string(pos) + "): "
						+ m_compressor->GetError();
					return false;
				}

				allDecompressed.insert(allDecompressed.end(), block.begin(), block.end());
			}

			if (allDecompressed.size() != entry->fileSize) {
				*errorOut = "Decompressed size mismatch (expected "
					+ std::to_string(entry->fileSize) + ", got "
					+ std::to_string(allDecompressed.size()) + ")";
				return false;
			}

			*outData = std::move(allDecompressed);
		}
		else {
			*outData = std::move(raw);
		}

		return true;
	}

	bool StaticForgeArchive::LoadEntryMapped(
		Internal::StaticForgeIndexEntry* entry,
		std::vector<std::byte>* outData,
		std::string* errorOut
	) {
		if (entry->compressedFileSize != 0) {
			*errorOut = "Entry is compressed - memory-mapped loading is not supported for compressed assets";
			return false;
		}

		const uint64_t storedSize = entry->fileSize;
		const uint64_t absOffset = m_header.dataOffset + entry->fileOffset;

		const auto* src = reinterpret_cast<const std::byte*>(m_mmap->At(absOffset, storedSize));
		if (!src) {
			*errorOut = "Entry is out of bounds"
				+ std::string(" (offset=") + std::to_string(absOffset)
				+ ", size=" + std::to_string(storedSize)
				+ ", map size=" + std::to_string(m_mmap->Size()) + ")";
			return false;
		}

		if (!VerifyChecksum(src, storedSize, entry->checksum, errorOut))
			return false;

		(*outData).assign(src, src + storedSize);
		return true;
	}

	Internal::StaticForgeIndexEntry* StaticForgeArchive::GetIndexEntry(uint64_t hash) {
		auto it = m_hashNameToEntry.find(hash);
		if (it == m_hashNameToEntry.end())
			return nullptr;
#ifdef DEBUG
		if (it->second >= m_indexEntries.size()) {
			std::cout << "StaticForgeArchive::GetIndexEntry: Index entry out of bounds for hash '" << hash << "'" << std::endl;
			return nullptr;
		}
#endif 

		return &m_indexEntries[it->second];
	}

	void StaticForgeArchive::Reset() {
		ClearErrors();
		CloseFileStream();
		m_stream.clear();
		CloseMapped();

		m_path.clear();
		m_header = {};
		m_indexEntries.clear();
		m_hashNameToEntry.clear();
		m_indexToName.clear();
	}

	bool StaticForgeArchive::VerifyChecksum(
		const std::byte* data,
		uint64_t size,
		uint32_t expected,
		std::string* errorOut
	) const {
		uint32_t computed = Internal::FNV1a(data, static_cast<size_t>(size), 2166136261u);
		if (computed != expected) {
			*errorOut = "Checksum mismatch for entry (expected " + std::to_string(expected) + ", got " + std::to_string(computed) + ")";
			return false;
		}
		return true;
	}

}