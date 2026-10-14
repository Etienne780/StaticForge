#pragma once
#include <memory>
#include <vector>
#include <string>

#include "StaticForgeTypes.h"
#include "StaticForgeConcurrentArchive.h"

namespace StaticForge {

	/**
	 * @brief Loads assets from several StaticForgeConcurrentArchive objects from several threads at once.
	 *
	 * The set doesn't own the archives, it only keeps pointers. An archive must not be destroyed while the set uses it,
	 * call Remove() or Clear() first. An asset path may only exist in one of the archives, Add() fails otherwise.
	 *
	 * If an added archive is loaded again with StaticForgeReader::Load, remove it and add it again.
	 *
	 * Thread-safe: all Load functions, VerifyAll, FindArchive, Contains, GetAssetSize and the error functions.
	 * Not thread-safe: Add, Remove and Clear, they must not be called while other threads load assets.
	 *
	 * Errors work like in StaticForgeConcurrentArchive: with errorOut the error is only written there,
	 * otherwise it is collected for the calling thread.
	 */
	class StaticForgeConcurrentArchiveSet {
	public:
		/**
		 * @brief Creates an empty set.
		 */
		StaticForgeConcurrentArchiveSet();

		/**
		 * @brief Doesn't touch the archives.
		 */
		~StaticForgeConcurrentArchiveSet();

		StaticForgeConcurrentArchiveSet(const StaticForgeConcurrentArchiveSet&) = delete;
		StaticForgeConcurrentArchiveSet& operator=(const StaticForgeConcurrentArchiveSet&) = delete;

		StaticForgeConcurrentArchiveSet(StaticForgeConcurrentArchiveSet&&) noexcept;
		StaticForgeConcurrentArchiveSet& operator=(StaticForgeConcurrentArchiveSet&&) noexcept;

		/**
		 * @brief Adds a loaded archive. Not thread-safe.
		 *
		 * Fails if the archive is null, not loaded, already added, or contains an asset path
		 * that an added archive has too. Nothing is added on failure.
		 */
		bool Add(StaticForgeConcurrentArchive* archive, std::string* errorOut = nullptr);

		/**
		 * @brief Removes an archive. Not thread-safe.
		 *
		 * @return true if the archive was part of the set.
		 */
		bool Remove(StaticForgeConcurrentArchive* archive);

		/**
		 * @brief Removes all archives. Not thread-safe.
		 */
		void Clear();

		/**
		 * @brief Returns the number of added archives.
		 */
		size_t GetArchiveCount() const;

		/**
		 * @brief Returns the archive that contains an asset, or nullptr.
		 */
		StaticForgeConcurrentArchive* FindArchive(const std::string& key) const;

		/**
		 * @brief Checks whether one of the archives contains an asset.
		 */
		bool Contains(const std::string& key) const;

		/**
		 * @brief Returns the original (uncompressed) size of an asset without loading it.
		 */
		bool GetAssetSize(const std::string& key, uint64_t* outSize) const;

		/**
		 * @brief Loads an asset, see StaticForgeConcurrentArchive::LoadAsset().
		 */
		bool LoadAsset(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset into an existing buffer.
		 */
		bool LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut = nullptr);

		/**
		 * @brief Loads a part of an asset without checksum validation.
		 */
		bool LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut = nullptr);

		/**
		 * @brief Returns a view of an uncompressed asset, the archive that contains it must be mapped.
		 */
		AssetView GetAssetMapped(const std::string& key, std::string* errorOut = nullptr);

		/**
		 * @brief Loads an asset using memory-mapped access, the archive that contains it must be mapped.
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
		 * @brief Checks the checksums of all archives.
		 *
		 * @param errorOut Receives one line per damaged asset (optional, otherwise they are collected for the thread).
		 * @return true if all assets are valid.
		 */
		bool VerifyAll(std::string* errorOut = nullptr);

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
		struct Impl;
		std::unique_ptr<Impl> m_impl;

		/**
		 * @brief Writes the error to errorOut, or collects it for the calling thread.
		 */
		void Report(const std::string& error, std::string* errorOut) const;

		/**
		 * @brief Finds the archive and entry of a key, reports an error if it is missing or the archive was loaded again.
		 */
		bool FindEntry(const std::string& key, const std::string& action, std::string* errorOut,
			StaticForgeConcurrentArchive** archiveOut, const Internal::StaticForgeIndexEntry** entryOut) const;
	};

}
