#pragma once
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>

#include "StaticForgeTypes.h"
#include "StaticForgeReader.h"
#include "ErrorSupport.h"

namespace StaticForge {

	namespace Internal {

		class MmapFile;
		class LZ4Compression;

	}

	class StaticForgeReader;

	/**
	 * @brief Lightweight view into asset data stored inside a StaticForge archive.
	 */
	struct AssetView {
		const std::byte* data = nullptr;
		size_t size = 0;
	};

	/**
	 * @brief Represents an opened StaticForge archive and provides asset access.
	 *
	 * Supports both stream-based and memory-mapped reading.
	 */
	class StaticForgeArchive : public ErrorSupport {
		friend class StaticForgeReader;
	public:
		/**
		 * @brief Creates an empty archive object.
		 */
		StaticForgeArchive();

		/**
		 * @brief Closes all open resources.
		 */
		~StaticForgeArchive();

		StaticForgeArchive(const StaticForgeArchive&) = delete;
		StaticForgeArchive& operator=(const StaticForgeArchive&) = delete;

		StaticForgeArchive(StaticForgeArchive&&) noexcept = default;
		StaticForgeArchive& operator=(StaticForgeArchive&&) noexcept = default;

		/**
		 * @brief Opens the archive file stream for reading.
		 *
		 * @return true if the file was opened successfully.
		 */
		bool OpenFileStream();

		/**
		 * @brief Closes the file stream if open.
		 */
		void CloseFileStream();

		/**
		 * @brief Checks whether the file stream is currently open.
		 *
		 * @return true if open.
		 */
		bool IsFileStreamOpen() const;

		/**
		 * @brief Loads an asset into a byte buffer using the file stream.
		 *
		 * @param key Asset path/key inside the archive.
		 * @param outData Output buffer receiving the asset data.
		 * @return true if loading succeeded.
		 */
		bool LoadAsset(const std::string& key, std::vector<std::byte>* outData);

		/**
		 * @brief Loads an asset by its index using the file stream.
		 *
		 * Useful to iterate over all assets.
		 *
		 * @param fileIndex Index of the file (0 to GetFileCount() - 1).
		 * @param outData Output buffer receiving the asset data.
		 * @return true if loading succeeded.
		 */
		bool LoadAssetAt(size_t fileIndex, std::vector<std::byte>* outData);

		/**
		 * @brief Loads an asset into an existing buffer using the file stream.
		 *
		 * @param key Asset path/key inside the archive.
		 * @param buffer Output buffer, must hold at least the asset size (see GetAssetSize()).
		 * @param bufferSize Size of the buffer in bytes.
		 * @return true if loading succeeded.
		 */
		bool LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize);

		/**
		 * @brief Loads an asset by its index into an existing buffer using the file stream.
		 *
		 * @param fileIndex Index of the file (0 to GetFileCount() - 1).
		 * @param buffer Output buffer, must hold at least the asset size (see GetFileSize()).
		 * @param bufferSize Size of the buffer in bytes.
		 * @return true if loading succeeded.
		 */
		bool LoadAssetAt(size_t fileIndex, std::byte* buffer, uint64_t bufferSize);

		/**
		 * @brief Loads a part of an asset using the file stream, e.g. to stream audio.
		 *
		 * The checksum is not validated, because only a part of the data is read (see VerifyAll()).
		 * For compressed assets only the blocks containing the range are decompressed.
		 *
		 * @param key Asset path/key inside the archive.
		 * @param offset Start of the range in the original (uncompressed) asset.
		 * @param size Size of the range in bytes, offset + size must not exceed the asset size.
		 * @param buffer Output buffer, must hold at least size bytes.
		 * @return true if loading succeeded.
		 */
		bool LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer);

		/**
		 * @brief Loads a part of an asset by its index using the file stream, see LoadAssetRange().
		 */
		bool LoadAssetRangeAt(size_t fileIndex, uint64_t offset, uint64_t size, std::byte* buffer);

		/**
		 * @brief Checks whether the archive contains an asset.
		 *
		 * @param key Asset path/key inside the archive.
		 * @return true if the asset exists.
		 */
		bool Contains(const std::string& key) const;

		/**
		 * @brief Returns the size of an asset without loading it.
		 *
		 * @param key Asset path/key inside the archive.
		 * @param outSize Receives the original (uncompressed) size in bytes.
		 * @return true if the asset exists.
		 */
		bool GetAssetSize(const std::string& key, uint64_t* outSize) const;

		/**
		 * @brief Checks the checksum of every asset, e.g. to detect a damaged installation at startup.
		 *
		 * Adds an error for every damaged asset.
		 *
		 * @return true if all assets are valid.
		 */
		bool VerifyAll();

		/**
		 * @brief Opens the archive using memory mapping.
		 *
		 * @return true if mapping succeeded.
		 */
		bool OpenMapped();

		/**
		 * @brief Closes the memory-mapped file.
		 */
		void CloseMapped();

		/**
		 * @brief Checks whether the archive is memory-mapped.
		 *
		 * @return true if mapped.
		 */
		bool IsMapped() const;

		/**
		 * @brief Returns a non-owning view of an asset in mapped mode.
		 *
		 * Not possible for compressed assets, use LoadAssetMapped() for them.
		 *
		 * @param key Asset path/key.
		 * @return View into mapped memory (invalid if unmapped or key missing).
		 */
		AssetView GetAssetMapped(const std::string& key);

		/**
		 * @brief Loads an asset into a buffer using memory-mapped access.
		 *
		 * @param key Asset path/key.
		 * @param outData Output buffer.
		 * @return true if loading succeeded.
		 */
		bool LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData);

		/**
		 * @brief Loads an asset into an existing buffer using memory-mapped access.
		 *
		 * @param key Asset path/key.
		 * @param buffer Output buffer, must hold at least the asset size (see GetAssetSize()).
		 * @param bufferSize Size of the buffer in bytes.
		 * @return true if loading succeeded.
		 */
		bool LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize);

		/**
		 * @brief Loads a part of an asset using memory-mapped access, see LoadAssetRange().
		 */
		bool LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer);

		/**
		 * @brief Checks whether this archive contains a filename table.
		 *
		 * @return true if names are stored.
		 */
		bool StoresNames() const;

		/**
		 * @brief Returns the archive file path.
		 */
		const StaticForgePath& GetPath() const;

		/**
		 * @brief Returns archive format version.
		 */
		uint64_t GetVersion() const;

		/**
		 * @brief Returns number of files stored in the archive.
		 */
		size_t GetFileCount() const;

		/**
		 * @brief Returns offset of the index table.
		 */
		uint64_t GetIndexOffset() const;

		/**
		 * @brief Returns size of the index table.
		 */
		uint64_t GetIndexSize() const;

		/**
		 * @brief Returns offset of the raw data section.
		 */
		uint64_t GetDataOffset() const;

		/**
		 * @brief Returns the filename for a given index (if available).
		 *
		 * @param fileIndex Index of the file.
		 * @return Filename or empty string.
		 */
		std::string GetName(size_t fileIndex) const;

		/**
		 * @brief Returns the hash of the filename at the given index. (0 if the index is invalid)
		 */
		uint64_t GetHashName(size_t fileIndex) const;

		/**
		 * @brief Returns the file offset inside the archive. (0 if the index is invalid)
		 */
		uint64_t GetFileOffset(size_t fileIndex) const;

		/**
		 * @brief Returns the file size. (0 if the index is invalid)
		 */
		uint64_t GetFileSize(size_t fileIndex) const;

		/**
		 * @brief Returns padding size after the file data. (0 if the index is invalid)
		 */
		uint64_t GetFilePadding(size_t fileIndex) const;

	private:
		StaticForgePath m_path;
		std::ifstream m_stream;
		std::unique_ptr<Internal::MmapFile> m_mmap;
		std::unique_ptr<Internal::LZ4Compression> m_compressor;

		Internal::StaticForgeHeader m_header;
		std::vector<Internal::StaticForgeIndexEntry> m_indexEntries;
		std::unordered_map<uint64_t, size_t> m_hashNameToEntry;
		std::unordered_map<size_t, std::string> m_indexToName;

		/**
		 * @brief Loads a whole entry into outVector (resized) or into outBuffer.
		 */
		bool LoadEntry(const Internal::StaticForgeIndexEntry& entry, bool mapped,
			std::vector<std::byte>* outVector, std::byte* outBuffer, uint64_t bufferSize, std::string* errorOut);

		/**
		 * @brief Loads a part of an entry into outBuffer, without checksum validation.
		 */
		bool LoadEntryRange(const Internal::StaticForgeIndexEntry& entry, bool mapped,
			uint64_t offset, uint64_t size, std::byte* outBuffer, std::string* errorOut);

		/**
		 * @brief Opens the stream if needed or checks that the archive is mapped.
		 */
		bool PrepareSource(bool mapped, std::string* errorOut);

		/**
		 * @brief Reads size bytes at an absolute file offset from the stream.
		 */
		bool ReadStream(uint64_t absOffset, uint64_t size, std::byte* dst, std::string* errorOut);

		/**
		 * @brief Returns a pointer to size stored bytes of an entry, starting at pos.
		 *
		 * Mapped: points into the mapping. Stream: the bytes are read into temp.
		 */
		const std::byte* ReadStored(const Internal::StaticForgeIndexEntry& entry, bool mapped,
			uint64_t pos, uint64_t size, std::vector<std::byte>* temp, std::string* errorOut);

		/**
		 * @brief Walks the compressed blocks of an entry and decompresses the blocks inside the range.
		 *
		 * stored: all stored bytes in memory, or nullptr to read each block with ReadStored().
		 * dst: receives the range, or nullptr to only validate the block sizes.
		 */
		bool DecompressBlocks(const Internal::StaticForgeIndexEntry& entry, bool mapped, const std::byte* stored,
			uint64_t rangeStart, uint64_t rangeSize, std::byte* dst, std::string* errorOut);

		Internal::StaticForgeIndexEntry* GetIndexEntry(uint64_t hash);
		Internal::StaticForgeIndexEntry* GetIndexEntry(const std::string& key);

		/**
		 * @brief Closes all open resources and clears the loaded archive data.
		 */
		void Reset();

		/**
		 * @brief Validates checksum of loaded asset data.
		 */
		bool VerifyChecksum(const std::byte* data, uint64_t size,
			uint32_t expected, std::string* errorOut) const;
	};

}