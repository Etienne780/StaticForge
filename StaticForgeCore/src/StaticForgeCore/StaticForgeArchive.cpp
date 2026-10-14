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
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, outData, nullptr, 0, &error)) {
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
		if (!LoadEntry(m_indexEntries[fileIndex], false, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetAt(size_t fileIndex, std::byte* buffer, uint64_t bufferSize) {
		if (fileIndex >= m_indexEntries.size()) {
			AddError("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(m_indexEntries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntry(m_indexEntries[fileIndex], false, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset range, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, false, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRangeAt(size_t fileIndex, uint64_t offset, uint64_t size, std::byte* buffer) {
		if (fileIndex >= m_indexEntries.size()) {
			AddError("Failed to load asset range, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(m_indexEntries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(m_indexEntries[fileIndex], false, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset at index '" + std::to_string(fileIndex) + "': " + error);
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

		auto* entry = GetIndexEntry(key);
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
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		auto* entry = GetIndexEntry(key);
		if (!entry) {
			AddError("Failed to load asset range mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, true, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset mapped '" + key + "': " + error);
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
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		std::vector<std::byte>* outVector,
		std::byte* outBuffer,
		uint64_t bufferSize,
		std::string* errorOut
	) {
		if (!PrepareSource(mapped, errorOut))
			return false;

		const bool isCompressed = entry.compressedFileSize != 0;
		if (mapped && isCompressed) {
			*errorOut = "Entry is compressed - memory-mapped loading is not supported for compressed assets";
			return false;
		}

		if (!outVector) {
			if (bufferSize < entry.fileSize) {
				*errorOut = "Buffer is too small (asset size " + std::to_string(entry.fileSize)
					+ ", buffer size " + std::to_string(bufferSize) + ")";
				return false;
			}
			if (!outBuffer && entry.fileSize > 0) {
				*errorOut = "Buffer is null";
				return false;
			}
		}

		const uint64_t storedSize = isCompressed ? entry.compressedFileSize : entry.fileSize;

		// outVector is only changed on success
		std::vector<std::byte> local;

		// uncompressed data from the stream is read directly into the target
		if (!isCompressed && !mapped) {
			std::byte* dst = outBuffer;
			if (outVector) {
				local.resize(static_cast<size_t>(storedSize));
				dst = local.data();
			}

			if (!ReadStream(m_header.dataOffset + entry.fileOffset, storedSize, dst, errorOut))
				return false;
			if (!VerifyChecksum(dst, storedSize, entry.checksum, errorOut))
				return false;

			if (outVector)
				*outVector = std::move(local);
			return true;
		}

		std::vector<std::byte> temp;
		const std::byte* stored = ReadStored(entry, mapped, 0, storedSize, &temp, errorOut);
		if (!stored)
			return false;

		if (!VerifyChecksum(stored, storedSize, entry.checksum, errorOut))
			return false;

		// fileSize comes from the archive, so check the blocks before allocating it
		if (isCompressed && outVector && !DecompressBlocks(entry, mapped, stored, 0, entry.fileSize, nullptr, errorOut))
			return false;

		std::byte* dst = outBuffer;
		if (outVector) {
			local.resize(static_cast<size_t>(entry.fileSize));
			dst = local.data();
		}

		if (isCompressed) {
			if (!DecompressBlocks(entry, mapped, stored, 0, entry.fileSize, dst, errorOut))
				return false;
		}
		else if (entry.fileSize > 0) {
			std::memcpy(dst, stored, static_cast<size_t>(entry.fileSize));
		}

		if (outVector)
			*outVector = std::move(local);
		return true;
	}

	bool StaticForgeArchive::LoadEntryRange(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		uint64_t offset,
		uint64_t size,
		std::byte* outBuffer,
		std::string* errorOut
	) {
		if (!PrepareSource(mapped, errorOut))
			return false;

		const bool isCompressed = entry.compressedFileSize != 0;
		if (mapped && isCompressed) {
			*errorOut = "Entry is compressed - memory-mapped loading is not supported for compressed assets";
			return false;
		}

		if (offset > entry.fileSize || size > entry.fileSize - offset) {
			*errorOut = "Range (offset " + std::to_string(offset) + ", size " + std::to_string(size)
				+ ") is outside of the asset (size " + std::to_string(entry.fileSize) + ")";
			return false;
		}

		if (size == 0)
			return true;

		if (!outBuffer) {
			*errorOut = "Buffer is null";
			return false;
		}

		if (isCompressed)
			return DecompressBlocks(entry, mapped, nullptr, offset, size, outBuffer, errorOut);

		if (!mapped)
			return ReadStream(m_header.dataOffset + entry.fileOffset + offset, size, outBuffer, errorOut);

		std::vector<std::byte> unused;
		const std::byte* src = ReadStored(entry, true, offset, size, &unused, errorOut);
		if (!src)
			return false;

		std::memcpy(outBuffer, src, static_cast<size_t>(size));
		return true;
	}

	bool StaticForgeArchive::PrepareSource(bool mapped, std::string* errorOut) {
		if (mapped) {
			if (!IsMapped()) {
				*errorOut = "Archive is not mapped (use OpenMapped() before this call)";
				return false;
			}
			return true;
		}

		if (!IsFileStreamOpen() && !OpenFileStream()) {
			*errorOut = "Failed to open archive file at '" + m_path.u8string() + "'";
			return false;
		}
		return true;
	}

	bool StaticForgeArchive::ReadStream(uint64_t absOffset, uint64_t size, std::byte* dst, std::string* errorOut) {
		// prevents a previous failed load from breaking all following loads
		m_stream.clear();
		m_stream.seekg(static_cast<std::streamoff>(absOffset), std::ios::beg);
		if (!m_stream) {
			*errorOut = "Failed to seek at offset " + std::to_string(absOffset);
			return false;
		}

		if (size == 0)
			return true;

		m_stream.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
		if (!m_stream) {
			*errorOut = "Failed to read " + std::to_string(size) + " bytes at offset " + std::to_string(absOffset);
			return false;
		}
		return true;
	}

	const std::byte* StaticForgeArchive::ReadStored(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		uint64_t pos,
		uint64_t size,
		std::vector<std::byte>* temp,
		std::string* errorOut
	) {
		const uint64_t absOffset = m_header.dataOffset + entry.fileOffset + pos;

		if (mapped) {
			const auto* src = reinterpret_cast<const std::byte*>(m_mmap->At(absOffset, size));
			if (!src) {
				*errorOut = "Entry is out of bounds"
					+ std::string(" (offset=") + std::to_string(absOffset)
					+ ", size=" + std::to_string(size)
					+ ", map size=" + std::to_string(m_mmap->Size()) + ")";
			}
			return src;
		}

		temp->resize(static_cast<size_t>(size));
		if (!ReadStream(absOffset, size, temp->data(), errorOut))
			return nullptr;

		// never null on success, also if size is 0
		static const std::byte empty{};
		return temp->empty() ? &empty : temp->data();
	}

	bool StaticForgeArchive::DecompressBlocks(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		const std::byte* stored,
		uint64_t rangeStart,
		uint64_t rangeSize,
		std::byte* dst,
		std::string* errorOut
	) {
		constexpr uint64_t frameSize = sizeof(uint32_t) * 2;
		const uint64_t storedSize = entry.compressedFileSize;
		const uint64_t rangeEnd = rangeStart + rangeSize;

		std::vector<std::byte> temp;
		std::vector<std::byte> blockBuffer;

		uint64_t pos = 0;    // position in the stored data
		uint64_t outPos = 0; // position in the original data
		while (outPos < rangeEnd) {
			if (storedSize - pos < frameSize) {
				*errorOut = "Compressed block framing error at pos " + std::to_string(pos);
				return false;
			}

			// frame: [u32 compressed size][u32 original size]
			const std::byte* frame = stored ? stored + pos : ReadStored(entry, mapped, pos, frameSize, &temp, errorOut);
			if (!frame)
				return false;

			uint32_t compSize32 = 0;
			uint32_t origSize32 = 0;
			std::memcpy(&compSize32, frame, sizeof(uint32_t));
			std::memcpy(&origSize32, frame + sizeof(uint32_t), sizeof(uint32_t));
			if (!Internal::IsLittleEndian()) {
				compSize32 = Internal::SwapEndian(compSize32);
				origSize32 = Internal::SwapEndian(origSize32);
			}
			pos += frameSize;

			if (compSize32 > storedSize - pos) {
				*errorOut = "Compressed block overflows data at pos " + std::to_string(pos);
				return false;
			}

			if (origSize32 > MAX_COMPRESSION_BLOCK_SIZE || origSize32 > entry.fileSize - outPos) {
				*errorOut = "Compressed block at pos " + std::to_string(pos) + " has an invalid size (archive is corrupted)";
				return false;
			}

			const uint64_t blockEnd = outPos + origSize32;

			// only the blocks inside of the range are decompressed
			if (dst && blockEnd > rangeStart) {
				const std::byte* payload = stored ? stored + pos : ReadStored(entry, mapped, pos, compSize32, &temp, errorOut);
				if (!payload)
					return false;

				const uint64_t copyStart = std::max(rangeStart, outPos);
				const uint64_t copyEnd = std::min(rangeEnd, blockEnd);

				// a block that is only partly inside of the range is decompressed into blockBuffer first
				const bool wholeBlock = copyStart == outPos && copyEnd == blockEnd;
				std::byte* target = nullptr;
				if (wholeBlock) {
					target = dst + (outPos - rangeStart);
				}
				else {
					blockBuffer.resize(origSize32);
					target = blockBuffer.data();
				}

				if (!m_compressor->DecompressTo(payload, compSize32, target, origSize32)) {
					*errorOut = "Decompression failed at block (pos=" + std::to_string(pos) + "): "
						+ m_compressor->GetError();
					return false;
				}

				if (!wholeBlock) {
					std::memcpy(dst + (copyStart - rangeStart), blockBuffer.data() + (copyStart - outPos),
						static_cast<size_t>(copyEnd - copyStart));
				}
			}

			pos += compSize32;
			outPos = blockEnd;
		}

		return true;
	}

	Internal::StaticForgeIndexEntry* StaticForgeArchive::GetIndexEntry(const std::string& key) {
		return GetIndexEntry(Internal::HashFilename(Internal::NormalizeFilepathSlashes(key)));
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