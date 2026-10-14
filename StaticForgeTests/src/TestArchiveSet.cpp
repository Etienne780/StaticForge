#include <random>
#include <thread>
#include <atomic>
#include <stdexcept>

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

static fs::path BuildSetArchive(const TempDir& dir, const std::vector<Asset>& assets, const std::string& name, bool compress) {
	WriteAssets(dir / ("src_" + name), assets);
	StaticForgeBuilder builder{ name, { dir / ("src_" + name) }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(compress).SetStoreNames(true);
	if (!builder.Build())
		throw std::runtime_error("build failed: " + builder.GetError());
	return dir / "out" / (name + ".sfpak");
}

TEST_CASE("ArchiveSet/Add, find and load") {
	TempDir dir;
	const auto texturesAssets = MakeAssets("textures/", 30);
	const auto audioAssets = MakeAssets("audio/", 31);
	const fs::path texturesPath = BuildSetArchive(dir, texturesAssets, "textures", true);
	const fs::path audioPath = BuildSetArchive(dir, audioAssets, "audio", false);
	const fs::path dupPath = BuildSetArchive(dir, { { "textures/tiny.txt", "dup" }, { "other.txt", "o" } }, "dup", false);

	StaticForgeReader reader;
	StaticForgeArchive textures, audio, dup, notLoaded;
	REQUIRE(reader.Load(texturesPath, &textures));
	REQUIRE(reader.Load(audioPath, &audio));
	REQUIRE(reader.Load(dupPath, &dup));

	StaticForgeArchiveSet set;
	CHECK_MSG(set.Add(&textures) && set.Add(&audio), set.GetError());
	CHECK(!set.Add(nullptr));
	CHECK(!set.Add(&textures));
	CHECK(!set.Add(&notLoaded));
	set.ClearErrors();

	CHECK(!set.Add(&dup));
	const std::string error = set.TakeError();
	CHECK_MSG(error.find("textures/tiny.txt") != std::string::npos && error.find("dup.sfpak") != std::string::npos
		&& error.find("textures.sfpak") != std::string::npos, error);
	CHECK_EQ(set.GetArchiveCount(), size_t(2));
	CHECK(!set.Contains("other.txt"));

	REQUIRE(textures.OpenMapped() && audio.OpenMapped());
	std::vector<std::byte> data;
	for (const auto* list : { &texturesAssets, &audioAssets }) {
		StaticForgeArchive* expected = list == &texturesAssets ? &textures : &audio;
		for (const auto& asset : *list) {
			uint64_t size = 0;
			CHECK_MSG(set.FindArchive(asset.key) == expected, asset.key);
			CHECK_MSG(set.GetAssetSize(asset.key, &size) && size == asset.data.size(), asset.key);
			CHECK_MSG(set.LoadAsset(asset.key, &data) && Same(data, asset.data), asset.key + ": " + set.TakeError());
			CHECK_MSG(set.LoadAssetMapped(asset.key, &data) && Same(data, asset.data), asset.key + ": " + set.TakeError());

			std::vector<std::byte> buffer(asset.data.size());
			CHECK_MSG(set.LoadAsset(asset.key, buffer.data(), buffer.size()) && Same(buffer.data(), buffer.size(), asset.data), asset.key);
			CHECK_MSG(set.LoadAssetMapped(asset.key, buffer.data(), buffer.size()) && Same(buffer.data(), buffer.size(), asset.data), asset.key);

			if (asset.data.size() > 20) {
				std::vector<std::byte> part(10);
				CHECK_MSG(set.LoadAssetRange(asset.key, 7, 10, part.data()) && Same(part.data(), 10, asset.data, 7), asset.key);
				CHECK_MSG(set.LoadAssetRangeMapped(asset.key, 7, 10, part.data()) && Same(part.data(), 10, asset.data, 7), asset.key);
			}
		}
	}

	// raw file in an uncompressed archive can be viewed
	AssetView view = set.GetAssetMapped("audio/textures/hero.png");
	CHECK(view.data != nullptr && Same(view.data, view.size, audioAssets[3].data));
	CHECK(set.GetAssetMapped("textures/text/story.txt").data == nullptr); // compressed
	set.ClearErrors();

	CHECK(!set.LoadAsset("missing.txt", &data));
	CHECK(set.TakeError().find("not found in any archive") != std::string::npos);
	CHECK(textures.IsValid() && audio.IsValid());
	CHECK_MSG(set.VerifyAll(), set.GetError());
}

TEST_CASE("ArchiveSet/Remove, Clear and reloaded archives") {
	TempDir dir;
	const fs::path a = BuildSetArchive(dir, { { "a.txt", "a" } }, "a", false);
	const fs::path b = BuildSetArchive(dir, { { "b.txt", "b" } }, "b", false);
	const fs::path c = BuildSetArchive(dir, { { "c.txt", "c" } }, "c", false);

	StaticForgeReader reader;
	StaticForgeArchive archiveA, archiveB;
	REQUIRE(reader.Load(a, &archiveA) && reader.Load(b, &archiveB));

	StaticForgeArchiveSet set;
	REQUIRE(set.Add(&archiveA) && set.Add(&archiveB));
	std::vector<std::byte> data;

	// loaded again after it was added
	REQUIRE(reader.Load(c, &archiveB));
	CHECK(!set.LoadAsset("b.txt", &data));
	CHECK(set.TakeError().find("loaded again") != std::string::npos);
	CHECK(set.LoadAsset("a.txt", &data) && Same(data, "a"));

	CHECK(set.Remove(&archiveB));
	CHECK(!set.Remove(&archiveB));
	CHECK(set.Add(&archiveB));
	CHECK(set.LoadAsset("c.txt", &data) && Same(data, "c"));
	CHECK(!set.Contains("b.txt"));

	set.Clear();
	CHECK_EQ(set.GetArchiveCount(), size_t(0));
	CHECK(!set.Contains("a.txt"));
}

TEST_CASE("ArchiveSet/Destroyed before the archives") {
	TempDir dir;
	const fs::path a = BuildSetArchive(dir, { { "a.txt", "a" } }, "a", false);

	struct Assets {
		StaticForgeArchive archive;
		StaticForgeArchiveSet set; // declared last, so it is destroyed first
	};

	StaticForgeReader reader;
	Assets assets;
	REQUIRE(reader.Load(a, &assets.archive));
	REQUIRE(assets.set.Add(&assets.archive));
	std::vector<std::byte> data;
	CHECK(assets.set.LoadAsset("a.txt", &data) && Same(data, "a"));
}

TEST_CASE("ArchiveSet/VerifyAll names the archive") {
	TempDir dir;
	const fs::path a = BuildSetArchive(dir, { { "a.txt", TextData(9000, 1) } }, "a", false);
	const fs::path b = BuildSetArchive(dir, { { "b.txt", TextData(9000, 2) } }, "b", false);

	StaticForgeReader reader;
	StaticForgeArchive archiveB;
	REQUIRE(reader.Load(b, &archiveB));
	const uint64_t offset = archiveB.GetDataOffset() + archiveB.GetFileOffset(0) + 5;
	archiveB.CloseFileStream();
	FlipByte(b, offset);

	StaticForgeArchive archiveA;
	REQUIRE(reader.Load(a, &archiveA));
	REQUIRE(reader.Load(b, &archiveB));
	StaticForgeArchiveSet set;
	REQUIRE(set.Add(&archiveA) && set.Add(&archiveB));
	CHECK(!set.VerifyAll());
	const std::string error = set.GetError();
	CHECK_MSG(error.find("b.sfpak") != std::string::npos && error.find("b.txt") != std::string::npos && error.find("a.txt") == std::string::npos, error);
}

TEST_CASE("ConcurrentArchiveSet/Load from many threads") {
	TempDir dir;
	const auto first = MakeAssets("first/", 40);
	const auto second = MakeAssets("second/", 41);
	const fs::path a = BuildSetArchive(dir, first, "first", true);
	const fs::path b = BuildSetArchive(dir, second, "second", false);

	StaticForgeReader reader;
	StaticForgeConcurrentArchive archiveA, archiveB, dup;
	archiveA.SetPoolSize(2);
	archiveB.SetPoolSize(2);
	REQUIRE(reader.Load(a, &archiveA) && reader.Load(b, &archiveB));
	REQUIRE(reader.Load(a, &dup));
	REQUIRE(archiveB.OpenMapped());

	StaticForgeConcurrentArchiveSet set;
	REQUIRE(set.Add(&archiveA) && set.Add(&archiveB));
	std::string error;
	CHECK(!set.Add(&dup, &error) && !error.empty() && set.IsValid());

	std::vector<Asset> all = first;
	all.insert(all.end(), second.begin(), second.end());

	const int threadCount = 8;
	std::atomic<int> wrong{ 0 };
	std::vector<std::string> own(threadCount);
	std::vector<std::thread> threads;
	for (int t = 0; t < threadCount; t++) {
		threads.emplace_back([&, t]() {
			std::mt19937 rng(static_cast<uint32_t>(t) + 3);
			std::vector<std::byte> data;
			for (int i = 0; i < 150; i++) {
				const Asset& asset = all[rng() % all.size()];
				const bool mapped = asset.key.rfind("second/", 0) == 0 && rng() % 2;
				const bool ok = mapped ? set.LoadAssetMapped(asset.key, &data) : set.LoadAsset(asset.key, &data);
				if (!ok || !Same(data, asset.data)) wrong++;
			}
			std::string e;
			if (set.LoadAsset("errorOut_" + std::to_string(t), &data, &e) || e.empty()) wrong++;
			set.LoadAsset("collected_" + std::to_string(t), &data);
			own[t] = set.TakeError();
		});
	}
	for (auto& thread : threads)
		thread.join();

	CHECK_MSG(wrong == 0, std::to_string(wrong.load()) + " wrong");
	for (int t = 0; t < threadCount; t++) {
		CHECK_MSG(own[t].find("collected_" + std::to_string(t)) != std::string::npos, own[t]);
		CHECK_MSG(own[t].find("errorOut_") == std::string::npos, own[t]);
	}
	CHECK(set.GetAllErrors().empty());
	CHECK(archiveA.GetAllErrors().empty() && archiveB.GetAllErrors().empty());
	CHECK_MSG(set.VerifyAll(&error), error);
}
