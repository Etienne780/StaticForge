#include <fstream>
#include <random>
#include <cstring>

#include "TestHelpers.h"

namespace fs = std::filesystem;

namespace SFTest {

	std::string RandomData(size_t size, uint32_t seed) {
		std::mt19937 rng(seed);
		std::string data(size, '\0');
		for (auto& c : data)
			c = static_cast<char>(rng() & 0xFF);
		return data;
	}

	std::string TextData(size_t size, uint32_t seed) {
		static const char* words[] = { "alpha ", "beta ", "gamma ", "delta ", "texture ", "model ", "sound ", "level\n" };
		std::mt19937 rng(seed);
		std::string data;
		data.reserve(size + 16);
		while (data.size() < size)
			data += words[rng() % 8];
		data.resize(size);
		return data;
	}

	std::string MixedData(size_t size, uint32_t seed) {
		std::mt19937 rng(seed);
		std::string data = TextData(size, seed);
		for (size_t i = 0; i < data.size(); i += 1 + rng() % 6)
			data[i] = static_cast<char>(rng() & 0xFF);
		return data;
	}

	void WriteFile(const fs::path& path, const std::string& content) {
		fs::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		stream.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	std::string ReadFile(const fs::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	}

	void FlipByte(const fs::path& path, uint64_t offset, uint8_t mask) {
		std::fstream stream(path, std::ios::in | std::ios::out | std::ios::binary);
		stream.seekg(static_cast<std::streamoff>(offset));
		char c = 0;
		stream.get(c);
		stream.seekp(static_cast<std::streamoff>(offset));
		stream.put(static_cast<char>(static_cast<uint8_t>(c) ^ mask));
	}

	void WriteBytes(const fs::path& path, uint64_t offset, const void* data, size_t size) {
		std::fstream stream(path, std::ios::in | std::ios::out | std::ios::binary);
		stream.seekp(static_cast<std::streamoff>(offset));
		stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
	}

	bool Same(const std::vector<std::byte>& data, const std::string& expected) {
		return data.size() == expected.size() && Same(data.data(), data.size(), expected);
	}

	bool Same(const std::byte* data, size_t size, const std::string& expected, size_t expectedOffset) {
		if (expectedOffset > expected.size() || size > expected.size() - expectedOffset)
			return false;
		return size == 0 || std::memcmp(data, expected.data() + expectedOffset, size) == 0;
	}

	void WriteAssets(const fs::path& sourceDir, const std::vector<Asset>& assets) {
		for (const auto& asset : assets)
			WriteFile(sourceDir / fs::u8path(asset.key), asset.data);
	}

	std::vector<Asset> MakeAssets(const std::string& prefix, uint32_t seed) {
		return {
			{ prefix + "empty.bin", "" },
			{ prefix + "tiny.txt", "tiny" },
			{ prefix + "text/story.txt", TextData(100000, seed + 1) },
			{ prefix + "textures/hero.png", RandomData(50000, seed + 2) },
			{ prefix + "models/level.mesh", MixedData(300000, seed + 3) },
			{ prefix + "data/zeros.bin", std::string(200000, '\0') },
			{ prefix + "exact/4096.bin", TextData(4096, seed + 4) },
			{ prefix + "exact/4097.bin", TextData(4097, seed + 5) },
			{ prefix + u8"日本語/テクスチャ.txt", TextData(9000, seed + 6) },
			{ prefix + u8"folder with space/Ünïcödé file.txt", TextData(5000, seed + 7) },
			{ prefix + "a/b/c/d/e/f/deep.txt", "deep" },
		};
	}

}
