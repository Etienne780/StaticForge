#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

#include "StaticForgeTypes.h"

namespace StaticForge::Internal {

	constexpr uint64_t BUILD_CACHE_VERSION = 1;
	constexpr std::string_view BUILD_CACHE_EXTENSION = ".sfcache";

	struct BuildCacheFile {
		uint64_t size = 0;
		int64_t modified = 0;
		uint64_t contentHash = 0;
		std::string path;
	};

	/**
	 * @brief State of an archive at its last build, used to skip archives that didn't change.
	 */
	struct BuildCache {
		uint64_t formatVersion = 0;
		std::string outputPath;
		std::string settings;
		uint64_t archiveSize = 0;
		int64_t archiveModified = 0;
		std::vector<BuildCacheFile> files;
	};

	/**
	 * @brief Reads a cache file, returns false if it is missing or can't be parsed.
	 */
	bool ReadBuildCache(const StaticForgePath& path, BuildCache* cacheOut);

	/**
	 * @brief Writes a cache file, creates the cache directory if needed.
	 */
	bool WriteBuildCache(const StaticForgePath& path, const BuildCache& cache, std::string* errorOut);

	/**
	 * @brief Modification time as raw value of the file clock, only comparable on the same system.
	 */
	bool GetModifiedTime(const StaticForgePath& path, int64_t* timeOut);

	/**
	 * @brief 64-bit FNV-1a hash of the file content.
	 */
	bool HashFileContent(const StaticForgePath& path, uint64_t* hashOut);

}
