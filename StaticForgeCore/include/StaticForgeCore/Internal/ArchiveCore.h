#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

#include "StaticForgeTypes.h"

namespace StaticForge::Internal {

	class MmapFile;

	/**
	 * @brief Data of a loaded archive.
	 *
	 * Never changed after loading, so archive objects can share it, also between threads.
	 */
	struct ArchiveData {
		StaticForgePath path;
		StaticForgeHeader header;
		std::vector<StaticForgeIndexEntry> indexEntries;
		std::unordered_map<uint64_t, size_t> hashNameToEntry;
		std::unordered_map<size_t, std::string> indexToName;

		/**
		 * @brief Returns the entry of an asset key, or nullptr if it doesn't exist.
		 */
		const StaticForgeIndexEntry* Find(const std::string& key) const;

		/**
		 * @brief Returns the stored name, or "hash <hex>" if no names are stored. For error messages.
		 */
		std::string GetDisplayName(size_t index) const;
	};

	/**
	 * @brief Where the stored bytes are read from: a stream or a mapping (exactly one is set).
	 */
	struct EntrySource {
		std::ifstream* stream = nullptr;
		const MmapFile* mmap = nullptr;
	};

	/**
	 * @brief Loads a whole entry into outVector (resized) or into outBuffer and validates the checksum.
	 *
	 * outVector is only changed on success.
	 */
	bool LoadEntry(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		std::vector<std::byte>* outVector, std::byte* outBuffer, uint64_t bufferSize, std::string* errorOut);

	/**
	 * @brief Loads a part of an entry into outBuffer, without checksum validation.
	 */
	bool LoadEntryRange(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		uint64_t offset, uint64_t size, std::byte* outBuffer, std::string* errorOut);

	/**
	 * @brief Checks the checksum of an entry, the stream is read in chunks of chunkBuffer's size.
	 *
	 * @param reasonOut Receives why the entry is damaged.
	 * @return true if the entry is valid.
	 */
	bool VerifyEntry(const ArchiveData& data, const StaticForgeIndexEntry& entry, std::ifstream& stream,
		std::vector<char>* chunkBuffer, std::string* reasonOut);

}
