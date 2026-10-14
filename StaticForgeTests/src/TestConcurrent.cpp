#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

static fs::path BuildConcurrentArchive(const TempDir& dir, const std::vector<Asset>& assets, const std::string& name) {
	WriteAssets(dir / ("src_" + name), assets);
	StaticForgeBuilder builder{ name, { dir / ("src_" + name) }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(true).SetStoreNames(true).SetCompressionBlockSize(8192);
	if (!builder.Build())
		throw std::runtime_error("build failed: " + builder.GetError());
	return dir / "out" / (name + ".sfpak");
}

static size_t CountLines(const std::string& text) {
	if (text.empty())
		return 0;
	size_t lines = 1;
	for (char c : text) lines += c == '\n';
	return lines;
}

static size_t DefaultPoolSize() {
	const size_t hardware = std::thread::hardware_concurrency();
	return hardware != 0 ? hardware : FALLBACK_POOL_SIZE;
}

// loads random assets with all functions from several threads, returns the number of wrong results
static int LoadInParallel(StaticForgeConcurrentArchive& archive, const std::vector<Asset>& assets, int threadCount, int loadsPerThread) {
	std::atomic<int> wrong{ 0 };
	std::vector<std::thread> threads;
	for (int t = 0; t < threadCount; t++) {
		threads.emplace_back([&, t]() {
			std::mt19937 rng(static_cast<uint32_t>(t) * 7 + 1);
			std::vector<std::byte> data;
			for (int i = 0; i < loadsPerThread; i++) {
				const Asset& asset = assets[rng() % assets.size()];
				const size_t size = asset.data.size();
				bool ok = true;
				switch (rng() % 5) {
				case 0: ok = archive.LoadAsset(asset.key, &data) && Same(data, asset.data); break;
				case 1: ok = archive.LoadAssetMapped(asset.key, &data) && Same(data, asset.data); break;
				case 2: {
					std::vector<std::byte> buffer(size);
					ok = archive.LoadAsset(asset.key, buffer.data(), size) && Same(buffer.data(), size, asset.data);
					break;
				}
				default: {
					if (size == 0) break;
					const uint64_t offset = rng() % size;
					const uint64_t rangeSize = rng() % (size - offset + 1);
					std::vector<std::byte> part(rangeSize);
					const bool mapped = rng() % 2;
					ok = (mapped ? archive.LoadAssetRangeMapped(asset.key, offset, rangeSize, part.data())
						: archive.LoadAssetRange(asset.key, offset, rangeSize, part.data()))
						&& Same(part.data(), rangeSize, asset.data, offset);
					break;
				}
				}
				if (!ok) wrong++;
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	return wrong;
}

TEST_CASE("Concurrent/Pool size") {
	StaticForgeConcurrentArchive archive;
	CHECK_EQ(archive.GetPoolSize(), DefaultPoolSize());
	archive.SetPoolSize(3);
	CHECK_EQ(archive.GetPoolSize(), size_t(3));
	archive.SetPoolSize(DEFAULT_POOL_SIZE);
	CHECK_EQ(archive.GetPoolSize(), DefaultPoolSize());
	CHECK_EQ(archive.GetOpenStreamCount(), size_t(0));
	CHECK_EQ(archive.GetActiveStreamCount(), size_t(0));
}

TEST_CASE("Concurrent/Load from many threads") {
	TempDir dir;
	const auto assets = MakeAssets("", 20);
	const fs::path path = BuildConcurrentArchive(dir, assets, "main");

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	archive.SetPoolSize(3);
	REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());
	CHECK_EQ(archive.GetPoolSize(), size_t(3));
	CHECK_EQ(archive.GetOpenStreamCount(), size_t(1)); // the stream of the reader
	REQUIRE(archive.OpenMapped());

	std::atomic<bool> running{ true };
	std::atomic<size_t> maxOpen{ 0 };
	std::atomic<size_t> maxActive{ 0 };
	std::thread monitor([&]() {
		while (running) {
			maxOpen = std::max<size_t>(maxOpen, archive.GetOpenStreamCount());
			maxActive = std::max<size_t>(maxActive, archive.GetActiveStreamCount());
			std::this_thread::yield();
		}
	});

	const int wrong = LoadInParallel(archive, assets, 12, 150);
	running = false;
	monitor.join();

	CHECK_MSG(wrong == 0, std::to_string(wrong) + " wrong loads: " + archive.GetAllErrors());
	CHECK_MSG(maxOpen <= 3, maxOpen.load());
	CHECK_MSG(maxActive > 1, "streams were not used in parallel");
	CHECK_EQ(archive.GetActiveStreamCount(), size_t(0));
	CHECK(archive.GetAllErrors().empty());
}

TEST_CASE("Concurrent/Pool size can change while loading") {
	TempDir dir;
	const auto assets = MakeAssets("", 21);
	const fs::path path = BuildConcurrentArchive(dir, assets, "main");

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	archive.SetPoolSize(4);
	REQUIRE(reader.Load(path, &archive));
	REQUIRE(archive.OpenMapped());

	std::atomic<bool> running{ true };
	std::thread changer([&]() {
		size_t sizes[] = { 1, 6, 2, 8, 1, 3 };
		for (size_t i = 0; running; i++) {
			archive.SetPoolSize(sizes[i % 6]);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	});

	const int wrong = LoadInParallel(archive, assets, 8, 150);
	running = false;
	changer.join();

	CHECK_MSG(wrong == 0, archive.GetAllErrors());
	CHECK_EQ(archive.GetActiveStreamCount(), size_t(0));
	CHECK(archive.GetOpenStreamCount() <= archive.GetPoolSize());

	archive.CloseFreeStreams();
	CHECK_EQ(archive.GetOpenStreamCount(), size_t(0));
	std::vector<std::byte> data;
	CHECK(archive.LoadAsset(assets[2].key, &data) && Same(data, assets[2].data));
	CHECK_EQ(archive.GetOpenStreamCount(), size_t(1));
}

TEST_CASE("Concurrent/Errors per thread and errorOut") {
	TempDir dir;
	const auto assets = MakeAssets("", 22);
	const fs::path path = BuildConcurrentArchive(dir, assets, "main");

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	REQUIRE(reader.Load(path, &archive));
	std::vector<std::byte> data;

	std::string error;
	CHECK(!archive.LoadAsset("missing.txt", &data, &error));
	CHECK(error.find("missing.txt") != std::string::npos);
	CHECK(archive.IsValid());

	const int threadCount = 8;
	std::vector<std::string> own(threadCount);
	std::vector<std::thread> threads;
	for (int t = 0; t < threadCount; t++) {
		threads.emplace_back([&, t]() {
			std::vector<std::byte> d;
			archive.LoadAsset("missing_" + std::to_string(t) + "_a", &d);
			archive.LoadAsset("missing_" + std::to_string(t) + "_b", &d);
			if (t % 2 == 0)
				own[t] = archive.TakeError();
		});
	}
	for (auto& thread : threads)
		thread.join();

	for (int t = 0; t < threadCount; t += 2) {
		CHECK_MSG(CountLines(own[t]) == 2 && own[t].find("missing_" + std::to_string(t) + "_a") != std::string::npos, own[t]);
		CHECK_MSG(own[t].rfind("[Error]: ", 0) == 0, own[t]);
		for (int o = 0; o < threadCount; o++)
			if (o != t) CHECK_MSG(own[t].find("missing_" + std::to_string(o) + "_") == std::string::npos, own[t]);
	}

	const std::string all = archive.GetAllErrors();
	CHECK_MSG(CountLines(all) == size_t(threadCount), all);
	CHECK(all.find("[Thread ") != std::string::npos);
	CHECK(archive.IsValid()); // the main thread has no errors
	CHECK(!archive.TakeAllErrors().empty());
	CHECK(archive.GetAllErrors().empty());
}

TEST_CASE("Concurrent/VerifyAll") {
	TempDir dir;
	const auto assets = MakeAssets("", 23);
	const fs::path path = BuildConcurrentArchive(dir, assets, "main");

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	REQUIRE(reader.Load(path, &archive));
	std::string error;
	CHECK_MSG(archive.VerifyAll(&error), error);

	// damage three assets that have data
	const fs::path damaged = dir / "out/damaged.sfpak";
	fs::copy_file(path, damaged);
	for (size_t i = 0; i < archive.GetFileCount(); i++) {
		const std::string name = archive.GetName(i);
		if (name == "text/story.txt" || name == "textures/hero.png" || name == "models/level.mesh")
			FlipByte(damaged, archive.GetDataOffset() + archive.GetFileOffset(i) + 10);
	}

	StaticForgeConcurrentArchive broken;
	REQUIRE(reader.Load(damaged, &broken));
	error.clear();
	CHECK(!broken.VerifyAll(&error));
	CHECK_MSG(CountLines(error) == 3, error);
	CHECK(broken.IsValid());
	CHECK(!broken.VerifyAll());
	CHECK_MSG(CountLines(broken.GetError()) == 3, broken.GetError());
}

TEST_CASE("Concurrent/Clones in all directions") {
	TempDir dir;
	const auto assets = MakeAssets("", 24);
	const fs::path path = BuildConcurrentArchive(dir, assets, "main");

	StaticForgeReader reader;
	StaticForgeArchive normal;
	REQUIRE(reader.Load(path, &normal));
	std::vector<std::byte> data;

	StaticForgeConcurrentArchive concurrent = normal.CloneConcurrent(2);
	CHECK_EQ(concurrent.GetPoolSize(), size_t(2));
	CHECK(concurrent.LoadAsset(assets[2].key, &data) && Same(data, assets[2].data));
	CHECK_EQ(normal.CloneConcurrent().GetPoolSize(), DefaultPoolSize());

	StaticForgeConcurrentArchive clone = concurrent.Clone();
	CHECK_EQ(clone.GetPoolSize(), size_t(2));
	CHECK_EQ(clone.GetOpenStreamCount(), size_t(0));
	CHECK(clone.LoadAsset(assets[3].key, &data) && Same(data, assets[3].data));

	StaticForgeArchive back = concurrent.CloneArchive();
	CHECK(back.LoadAsset(assets[4].key, &data) && Same(data, assets[4].data));

	// moving keeps the moved-from object usable
	StaticForgeConcurrentArchive moved = std::move(clone);
	CHECK(moved.LoadAsset(assets[5].key, &data) && Same(data, assets[5].data));
	CHECK_EQ(clone.GetFileCount(), size_t(0));
	CHECK(!clone.LoadAsset(assets[5].key, &data));
	CHECK(!clone.IsValid());
}

TEST_CASE("Concurrent/Reader keeps the pool size") {
	TempDir dir;
	const fs::path a = BuildConcurrentArchive(dir, { { "a.txt", "a" } }, "a");
	const fs::path b = BuildConcurrentArchive(dir, { { "b.txt", "b" } }, "b");

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archive;
	archive.SetPoolSize(5);
	REQUIRE(reader.Load(a, &archive));
	archive.LoadAsset("missing", nullptr);
	REQUIRE(reader.Load(b, &archive));
	CHECK_EQ(archive.GetPoolSize(), size_t(5));
	CHECK(archive.GetAllErrors().empty());
	CHECK(archive.Contains("b.txt") && !archive.Contains("a.txt"));

	CHECK(!reader.Load(dir / "missing.sfpak", &archive));
	CHECK(archive.Contains("b.txt"));
}
