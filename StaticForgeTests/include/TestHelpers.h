#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <filesystem>

#include <StaticForgeCore/StaticForge.h>

namespace SFTest {

	/**
	 * @brief Random bytes, does not compress.
	 */
	std::string RandomData(size_t size, uint32_t seed);

	/**
	 * @brief Text made of a few words, compresses well.
	 */
	std::string TextData(size_t size, uint32_t seed);

	/**
	 * @brief Mostly text with some random bytes, compresses a bit.
	 */
	std::string MixedData(size_t size, uint32_t seed);

	/**
	 * @brief Writes a file, creates the parent folders.
	 */
	void WriteFile(const std::filesystem::path& path, const std::string& content);

	std::string ReadFile(const std::filesystem::path& path);

	/**
	 * @brief Changes one byte of a file (XOR with mask).
	 */
	void FlipByte(const std::filesystem::path& path, uint64_t offset, uint8_t mask = 0x5A);

	/**
	 * @brief Overwrites bytes of a file.
	 */
	void WriteBytes(const std::filesystem::path& path, uint64_t offset, const void* data, size_t size);

	template<typename T>
	void WriteValue(const std::filesystem::path& path, uint64_t offset, T value) {
		WriteBytes(path, offset, &value, sizeof(T));
	}

	bool Same(const std::vector<std::byte>& data, const std::string& expected);
	bool Same(const std::byte* data, size_t size, const std::string& expected, size_t expectedOffset = 0);

	/**
	 * @brief A source file of an archive and its expected content.
	 */
	struct Asset {
		std::string key;
		std::string data;
	};

	/**
	 * @brief Writes the assets into a source folder.
	 */
	void WriteAssets(const std::filesystem::path& sourceDir, const std::vector<Asset>& assets);

	/**
	 * @brief Assets of different kinds and sizes, the keys are prefixed with prefix.
	 */
	std::vector<Asset> MakeAssets(const std::string& prefix, uint32_t seed);

	// offsets of the header fields (the header is packed)
	constexpr uint64_t HEADER_VERSION_OFFSET = 4;
	constexpr uint64_t HEADER_FILE_COUNT_OFFSET = 12;
	constexpr uint64_t HEADER_INDEX_OFFSET_OFFSET = 20;
	constexpr uint64_t HEADER_INDEX_SIZE_OFFSET = 28;
	constexpr uint64_t HEADER_DATA_OFFSET_OFFSET = 36;
	constexpr uint64_t HEADER_NAME_TABLE_OFFSET = 44;

	// offsets inside of an index entry (40 bytes)
	constexpr uint64_t INDEX_ENTRY_SIZE = 40;
	constexpr uint64_t ENTRY_HASH_OFFSET = 0;
	constexpr uint64_t ENTRY_FILE_OFFSET_OFFSET = 8;
	constexpr uint64_t ENTRY_FILE_SIZE_OFFSET = 16;
	constexpr uint64_t ENTRY_COMPRESSED_SIZE_OFFSET = 24;
	constexpr uint64_t ENTRY_CHECKSUM_OFFSET = 36;

}
