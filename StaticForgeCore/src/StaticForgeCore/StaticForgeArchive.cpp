#include <iostream>
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
				+ "', asset is compressed — use LoadAsset() or LoadAssetMapped() instead");
			return {};
		}

		const uint64_t absOffset = m_header.dataOffset + entry->fileOffset;
		if (!m_mmap->InRange(absOffset, entry->fileSize)) {
			AddError("Entry with key '" + key + "' is out of bounds"
				+ " (offset=" + std::to_string(absOffset)
				+ ", size=" + std::to_string(entry->fileSize)
				+ ", map size=" + std::to_string(m_mmap->Size()) + ")");
			return {};
		}

		AssetView view{};
		view.data = reinterpret_cast<const std::byte*>(m_mmap->At(absOffset));
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

	uint64_t StaticForgeArchive::GetHashName(size_t index) const {
		return m_indexEntries[index].hashName;
	}

	uint64_t StaticForgeArchive::GetFileOffset(size_t index) const {
		return m_indexEntries[index].fileOffset;
	}

	uint64_t StaticForgeArchive::GetFileSize(size_t index) const {
		return m_indexEntries[index].fileSize;
	}

	uint64_t StaticForgeArchive::GetFilePadding(size_t index) const {
		return m_indexEntries[index].filePadding;
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
			allDecompressed.reserve(entry->fileSize);

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
				pos += sizeof(uint32_t) * 2;

				if (pos + compSize32 > raw.size()) {
					*errorOut = "Compressed block overflows data at pos " + std::to_string(pos);
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
			*errorOut = "Entry is compressed — memory-mapped loading is not supported for compressed assets";
			return false;
		}

		const uint64_t storedSize = entry->fileSize;
		const uint64_t absOffset = m_header.dataOffset + entry->fileOffset;

		if (!m_mmap->InRange(absOffset, storedSize)) {
			*errorOut = "Entry is out of bounds"
				+ std::string(" (offset=") + std::to_string(absOffset)
				+ ", size=" + std::to_string(storedSize)
				+ ", map size=" + std::to_string(m_mmap->Size()) + ")";
			return false;
		}

		const auto* src = reinterpret_cast<const std::byte*>(m_mmap->At(absOffset));
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