#pragma once
#include <memory>
#include <vector>
#include <string>
#include <fstream>

#include "StaticForgeTypes.h"
#include "StaticForgeArchive.h"

namespace StaticForge {

	namespace Internal {

		class MmapFile;
		struct ArchiveData;
		struct EntrySource;

	}

	class StaticForgeReader;

	/**
	 * @brief Archive that can be used from several threads at the same time.
	 *
	 * Stream loads use a pool of file streams: each load takes a free stream and gives it back afterwards.
	 * If all streams are in use, the load waits until one is free. Streams are opened when they are needed.
	 * Mapped loads don't need the pool.
	 *
	 * Thread-safe: all Load functions, VerifyAll, Contains, GetAssetSize, the getters, the pool functions
	 * and the error functions.
	 * Not thread-safe: OpenMapped and CloseMapped, they must not be called while other threads load assets.
	 *
	 * Errors: every function takes an optional errorOut. If it is set, the error is only written there.
	 * Otherwise it is collected per thread, see TakeError() and TakeAllErrors().
	 */
	class StaticForgeConcurrentArchive {
		friend class StaticForgeReader;
		friend class StaticForgeArchive;
	public:
		/**
		 * @brief Creates an empty archive object, load it with StaticForgeReader::Load().
		 */
		StaticForgeConcurrentArchive();

		/**
		 * @brief Closes all open resources.
		 */
		~StaticForgeConcurrentArchive();

		// not copyable, use Clone()
		StaticForgeConcurrentArchive(const StaticForgeConcurrentArchive&) = delete;
		StaticForgeConcurrentArchive& operator=(const StaticForgeConcurrentArchive&) = delete;

		StaticForgeConcurrentArchive(StaticForgeConcurrentArchive&&);
		StaticForgeConcurrentArchive& operator=(StaticForgeConcurrentArchive&&);

		/**
		 * @brief Creates a new concurrent archive that shares the loaded data and the mapping.
		 *
		 * The clone has its own stream pool (same pool size) and its own errors.
		 */
		StaticForgeConcurrentArchive Clone() const;

		/**
		 * @brief Creates a normal StaticForgeArchive that shares the loaded data and the mapping.
		 */
		StaticForgeArchive CloneArchive() const;

		/**
		 * @brief Sets the maximum number of open file streams.
		 *
		 * Can be called while loading: if the pool gets smaller, streams are closed when they are given back.
		 *
		 * @param poolSize Number of streams, DEFAULT_POOL_SIZE (0) = std::thread::hardware_concurrency().
		 */
		void SetPoolSize(size_t poolSize);

		/**
		 * @brief Returns the maximum number of open file streams.
		 */
		size_t GetPoolSize() const;

		/**
		 * @brief Returns how many streams are used by loads right now (only a snapshot).
		 */
		size_t GetActiveStreamCount() const;

		/**
		 * @brief Returns how many streams are open right now, used or free (only a snapshot).
		 */
		size_t GetOpenStreamCount() const;

		/**
		 * @brief Closes all streams that are not used right now. They are opened again when needed.
		 */
		void CloseFreeStreams();

		/**
		 * @brief Loads an asset into a byte buffer using a stream of the pool.
		 *
		 * @param key Asset path/key inside the archive.
		 * @param outData Output buffer receiving the asset data.
		 * @param errorOut Receives the error (optional, otherwise it is collected for the thread).
		 * @return true if loading succeeded.
		 */
		bool LoadAsset(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset by its index, see LoadAsset().
		 */
		bool LoadAssetAt(size_t fileIndex, std::vector<std::byte>* outData, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset into an existing buffer, see StaticForgeArchive::LoadAsset().
		 */
		bool LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset by its index into an existing buffer, see StaticForgeArchive::LoadAssetAt().
		 */
		bool LoadAssetAt(size_t fileIndex, std::byte* buffer, uint64_t bufferSize, std::string* errorOut = nullptr);

		/**
		 * @brief Loads a part of an asset without checksum validation, see StaticForgeArchive::LoadAssetRange().
		 */
		bool LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut = nullptr);

		/**
		 * @brief Loads a part of an asset by its index, see StaticForgeArchive::LoadAssetRange().
		 */
		bool LoadAssetRangeAt(size_t fileIndex, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut = nullptr);

		/**
		 * @brief Checks whether the archive contains an asset.
		 */
		bool Contains(const std::string& key) const;

		/**
		 * @brief Returns the original (uncompressed) size of an asset without loading it.
		 *
		 * @return true if the asset exists.
		 */
		bool GetAssetSize(const std::string& key, uint64_t* outSize) const;

		/**
		 * @brief Checks the checksum of every asset.
		 *
		 * @param errorOut Receives one line per damaged asset (optional, otherwise they are collected for the thread).
		 * @return true if all assets are valid.
		 */
		bool VerifyAll(std::string* errorOut = nullptr);

		/**
		 * @brief Opens the archive using memory mapping. Not thread-safe.
		 */
		bool OpenMapped(std::string* errorOut = nullptr);

		/**
		 * @brief Releases the mapping, clones keep it open. Not thread-safe, invalidates all AssetViews.
		 */
		void CloseMapped();

		/**
		 * @brief Checks whether the archive is memory-mapped.
		 */
		bool IsMapped() const;

		/**
		 * @brief Returns a non-owning view of an uncompressed asset in mapped mode.
		 */
		AssetView GetAssetMapped(const std::string& key, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset using memory-mapped access.
		 */
		bool LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset into an existing buffer using memory-mapped access.
		 */
		bool LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut = nullptr);

		/**
		 * @brief Loads a part of an asset using memory-mapped access, without checksum validation.
		 */
		bool LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut = nullptr);

		/**
		 * @brief Checks whether this archive contains a filename table.
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
		 * @brief Returns the filename for a given index (empty if not available).
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

		/**
		 * @brief Checks whether the calling thread has no collected errors.
		 */
		bool IsValid() const;

		/**
		 * @brief Returns the collected errors of the calling thread.
		 */
		std::string GetError() const;

		/**
		 * @brief Returns the collected errors of the calling thread and clears them.
		 */
		std::string TakeError();

		/**
		 * @brief Clears the collected errors of the calling thread.
		 */
		void ClearErrors();

		/**
		 * @brief Returns the collected errors of all threads, each line starts with "[Thread <n>]".
		 *
		 * Threads are numbered in the order of their first error, starting with 1.
		 */
		std::string GetAllErrors() const;

		/**
		 * @brief Returns the collected errors of all threads and clears them.
		 */
		std::string TakeAllErrors();

		/**
		 * @brief Clears the collected errors of all threads.
		 */
		void ClearAllErrors();

	private:
		struct State;

		// shared with clones, never changed after loading (nullptr if nothing is loaded)
		std::shared_ptr<const Internal::ArchiveData> m_data;
		// shared with clones, closed when the last archive releases it
		std::shared_ptr<Internal::MmapFile> m_mmap;
		// stream pool and errors
		std::unique_ptr<State> m_state;

		/**
		 * @brief Returns the loaded data, or empty data if nothing is loaded.
		 */
		const Internal::ArchiveData& Data() const;

		/**
		 * @brief Writes the error to errorOut, or collects it for the calling thread.
		 */
		void Report(const std::string& error, std::string* errorOut) const;

		/**
		 * @brief Loads a whole entry with a stream of the pool or from the mapping.
		 */
		bool LoadEntry(const Internal::StaticForgeIndexEntry& entry, bool mapped,
			std::vector<std::byte>* outVector, std::byte* outBuffer, uint64_t bufferSize, std::string* errorOut);

		/**
		 * @brief Loads a part of an entry with a stream of the pool or from the mapping.
		 */
		bool LoadEntryRange(const Internal::StaticForgeIndexEntry& entry, bool mapped,
			uint64_t offset, uint64_t size, std::byte* outBuffer, std::string* errorOut);

		/**
		 * @brief Adds an already opened stream to the pool as a free stream.
		 */
		void AddStreamToPool(std::ifstream stream);

		/**
		 * @brief Closes all open resources, clears the loaded data and all errors. Not thread-safe.
		 */
		void Reset();
	};

}
