#include <random>
#include <stdexcept>

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

static fs::path BuildArchive(const TempDir& dir, const std::vector<Asset>& assets, const std::string& name,
	bool compress, bool names, uint64_t blockSize = DEFAULT_COMPRESSION_BLOCK_SIZE) {
	WriteAssets(dir / ("src_" + name), assets);
	StaticForgeBuilder builder{ name, { dir / ("src_" + name) }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(compress).SetStoreNames(names).SetCompressionBlockSize(blockSize);
	if (!builder.Build())
		throw std::runtime_error("build failed: " + builder.GetError());
	return dir / "out" / (name + ".sfpak");
}

static size_t FindIndex(const StaticForgeArchive& archive, const std::string& key) {
	for (size_t i = 0; i < archive.GetFileCount(); i++)
		if (archive.GetName(i) == key)
			return i;
	return SIZE_MAX;
}

// ranges at the start, the end, around the block borders and random ones
static std::vector<std::pair<uint64_t, uint64_t>> MakeRanges(uint64_t size, uint64_t blockSize, uint32_t seed) {
	std::vector<std::pair<uint64_t, uint64_t>> ranges = { { 0, size }, { 0, 0 }, { size, 0 } };
	if (size > 0) {
		ranges.push_back({ 0, 1 });
		ranges.push_back({ size - 1, 1 });
	}
	for (uint64_t border = blockSize; border < size; border += blockSize) {
		ranges.push_back({ border - 1, std::min<uint64_t>(2, size - border + 1) });
		ranges.push_back({ border, std::min(blockSize, size - border) });
		ranges.push_back({ border - std::min<uint64_t>(10, border), std::min<uint64_t>(blockSize + 20, size - border + 10) });
	}
	std::mt19937 rng(seed);
	for (int i = 0; i < 40 && size > 0; i++) {
		uint64_t offset = rng() % size;
		ranges.push_back({ offset, rng() % (size - offset + 1) });
	}
	return ranges;
}

TEST_CASE("Archive/All load functions return the same data") {
	TempDir dir;
	const auto assets = MakeAssets("", 10);
	const uint64_t blockSize = 8192;

	for (bool compress : { false, true }) {
		const std::string name = compress ? "compressed" : "raw";
		const fs::path path = BuildArchive(dir, assets, name, compress, true, blockSize);

		StaticForgeReader reader;
		StaticForgeArchive archive;
		REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());
		REQUIRE_MSG(archive.OpenMapped(), archive.GetError());

		for (const auto& asset : assets) {
			const std::string what = name + " " + asset.key;
			const size_t index = FindIndex(archive, asset.key);
			REQUIRE_MSG(index != SIZE_MAX, what);
			const size_t size = asset.data.size();

			uint64_t assetSize = 0;
			CHECK_MSG(archive.Contains(asset.key) && archive.GetAssetSize(asset.key, &assetSize) && assetSize == size, what);
			CHECK_EQ(archive.GetFileSize(index), uint64_t(size));

			std::vector<std::byte> data;
			CHECK_MSG(archive.LoadAsset(asset.key, &data) && Same(data, asset.data), what + ": " + archive.TakeError());
			CHECK_MSG(archive.LoadAssetAt(index, &data) && Same(data, asset.data), what + ": " + archive.TakeError());
			CHECK_MSG(archive.LoadAssetMapped(asset.key, &data) && Same(data, asset.data), what + ": " + archive.TakeError());

			// buffers: exact, larger, nothing behind the asset is written
			std::vector<std::byte> buffer(size + 16, std::byte{ 0xEE });
			CHECK_MSG(archive.LoadAsset(asset.key, buffer.data(), size) && Same(buffer.data(), size, asset.data), what);
			CHECK_MSG(archive.LoadAssetAt(index, buffer.data(), buffer.size()) && Same(buffer.data(), size, asset.data), what);
			CHECK_MSG(archive.LoadAssetMapped(asset.key, buffer.data(), buffer.size()) && Same(buffer.data(), size, asset.data), what);
			bool untouched = true;
			for (size_t i = size; i < buffer.size(); i++)
				untouched = untouched && buffer[i] == std::byte{ 0xEE };
			CHECK_MSG(untouched, what);

			// ranges with all three functions
			for (auto [offset, rangeSize] : MakeRanges(size, blockSize, static_cast<uint32_t>(index))) {
				std::vector<std::byte> part(rangeSize + 1, std::byte{ 0xEE });
				const std::string range = what + " range " + std::to_string(offset) + "+" + std::to_string(rangeSize);
				CHECK_MSG(archive.LoadAssetRange(asset.key, offset, rangeSize, part.data()) && Same(part.data(), rangeSize, asset.data, offset), range + ": " + archive.TakeError());
				CHECK_MSG(archive.LoadAssetRangeAt(index, offset, rangeSize, part.data()) && Same(part.data(), rangeSize, asset.data, offset), range);
				CHECK_MSG(archive.LoadAssetRangeMapped(asset.key, offset, rangeSize, part.data()) && Same(part.data(), rangeSize, asset.data, offset), range);
				CHECK_MSG(part[rangeSize] == std::byte{ 0xEE }, range + " wrote behind the range");
			}
		}
		CHECK_MSG(archive.IsValid(), archive.GetError());
	}
}

TEST_CASE("Archive/Keys") {
	TempDir dir;
	const auto assets = MakeAssets("", 11);
	const fs::path path = BuildArchive(dir, assets, "keys", false, false);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	std::vector<std::byte> data;

	CHECK_MSG(archive.LoadAsset("a\\b\\c\\d\\e\\f\\deep.txt", &data) && Same(data, "deep"), archive.TakeError());
	CHECK(archive.LoadAsset(u8"日本語/テクスチャ.txt", &data));
	CHECK(archive.LoadAsset(u8"folder with space/Ünïcödé file.txt", &data));
	CHECK(!archive.Contains("TINY.txt"));
	CHECK(!archive.Contains("/tiny.txt"));
	CHECK(!archive.Contains(""));
	CHECK(!archive.StoresNames());
	CHECK(archive.GetName(0).empty());
}

TEST_CASE("Archive/GetAssetMapped") {
	TempDir dir;
	const std::vector<Asset> assets = {
		{ "text.txt", TextData(50000, 1) },
		{ "random.bin", RandomData(50000, 2) },
		{ "empty.bin", "" },
	};
	const fs::path path = BuildArchive(dir, assets, "mapped", true, true);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));

	CHECK(archive.GetAssetMapped("random.bin").data == nullptr);
	CHECK_MSG(archive.TakeError().find("not mapped") != std::string::npos, "error without mapping");

	REQUIRE(archive.OpenMapped());
	CHECK(archive.OpenMapped()); // opening twice is fine

	AssetView view = archive.GetAssetMapped("random.bin");
	CHECK(view.data != nullptr && Same(view.data, view.size, assets[1].data) && view.size == assets[1].data.size());

	AssetView empty = archive.GetAssetMapped("empty.bin");
	CHECK(empty.data != nullptr && empty.size == 0);

	CHECK(archive.GetAssetMapped("text.txt").data == nullptr);
	CHECK_MSG(archive.TakeError().find("compressed") != std::string::npos, "compressed error");

	archive.CloseMapped();
	CHECK(!archive.IsMapped());
	std::vector<std::byte> data;
	CHECK(!archive.LoadAssetMapped("text.txt", &data));
}

TEST_CASE("Archive/Invalid arguments") {
	TempDir dir;
	const std::vector<Asset> assets = { { "a.txt", TextData(10000, 1) }, { "empty.txt", "" } };
	const fs::path path = BuildArchive(dir, assets, "args", true, false);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	std::vector<std::byte> data(10000);
	std::byte one{};

	CHECK(!archive.LoadAsset("missing", &data));
	CHECK(!archive.LoadAssetAt(99, &data));
	CHECK(!archive.LoadAsset("a.txt", data.data(), 9999)); // buffer too small
	CHECK(!archive.LoadAsset("a.txt", nullptr, 10000));
	CHECK(archive.LoadAsset("empty.txt", nullptr, 0));
	CHECK(!archive.LoadAssetRange("a.txt", 10000, 1, &one));
	CHECK(!archive.LoadAssetRange("a.txt", 1, UINT64_MAX, &one));
	CHECK(!archive.LoadAssetRange("a.txt", UINT64_MAX, 2, &one));
	CHECK(!archive.LoadAssetRange("a.txt", 0, 1, nullptr));
	CHECK(archive.LoadAssetRange("a.txt", 10000, 0, nullptr));
	CHECK(!archive.LoadAssetRangeAt(99, 0, 0, nullptr));
	CHECK(!archive.LoadAssetMapped("a.txt", &data)); // not mapped

	// all errors are collected
	const std::string errors = archive.TakeError();
	size_t lines = 1;
	for (char c : errors) lines += c == '\n';
	CHECK_MSG(lines == 10, errors);
	CHECK(archive.IsValid());

	// invalid index getters return 0
	CHECK_EQ(archive.GetHashName(99), uint64_t(0));
	CHECK_EQ(archive.GetFileOffset(99), uint64_t(0));
	CHECK_EQ(archive.GetFileSize(99), uint64_t(0));
	CHECK_EQ(archive.GetFilePadding(99), uint64_t(0));
	CHECK(archive.GetName(99).empty());
	uint64_t size = 5;
	CHECK(!archive.GetAssetSize("missing", &size) && size == 5);
}

TEST_CASE("Archive/Empty archive object") {
	StaticForgeArchive archive;
	std::vector<std::byte> data;
	CHECK_EQ(archive.GetFileCount(), size_t(0));
	CHECK(archive.GetPath().empty());
	CHECK(!archive.LoadAsset("x", &data));
	CHECK(!archive.OpenMapped());
	CHECK(!archive.OpenFileStream());
	CHECK(archive.VerifyAll() || !archive.IsValid());
	archive.ClearErrors();

	StaticForgeArchive moved = std::move(archive);
	CHECK_EQ(archive.GetFileCount(), size_t(0));
	CHECK(!archive.Contains("x"));
}

TEST_CASE("Archive/File stream is reopened") {
	TempDir dir;
	const std::vector<Asset> assets = { { "a.txt", "content" } };
	const fs::path path = BuildArchive(dir, assets, "stream", false, false);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	archive.CloseFileStream();
	CHECK(!archive.IsFileStreamOpen());

	std::vector<std::byte> data;
	CHECK(archive.LoadAsset("a.txt", &data) && Same(data, "content"));
	CHECK(archive.IsFileStreamOpen());
}

TEST_CASE("Archive/A failed load doesn't break the next one") {
	TempDir dir;
	const std::vector<Asset> assets = { { "a.txt", TextData(8000, 1) }, { "b.txt", TextData(8000, 2) } };
	const fs::path path = BuildArchive(dir, assets, "failed", false, true);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	const size_t index = FindIndex(archive, "a.txt");
	const uint64_t offset = archive.GetDataOffset() + archive.GetFileOffset(index) + 100;
	archive.CloseFileStream();
	FlipByte(path, offset);

	std::vector<std::byte> data;
	CHECK(!archive.LoadAsset("a.txt", &data));
	CHECK(archive.LoadAsset("b.txt", &data) && Same(data, assets[1].data));
	CHECK(!archive.VerifyAll());
	CHECK_MSG(archive.TakeError().find("a.txt") != std::string::npos, "verify names the damaged asset");
}

TEST_CASE("Archive/Clone") {
	TempDir dir;
	const auto assets = MakeAssets("", 12);
	const fs::path path = BuildArchive(dir, assets, "clone", true, true);
	const fs::path other = BuildArchive(dir, { { "other.txt", "other" } }, "other", false, true);

	StaticForgeReader reader;
	StaticForgeArchive original;
	REQUIRE(reader.Load(path, &original));
	REQUIRE(original.OpenMapped());

	StaticForgeArchive clone = original.Clone();
	CHECK_EQ(clone.GetFileCount(), original.GetFileCount());
	CHECK(clone.IsMapped());
	CHECK(!clone.IsFileStreamOpen());

	// errors are separate
	std::vector<std::byte> data;
	clone.LoadAsset("missing", &data);
	CHECK(!clone.IsValid() && original.IsValid());
	clone.ClearErrors();

	// the clone keeps the data and the mapping when the original changes
	original.CloseMapped();
	CHECK(clone.IsMapped());
	REQUIRE(reader.Load(other, &original));
	CHECK_EQ(original.GetFileCount(), size_t(1));
	for (const auto& asset : assets) {
		CHECK_MSG(clone.LoadAsset(asset.key, &data) && Same(data, asset.data), asset.key);
		CHECK_MSG(clone.LoadAssetMapped(asset.key, &data) && Same(data, asset.data), asset.key);
	}

	// clone of a clone outlives its source
	StaticForgeArchive second;
	{
		StaticForgeArchive temp = clone.Clone();
		second = temp.Clone();
	}
	CHECK(second.LoadAsset(assets[2].key, &data) && Same(data, assets[2].data));

	// clone of an empty archive
	StaticForgeArchive empty;
	CHECK_EQ(empty.Clone().GetFileCount(), size_t(0));
}

TEST_CASE("Archive/Reload into the same object") {
	TempDir dir;
	const fs::path a = BuildArchive(dir, { { "a.txt", "a" } }, "a", false, true);
	const fs::path b = BuildArchive(dir, { { "b.txt", "b" }, { "c.txt", "c" } }, "b", true, false);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(a, &archive));
	REQUIRE(archive.OpenMapped());
	archive.LoadAsset("missing", nullptr);

	REQUIRE(reader.Load(b, &archive));
	CHECK(archive.IsValid());
	CHECK(!archive.IsMapped());
	CHECK(!archive.StoresNames());
	CHECK_EQ(archive.GetFileCount(), size_t(2));
	CHECK(!archive.Contains("a.txt"));
	std::vector<std::byte> data;
	CHECK(archive.LoadAsset("c.txt", &data) && Same(data, "c"));
}
