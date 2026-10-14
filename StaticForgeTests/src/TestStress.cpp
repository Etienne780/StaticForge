#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <stdexcept>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "Internal/LZ4Compression.h"
#include "Internal/InternalHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

static fs::path BuildStressArchive(const TempDir& dir, const std::vector<Asset>& assets, const std::string& name,
	bool compress, bool names, uint64_t blockSize = DEFAULT_COMPRESSION_BLOCK_SIZE) {
	WriteAssets(dir / ("src_" + name), assets);
	StaticForgeBuilder builder{ name, { dir / ("src_" + name) }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(compress).SetStoreNames(names).SetCompressionBlockSize(blockSize);
	if (!builder.Build())
		throw std::runtime_error("build failed: " + builder.GetError());
	return dir / "out" / (name + ".sfpak");
}

static void PrintInfo(const std::string& text) {
	std::cout << "    " << text << std::endl;
}

// changes random bytes of a valid archive again and again, loading must never crash,
// and an asset that loads successfully must have its original content
static void FuzzArchive(int iterations, uint32_t seed) {
	TempDir dir;
	std::vector<Asset> assets = {
		{ "text.txt", TextData(9000, 1) },
		{ "random.bin", RandomData(5000, 2) },
		{ "mixed.bin", MixedData(20000, 3) },
		{ "empty.bin", "" },
		{ "small.txt", "small" },
	};
	const fs::path path = BuildStressArchive(dir, assets, "fuzz", true, true, 4096);
	const std::string original = ReadFile(path);

	std::unordered_map<uint64_t, std::string> byHash;
	for (const auto& asset : assets)
		byHash[Internal::HashFilename(asset.key)] = asset.data;

	std::mt19937 rng(seed);
	const fs::path damaged = dir / "damaged.sfpak";
	int loaded = 0;
	int wrongContent = 0;

	for (int i = 0; i < iterations; i++) {
		std::string content = original;
		const int changes = 1 + rng() % 8;
		for (int c = 0; c < changes; c++) {
			// the start of the file (header, index, name table header) is more interesting
			const size_t offset = rng() % 3 == 0 ? rng() % std::min<size_t>(content.size(), 512) : rng() % content.size();
			content[offset] = static_cast<char>(rng() & 0xFF);
		}
		if (rng() % 10 == 0)
			content.resize(rng() % content.size());
		WriteFile(damaged, content);

		StaticForgeReader reader;
		StaticForgeArchive archive;
		if (!reader.Load(damaged, &archive))
			continue;
		loaded++;

		archive.OpenMapped();
		std::vector<std::byte> data;
		for (size_t index = 0; index < archive.GetFileCount(); index++) {
			auto it = byHash.find(archive.GetHashName(index));
			if (archive.LoadAssetAt(index, &data) && it != byHash.end() && !Same(data, it->second))
				wrongContent++;

			const uint64_t size = archive.GetFileSize(index);
			if (size > 0 && size < (1u << 24)) {
				std::vector<std::byte> part(std::min<uint64_t>(size, 300));
				archive.LoadAssetRange(archive.GetName(index), size - part.size(), part.size(), part.data());
				archive.LoadAssetRangeAt(index, 0, part.size(), part.data());
			}
		}
		for (const auto& asset : assets) {
			archive.LoadAssetMapped(asset.key, &data);
			archive.GetAssetMapped(asset.key);
		}
		archive.VerifyAll();
	}

	CHECK_MSG(wrongContent == 0, std::to_string(wrongContent) + " damaged assets were loaded without an error");
	PrintInfo(std::to_string(iterations) + " damaged archives, " + std::to_string(loaded) + " could be opened");
}

// writing a file for every damaged archive is slow on Windows, so the normal run only does a few
TEST_CASE("Fuzz/Damaged archives (short)") {
	FuzzArchive(100, 1);
}

STRESS_TEST("Stress/Damaged archives") {
	FuzzArchive(5000, 2);
}

STRESS_TEST("Stress/Random input for the decompressor") {
	Internal::LZ4Compression compressor(Internal::IsLittleEndian(), !Internal::LZ4_STORE_HEADER);
	std::mt19937 rng(3);
	std::vector<std::byte> out(70000);

	// valid compressed data with random changes
	const std::string text = MixedData(65536, 4);
	const auto valid = compressor.Compress(text.data(), text.size());

	for (int i = 0; i < 200000; i++) {
		std::vector<std::byte> input = valid;
		for (int c = 0; c < 1 + static_cast<int>(rng() % 5); c++)
			input[rng() % input.size()] = static_cast<std::byte>(rng() & 0xFF);
		const size_t length = rng() % 8 == 0 ? rng() % input.size() : input.size();

		std::string error;
		if (compressor.DecompressTo(input.data(), length, out.data(), text.size(), &error))
			continue; // a change in a literal still decompresses
		CHECK_MSG(!error.empty(), "failed without an error");
	}
	CHECK(true);
}

STRESS_TEST("Stress/Many files") {
	TempDir dir;
	std::mt19937 rng(5);
	std::vector<Asset> assets;
	for (int i = 0; i < 5000; i++) {
		const size_t size = rng() % 3 == 0 ? rng() % 20000 : rng() % 600;
		const std::string folder = "dir" + std::to_string(i % 37) + "/sub" + std::to_string(i % 5) + "/";
		assets.push_back({ folder + "file_" + std::to_string(i) + ".dat", i % 2 ? TextData(size, i) : MixedData(size, i) });
	}

	// only the builder is measured, not writing the source files
	WriteAssets(dir / "src_many", assets);
	auto start = std::chrono::steady_clock::now();
	StaticForgeBuilder builder{ "many", { dir / "src_many" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(true).SetStoreNames(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());
	auto buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	const fs::path path = dir / "out/many.sfpak";

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());
	CHECK_EQ(archive.GetFileCount(), assets.size());

	start = std::chrono::steady_clock::now();
	std::vector<std::byte> data;
	int wrong = 0;
	for (const auto& asset : assets)
		if (!archive.LoadAsset(asset.key, &data) || !Same(data, asset.data)) wrong++;
	auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	CHECK_MSG(wrong == 0, std::to_string(wrong) + " wrong");
	CHECK_MSG(archive.VerifyAll(), archive.GetError());
	PrintInfo("5000 files: build " + std::to_string(buildMs) + " ms, load all " + std::to_string(loadMs) + " ms, archive "
		+ std::to_string(fs::file_size(path) / 1024) + " KB");
}

STRESS_TEST("Stress/Large file") {
	TempDir dir;
	const size_t size = 200 * 1024 * 1024 + 12345;
	const std::vector<Asset> assets = { { "large.bin", MixedData(size, 6) } };

	WriteAssets(dir / "src_large", assets);
	for (bool compress : { false, true }) {
		const std::string name = compress ? "large_compressed" : "large_raw";
		auto start = std::chrono::steady_clock::now();
		StaticForgeBuilder builder{ name, { dir / "src_large" }, dir / "out" };
		builder.SetCreateOutputDir(true).SetCompress(compress);
		REQUIRE_MSG(builder.Build(), builder.GetError());
		auto buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		const fs::path path = dir / "out" / (name + ".sfpak");

		StaticForgeReader reader;
		StaticForgeArchive archive;
		REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());

		start = std::chrono::steady_clock::now();
		std::vector<std::byte> data;
		CHECK_MSG(archive.LoadAsset("large.bin", &data) && Same(data, assets[0].data), name + ": " + archive.TakeError());
		auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		data.clear();
		data.shrink_to_fit();

		// ranges near the end only decompress the needed blocks
		std::vector<std::byte> part(100000);
		const uint64_t offset = size - part.size() - 7;
		start = std::chrono::steady_clock::now();
		CHECK_MSG(archive.LoadAssetRange("large.bin", offset, part.size(), part.data()) && Same(part.data(), part.size(), assets[0].data, offset), name);
		auto rangeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

		REQUIRE(archive.OpenMapped());
		CHECK_MSG(archive.LoadAssetRangeMapped("large.bin", offset, part.size(), part.data()) && Same(part.data(), part.size(), assets[0].data, offset), name);
		CHECK_MSG(archive.VerifyAll(), archive.GetError());

		PrintInfo(name + ": build " + std::to_string(buildMs) + " ms, load " + std::to_string(loadMs) + " ms, range at the end "
			+ std::to_string(rangeMs) + " ms, archive " + std::to_string(fs::file_size(path) / (1024 * 1024)) + " MB");
	}
}

STRESS_TEST("Stress/Many threads, small pool") {
	TempDir dir;
	const auto assets = MakeAssets("", 7);
	const fs::path path = BuildStressArchive(dir, assets, "threads", true, true, 4096);

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	archive.SetPoolSize(2);
	REQUIRE(reader.Load(path, &archive));
	REQUIRE(archive.OpenMapped());

	std::atomic<int> wrong{ 0 };
	std::vector<std::thread> threads;
	for (int t = 0; t < 64; t++) {
		threads.emplace_back([&, t]() {
			std::mt19937 rng(static_cast<uint32_t>(t));
			std::vector<std::byte> data;
			for (int i = 0; i < 300; i++) {
				const Asset& asset = assets[rng() % assets.size()];
				const bool ok = rng() % 2 ? archive.LoadAsset(asset.key, &data) : archive.LoadAssetMapped(asset.key, &data);
				if (!ok || !Same(data, asset.data)) wrong++;
				if (i % 50 == 0)
					archive.LoadAsset("missing", &data); // errors from many threads
			}
		});
	}
	for (auto& thread : threads)
		thread.join();

	CHECK_MSG(wrong == 0, std::to_string(wrong.load()) + " wrong");
	CHECK(archive.GetOpenStreamCount() <= 2);
	const std::string errors = archive.TakeAllErrors();
	size_t lines = 1;
	for (char c : errors) lines += c == '\n';
	CHECK_MSG(lines == 64 * 6, lines);
}

STRESS_TEST("Stress/Set with many archives") {
	TempDir dir;
	std::vector<std::vector<Asset>> allAssets;
	std::vector<StaticForgeArchive> archives(100);
	StaticForgeReader reader;

	for (int a = 0; a < 100; a++) {
		allAssets.push_back(MakeAssets("archive" + std::to_string(a) + "/", 100 + a));
		const fs::path path = BuildStressArchive(dir, allAssets.back(), "a" + std::to_string(a), a % 2 == 0, a % 3 == 0);
		REQUIRE(reader.Load(path, &archives[a]));
	}

	StaticForgeArchiveSet set;
	for (auto& archive : archives)
		REQUIRE_MSG(set.Add(&archive), set.GetError());

	std::vector<std::byte> data;
	int wrong = 0;
	for (const auto& assets : allAssets)
		for (const auto& asset : assets)
			if (!set.LoadAsset(asset.key, &data) || !Same(data, asset.data)) wrong++;
	CHECK_MSG(wrong == 0, std::to_string(wrong) + " wrong");
	CHECK_MSG(set.VerifyAll(), set.GetError());

	// removing in the middle rebuilds the lookup
	for (int a = 10; a < 60; a++)
		REQUIRE(set.Remove(&archives[a]));
	CHECK_EQ(set.GetArchiveCount(), size_t(50));
	CHECK(!set.Contains(allAssets[30][1].key));
	CHECK(set.LoadAsset(allAssets[80][2].key, &data) && Same(data, allAssets[80][2].data));
}
