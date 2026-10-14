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

		bool LoadEntry(Internal::StaticForgeIndexEntry* entry, std::vector<std::byte>* outData, std::string* errorOut);
		bool LoadEntryMapped(Internal::StaticForgeIndexEntry* entry, std::vector<std::byte>* outData, std::string* errorOut);

		Internal::StaticForgeIndexEntry* GetIndexEntry(uint64_t hash);

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