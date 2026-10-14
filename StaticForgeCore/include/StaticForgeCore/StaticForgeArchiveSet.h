#pragma once
#include <memory>
#include <vector>
#include <string>

#include "StaticForgeTypes.h"
#include "StaticForgeArchive.h"
#include "ErrorSupport.h"

namespace StaticForge {

	/**
	 * @brief Loads assets from several StaticForgeArchive objects, as if they were one archive.
	 *
	 * The set doesn't own the archives, it only keeps pointers. An archive must not be destroyed while the set uses it,
	 * call Remove() or Clear() first. An asset path may only exist in one of the archives, Add() fails otherwise.
	 *
	 * If an added archive is loaded again with StaticForgeReader::Load, remove it and add it again.
	 *
	 * Not thread-safe, like StaticForgeArchive. Use StaticForgeConcurrentArchiveSet for several threads.
	 */
	class StaticForgeArchiveSet : public ErrorSupport {
	public:
		/**
		 * @brief Creates an empty set.
		 */
		StaticForgeArchiveSet();

		/**
		 * @brief Doesn't touch the archives.
		 */
		~StaticForgeArchiveSet();

		StaticForgeArchiveSet(const StaticForgeArchiveSet&) = delete;
		StaticForgeArchiveSet& operator=(const StaticForgeArchiveSet&) = delete;

		StaticForgeArchiveSet(StaticForgeArchiveSet&&) noexcept;
		StaticForgeArchiveSet& operator=(StaticForgeArchiveSet&&) noexcept;

		/**
		 * @brief Adds a loaded archive.
		 *
		 * Fails if the archive is null, not loaded, already added, or contains an asset path
		 * that an added archive has too. Nothing is added on failure.
		 *
		 * @return true if the archive was added.
		 */
		bool Add(StaticForgeArchive* archive);

		/**
		 * @brief Removes an archive.
		 *
		 * @return true if the archive was part of the set.
		 */
		bool Remove(StaticForgeArchive* archive);

		/**
		 * @brief Removes all archives.
		 */
		void Clear();

		/**
		 * @brief Returns the number of added archives.
		 */
		size_t GetArchiveCount() const;

		/**
		 * @brief Returns the archive that contains an asset, or nullptr.
		 */
		StaticForgeArchive* FindArchive(const std::string& key) const;

		/**
		 * @brief Checks whether one of the archives contains an asset.
		 */
		bool Contains(const std::string& key) const;

		/**
		 * @brief Returns the original (uncompressed) size of an asset without loading it.
		 *
		 * @return true if the asset exists.
		 */
		bool GetAssetSize(const std::string& key, uint64_t* outSize) const;

		/**
		 * @brief Loads an asset, see StaticForgeArchive::LoadAsset().
		 */
		bool LoadAsset(const std::string& key, std::vector<std::byte>* outData);

		/**
		 * @brief Loads an asset into an existing buffer, see StaticForgeArchive::LoadAsset().
		 */
		bool LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize);

		/**
		 * @brief Loads a part of an asset without checksum validation, see StaticForgeArchive::LoadAssetRange().
		 */
		bool LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer);

		/**
		 * @brief Returns a view of an uncompressed asset, the archive that contains it must be mapped.
		 */
		AssetView GetAssetMapped(const std::string& key);

		/**
		 * @brief Loads an asset using memory-mapped access, the archive that contains it must be mapped.
		 */
		bool LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData);

		/**
		 * @brief Loads an asset into an existing buffer using memory-mapped access.
		 */
		bool LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize);

		/**
		 * @brief Loads a part of an asset using memory-mapped access, without checksum validation.
		 */
		bool LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer);

		/**
		 * @brief Checks the checksums of all archives. Adds an error for every damaged asset.
		 *
		 * @return true if all assets are valid.
		 */
		bool VerifyAll();

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;

		/**
		 * @brief Finds the archive and entry of a key, adds an error if it is missing or the archive was loaded again.
		 */
		bool FindEntry(const std::string& key, const std::string& action,
			StaticForgeArchive** archiveOut, const Internal::StaticForgeIndexEntry** entryOut);
	};

}
