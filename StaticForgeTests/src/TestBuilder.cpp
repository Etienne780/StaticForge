#include <algorithm>

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

// loads the archive and checks that every asset has the expected content
static void CheckArchive(const fs::path& path, const std::vector<Asset>& assets, bool storesNames) {
	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());

	CHECK_EQ(archive.GetFileCount(), assets.size());
	CHECK_EQ(archive.StoresNames(), storesNames);
	CHECK_EQ(archive.GetVersion(), VERSION);

	std::vector<std::byte> data;
	for (const auto& asset : assets) {
		CHECK_MSG(archive.LoadAsset(asset.key, &data) && Same(data, asset.data), asset.key + ": " + archive.TakeError());
	}
	CHECK_MSG(archive.VerifyAll(), archive.GetError());

	if (storesNames) {
		size_t found = 0;
		for (size_t i = 0; i < archive.GetFileCount(); i++) {
			for (const auto& asset : assets)
				if (archive.GetName(i) == asset.key) found++;
		}
		CHECK_EQ(found, assets.size());
	}
}

// uncompressed assets can be viewed without copy, compressed ones not
static bool IsStoredCompressed(StaticForgeArchive& archive, const std::string& key) {
	if (!archive.IsMapped() && !archive.OpenMapped())
		return false;
	const bool compressed = archive.GetAssetMapped(key).data == nullptr;
	archive.ClearErrors();
	return compressed;
}

TEST_CASE("Builder/All combinations of names and compression") {
	TempDir dir;
	const auto assets = MakeAssets("", 1);
	WriteAssets(dir / "src", assets);

	for (bool names : { false, true }) {
		for (bool compress : { false, true }) {
			const std::string name = std::string("archive_") + (names ? "n" : "") + (compress ? "c" : "");
			StaticForgeBuilder builder{ name, { dir / "src" }, dir / "out" };
			builder.SetCreateOutputDir(true).SetStoreNames(names).SetCompress(compress);
			REQUIRE_MSG(builder.Build(), builder.GetError());
			CheckArchive(dir / "out" / (name + ".sfpak"), assets, names);
		}
	}
}

TEST_CASE("Builder/Compression block sizes") {
	TempDir dir;
	const auto assets = MakeAssets("", 2);
	WriteAssets(dir / "src", assets);

	for (uint64_t blockSize : { MIN_COMPRESSION_BLOCK_SIZE, uint64_t(4096), uint64_t(10000), DEFAULT_COMPRESSION_BLOCK_SIZE, uint64_t(1024 * 1024) }) {
		const std::string name = "block" + std::to_string(blockSize);
		StaticForgeBuilder builder{ name, { dir / "src" }, dir / "out" };
		builder.SetCreateOutputDir(true).SetCompress(true).SetCompressionBlockSize(blockSize);
		REQUIRE_MSG(builder.Build(), builder.GetError());
		CheckArchive(dir / "out" / (name + ".sfpak"), assets, false);
	}
}

TEST_CASE("Builder/Output and source paths") {
	TempDir dir;
	WriteFile(dir / "src/a.txt", "a");

	StaticForgeBuilder missingOut{ "main", { dir / "src" }, dir / "missing" };
	CHECK(!missingOut.Build());
	CHECK(!missingOut.GetError().empty());
	CHECK(!fs::exists(dir / "missing"));

	StaticForgeBuilder createOut{ "main", { dir / "src" }, dir / "created/sub" };
	createOut.SetCreateOutputDir(true);
	CHECK_MSG(createOut.Build(), createOut.GetError());
	CHECK(fs::exists(dir / "created/sub/main.sfpak"));

	StaticForgeBuilder outIsArchive{ "main", { dir / "src" }, dir / "out.sfpak" };
	outIsArchive.SetCreateOutputDir(true);
	CHECK(!outIsArchive.Build());

	WriteFile(dir / "file.txt", "x");
	StaticForgeBuilder outIsFile{ "main", { dir / "src" }, dir / "file.txt" };
	CHECK(!outIsFile.Build());

	StaticForgeBuilder missingSrc{ "main", { dir / "nothing" }, dir / "created" };
	CHECK(!missingSrc.Build());

	StaticForgeBuilder noSrc{ "main", {}, dir / "created" };
	CHECK(!noSrc.Build());
	CHECK_MSG(noSrc.GetError().find("No source path set") != std::string::npos, noSrc.GetError());

	StaticForgeBuilder noOut{ "main", { dir / "src" } };
	noOut.SetCreateOutputDir(true);
	CHECK(!noOut.Build());
	CHECK_MSG(noOut.GetError().find("No output path set") != std::string::npos, noOut.GetError());
	CHECK(!fs::exists("main.sfpak"));
}

TEST_CASE("Builder/Symbolic links keep their own path") {
	TempDir dir;
	WriteFile(dir / "shared/lib.dll", "library");
	WriteFile(dir / "src/a.txt", "a");
	fs::create_directories(dir / "src/client");
	fs::create_directories(dir / "src/office");

	// creating links needs the developer mode or admin rights on Windows
	std::error_code ec;
	fs::create_symlink(dir / "shared/lib.dll", dir / "src/client/lib.dll", ec);
	if (ec)
		return;
	fs::create_symlink(dir / "shared/lib.dll", dir / "src/office/lib.dll", ec);
	if (ec)
		return;

	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetStoreNames(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());

	StaticForgeArchive archive;
	StaticForgeReader reader;
	REQUIRE_MSG(reader.Load(dir / "out/main.sfpak", &archive), reader.GetError());
	CHECK_EQ(archive.GetFileCount(), size_t(3));

	std::vector<std::byte> data;
	CHECK(archive.LoadAsset("client/lib.dll", &data) && Same(data, "library"));
	CHECK(archive.LoadAsset("office/lib.dll", &data) && Same(data, "library"));
}

TEST_CASE("Builder/Failed build leaves no files behind") {
	TempDir dir;
	WriteFile(dir / "src/audio/audio.sfpak.meta", "archive = audio;");
	WriteFile(dir / "src/audio/music.ogg", RandomData(5000, 1));
	WriteFile(dir / "src/textures/textures.sfpak.meta", "archive = textures;");
	WriteFile(dir / "src/textures/hero.png", RandomData(5000, 2));

	// error while scanning, the output directory is not created
	WriteFile(dir / "bad/bad.sfpak.meta", "archive = bad; compress-min-saving = abc;");
	StaticForgeBuilder scanError{ "main", { dir / "bad" }, dir / "new/sub" };
	scanError.SetCreateOutputDir(true);
	CHECK(!scanError.Build());
	CHECK(!fs::exists(dir / "new"));

	// error while writing (file name too long), the created directories are removed again
	WriteFile(dir / "long/a/a.sfpak.meta", "archive = a;");
	WriteFile(dir / "long/a/a.txt", "a");
	WriteFile(dir / "long/b/b.sfpak.meta", "archive = b" + std::string(300, 'b') + ";");
	WriteFile(dir / "long/b/b.txt", "b");
	StaticForgeBuilder writeError{ "main", { dir / "long" }, dir / "new/sub" };
	writeError.SetCreateOutputDir(true);
	CHECK(!writeError.Build());
	CHECK_MSG(writeError.GetError().find("Failed to create file") != std::string::npos, writeError.GetError());
	CHECK_MSG(!fs::exists(dir / "new"), writeError.GetError());

	// error while writing, the old archives are kept and the temporary files are removed
	StaticForgeBuilder first{ "main", { dir / "src" }, dir / "out" };
	first.SetCreateOutputDir(true);
	CHECK_MSG(first.Build(), first.GetError());
	const std::string oldAudio = ReadFile(dir / "out/audio.sfpak");

	WriteFile(dir / "src/audio/music.ogg", RandomData(9000, 3));
	fs::create_directories(dir / "out/textures.sfpak.tmp");
	StaticForgeBuilder second{ "main", { dir / "src" }, dir / "out" };
	CHECK(!second.Build());
	CHECK_MSG(second.GetError().find("Failed to create file") != std::string::npos, second.GetError());
	CHECK(ReadFile(dir / "out/audio.sfpak") == oldAudio);
	CHECK(!fs::exists(dir / "out/audio.sfpak.tmp"));

	// after the problem is gone the build works and no temporary files are left
	fs::remove(dir / "out/textures.sfpak.tmp");
	CHECK_MSG(second.Build(), second.GetError());
	CHECK(ReadFile(dir / "out/audio.sfpak") != oldAudio);
	for (const auto& entry : fs::directory_iterator(dir / "out"))
		CHECK_MSG(entry.path().extension() == ".sfpak", entry.path().u8string());
}

TEST_CASE("Builder/Archive names") {
	TempDir dir;
	WriteFile(dir / "src/a.txt", "a");

	for (const std::string& invalid : { std::string(""), std::string("."), std::string(".."), std::string("a/b"), std::string("a\\b") }) {
		StaticForgeBuilder builder{ invalid, { dir / "src" }, dir / "out" };
		builder.SetCreateOutputDir(true);
		CHECK_MSG(!builder.Build(), "'" + invalid + "' was accepted");
	}

	// names that only differ in case would be the same file on Windows
	WriteFile(dir / "case/a/a.sfpak.meta", "archive = Audio;");
	WriteFile(dir / "case/a/a.txt", "a");
	WriteFile(dir / "case/b/b.sfpak.meta", "archive = audio;");
	WriteFile(dir / "case/b/b.txt", "b");
	StaticForgeBuilder caseOnly{ "main", { dir / "case" }, dir / "caseOut" };
	caseOnly.SetCreateOutputDir(true);
	CHECK(!caseOnly.Build());
	CHECK_MSG(caseOnly.GetError().find("only differ in case") != std::string::npos, caseOnly.GetError());

	// dots and Unicode are allowed, the extension is appended
	for (const std::string& valid : { std::string("my.archive"), std::string(u8"テクスチャ"), std::string("with space") }) {
		StaticForgeBuilder builder{ valid, { dir / "src" }, dir / "out" };
		builder.SetCreateOutputDir(true);
		CHECK_MSG(builder.Build(), valid + ": " + builder.GetError());
		CHECK_MSG(fs::exists(dir / "out" / fs::u8path(valid + ".sfpak")), valid);
	}
}

TEST_CASE("Builder/Meta files split the archives") {
	TempDir dir;
	WriteFile(dir / "src/textures/textures.sfpak.meta", "archive = textures; store-names = true; compress = true; exclude = psd;");
	WriteFile(dir / "src/textures/hero.png", RandomData(20000, 1));
	WriteFile(dir / "src/textures/theme.txt", TextData(30000, 2));
	WriteFile(dir / "src/textures/source.PSD", "excluded");
	WriteFile(dir / "src/textures/sub/deep.txt", "deep");
	WriteFile(dir / "src/audio/audio.sfpak.meta", "archive = audio;");
	WriteFile(dir / "src/audio/music.ogg", RandomData(10000, 3));
	WriteFile(dir / "src/scripts/main.lua", "print()");

	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());

	CheckArchive(dir / "out/textures.sfpak", {
		{ "textures/hero.png", RandomData(20000, 1) },
		{ "textures/theme.txt", TextData(30000, 2) },
		{ "textures/sub/deep.txt", "deep" },
	}, true);
	CheckArchive(dir / "out/audio.sfpak", { { "audio/music.ogg", RandomData(10000, 3) } }, false);
	CheckArchive(dir / "out/main.sfpak", { { "scripts/main.lua", "print()" } }, false);

	StaticForgeReader reader;
	StaticForgeArchive textures;
	REQUIRE(reader.Load(dir / "out/textures.sfpak", &textures));
	CHECK(!textures.Contains("textures/source.PSD"));
	CHECK(IsStoredCompressed(textures, "textures/theme.txt"));
}

TEST_CASE("Builder/Builder settings override meta files") {
	TempDir dir;
	WriteFile(dir / "src/a.sfpak.meta", "archive = a; store-names = false; compress = false; compress-min-saving = 99;");
	WriteFile(dir / "src/text.txt", TextData(50000, 1));

	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetStoreNames(true).SetCompress(true).SetCompressionMinSaving(5);
	REQUIRE_MSG(builder.Build(), builder.GetError());

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(dir / "out/a.sfpak", &archive));
	CHECK(archive.StoresNames());
	CHECK(IsStoredCompressed(archive, "text.txt"));
}

TEST_CASE("Builder/Compression settings out of range") {
	TempDir dir;
	WriteFile(dir / "src/a.txt", "a");

	struct Case { const char* name; void (*set)(StaticForgeBuilder&, uint64_t); uint64_t min; uint64_t max; const char* metaName; };
	const Case cases[] = {
		{ "block size", [](StaticForgeBuilder& b, uint64_t v) { b.SetCompressionBlockSize(v); }, MIN_COMPRESSION_BLOCK_SIZE, MAX_COMPRESSION_BLOCK_SIZE, "compress-block-size" },
		{ "table size", [](StaticForgeBuilder& b, uint64_t v) { b.SetCompressionTableSizeLog2(v); }, MIN_COMPRESSION_TABLE_SIZE_LOG2, MAX_COMPRESSION_TABLE_SIZE_LOG2, "compress-table-size" },
		{ "min match", [](StaticForgeBuilder& b, uint64_t v) { b.SetCompressionMinMatchLength(v); }, MIN_COMPRESSION_MIN_MATCH_LENGTH, MAX_COMPRESSION_MIN_MATCH_LENGTH, "compress-min-match" },
		{ "min saving", [](StaticForgeBuilder& b, uint64_t v) { b.SetCompressionMinSaving(v); }, MIN_COMPRESSION_MIN_SAVING, MAX_COMPRESSION_MIN_SAVING, "compress-min-saving" },
	};

	for (const auto& c : cases) {
		auto build = [&](uint64_t value) {
			StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
			builder.SetCreateOutputDir(true).SetCompress(true);
			c.set(builder, value);
			return builder.Build();
		};
		CHECK_MSG(build(c.min), std::string(c.name) + " min");
		CHECK_MSG(build(c.max), std::string(c.name) + " max");
		CHECK_MSG(c.max == UINT64_MAX || !build(c.max + 1), std::string(c.name) + " max + 1");
		CHECK_MSG(c.min == 0 || !build(c.min - 1), std::string(c.name) + " min - 1");

		// the same through a meta file
		TempDir metaDir;
		WriteFile(metaDir / "src/x.sfpak.meta", "archive = x; compress = true; " + std::string(c.metaName) + " = " + std::to_string(c.max + 1) + ";");
		WriteFile(metaDir / "src/x.txt", "x");
		StaticForgeBuilder metaBuilder{ "main", { metaDir / "src" }, metaDir / "out" };
		metaBuilder.SetCreateOutputDir(true);
		CHECK_MSG(!metaBuilder.Build(), std::string(c.name) + " in meta file");
		CHECK(metaBuilder.GetError().find("x.sfpak.meta") != std::string::npos);
	}
}

TEST_CASE("Builder/Duplicate paths") {
	TempDir dir;
	// two sources with the same relative path in the same archive
	WriteFile(dir / "src1/textures/hero.png", "1");
	WriteFile(dir / "src2/textures/hero.png", "2");
	StaticForgeBuilder sameArchive{ "main", { dir / "src1", dir / "src2" }, dir / "out" };
	sameArchive.SetCreateOutputDir(true);
	CHECK(!sameArchive.Build());
	CHECK_MSG(sameArchive.GetError().find("Duplicate path 'textures/hero.png'") != std::string::npos, sameArchive.GetError());

	// the same path in two different archives
	WriteFile(dir / "src3/textures/textures.sfpak.meta", "archive = textures;");
	WriteFile(dir / "src3/textures/hero.png", "3");
	StaticForgeBuilder twoArchives{ "main", { dir / "src3", dir / "src2" }, dir / "out" };
	twoArchives.SetCreateOutputDir(true);
	CHECK(!twoArchives.Build());
	const std::string error = twoArchives.GetError();
	CHECK_MSG(error.find("archive 'textures'") != std::string::npos && error.find("archive 'main'") != std::string::npos, error);
}

TEST_CASE("Builder/Empty files and folders") {
	TempDir dir;
	WriteFile(dir / "src/empty.txt", "");
	fs::create_directories(dir / "src/empty folder");

	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(true).SetStoreNames(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());
	CheckArchive(dir / "out/main.sfpak", { { "empty.txt", "" } }, true);
}

TEST_CASE("Builder/Builds are deterministic") {
	TempDir dir;
	WriteAssets(dir / "src", MakeAssets("", 3));

	for (int i = 0; i < 2; i++) {
		StaticForgeBuilder builder{ "main", { dir / "src" }, dir / ("out" + std::to_string(i)) };
		builder.SetCreateOutputDir(true).SetCompress(true).SetStoreNames(true);
		REQUIRE_MSG(builder.Build(), builder.GetError());
	}
	CHECK(ReadFile(dir / "out0/main.sfpak") == ReadFile(dir / "out1/main.sfpak"));
}

TEST_CASE("Builder/Files are sorted by path") {
	TempDir dir;
	for (const char* name : { "b.txt", "a/z.txt", "Z.txt", u8"ä.txt", "a.txt", "10.txt", "9.txt" })
		WriteFile(dir / "src1" / fs::u8path(name), name);
	for (const char* name : { "a/a.txt", "c.txt", "_.txt" })
		WriteFile(dir / "src2" / fs::u8path(name), name);

	StaticForgeBuilder builder{ "main", { dir / "src1", dir / "src2" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetStoreNames(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(dir / "out/main.sfpak", &archive));

	// byte order of UTF-8, the same on every system
	std::vector<std::string> expected = { "10.txt", "9.txt", "Z.txt", "_.txt", "a.txt", "a/a.txt", "a/z.txt", "b.txt", "c.txt", u8"ä.txt" };
	REQUIRE_MSG(archive.GetFileCount() == expected.size(), archive.GetFileCount());
	for (size_t i = 0; i < expected.size(); i++)
		CHECK_MSG(archive.GetName(i) == expected[i], std::to_string(i) + ": " + archive.GetName(i));

	// the offsets follow the order
	for (size_t i = 1; i < archive.GetFileCount(); i++)
		CHECK(archive.GetFileOffset(i) > archive.GetFileOffset(i - 1));
}

TEST_CASE("Builder/Builder can be used again") {
	TempDir dir;
	WriteFile(dir / "src/a.txt", "a");

	StaticForgeBuilder builder{ "main", { dir / "missing" }, dir / "out" };
	builder.SetCreateOutputDir(true);
	CHECK(!builder.Build());
	CHECK(!builder.IsValid());

	builder.SetSourcePath({ dir / "src" });
	CHECK_MSG(builder.Build(), builder.GetError());
	CHECK(builder.IsValid());

	// building again overwrites the archive
	WriteFile(dir / "src/b.txt", "b");
	CHECK_MSG(builder.Build(), builder.GetError());
	CheckArchive(dir / "out/main.sfpak", { { "a.txt", "a" }, { "b.txt", "b" } }, false);
}

TEST_CASE("Builder/Data that doesn't compress is stored raw") {
	TempDir dir;
	WriteFile(dir / "src/random.bin", RandomData(300000, 1));
	WriteFile(dir / "src/text.txt", TextData(300000, 2));
	WriteFile(dir / "src/zeros.bin", std::string(300000, '\0'));
	WriteFile(dir / "src/mixed.bin", TextData(150000, 3) + RandomData(150000, 4) + TextData(100000, 5));

	const std::vector<std::pair<uint64_t, std::vector<bool>>> expected = {
		// random, text, zeros, mixed
		{ 0, { false, true, true, true } },
		{ DEFAULT_COMPRESSION_MIN_SAVING, { false, true, true, true } },
		{ 99, { false, false, true, false } },
	};

	for (const auto& [minSaving, compressed] : expected) {
		const std::string name = "saving" + std::to_string(minSaving);
		StaticForgeBuilder builder{ name, { dir / "src" }, dir / "out" };
		builder.SetCreateOutputDir(true).SetCompress(true).SetCompressionMinSaving(minSaving);
		REQUIRE_MSG(builder.Build(), builder.GetError());

		StaticForgeReader reader;
		StaticForgeArchive archive;
		REQUIRE(reader.Load(dir / "out" / (name + ".sfpak"), &archive));
		const char* keys[] = { "random.bin", "text.txt", "zeros.bin", "mixed.bin" };
		for (size_t i = 0; i < 4; i++)
			CHECK_MSG(IsStoredCompressed(archive, keys[i]) == compressed[i], name + " " + keys[i]);
		CHECK_MSG(archive.VerifyAll(), archive.GetError());
	}
}

TEST_CASE("Builder/No old bytes behind the end of the archive") {
	TempDir dir;
	// random data of exactly 80 * 4096 bytes: the compressed attempt is longer and the file is written again raw
	WriteFile(dir / "src/end.bin", RandomData(80 * 4096, 1));
	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCompress(true);
	REQUIRE_MSG(builder.Build(), builder.GetError());

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(dir / "out/main.sfpak", &archive));
	CHECK_EQ(fs::file_size(dir / "out/main.sfpak"), archive.GetDataOffset() + 80 * 4096);
	CHECK(archive.VerifyAll());
}

// three archives (audio, data, textures) for the build cache tests
static void WriteCacheSources(const TempDir& dir) {
	WriteFile(dir / "src/audio/audio.sfpak.meta", "archive = audio;");
	WriteFile(dir / "src/audio/music.ogg", RandomData(9000, 1));
	WriteFile(dir / "src/data/data.sfpak.meta", "archive = data; compress = true;");
	WriteFile(dir / "src/data/items.json", TextData(20000, 2));
	WriteFile(dir / "src/textures/textures.sfpak.meta", "archive = textures; store-names = true;");
	WriteFile(dir / "src/textures/hero.png", RandomData(5000, 3));
}

struct CacheBuildResult {
	bool success = false;
	size_t built = 0;
	size_t upToDate = 0;
	std::string error;
};

static CacheBuildResult CacheBuild(const TempDir& dir, bool rebuild = false, bool compress = false) {
	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCacheDir(dir / "cache").SetRebuild(rebuild).SetCompress(compress);

	CacheBuildResult result;
	result.success = builder.Build();
	result.built = builder.GetBuiltArchiveCount();
	result.upToDate = builder.GetUpToDateArchiveCount();
	result.error = builder.GetError();
	return result;
}

// moves the modification time, so a file looks changed without changing its content
static void Touch(const fs::path& path) {
	fs::last_write_time(path, fs::last_write_time(path) + std::chrono::hours(1));
}

#define CHECK_CACHE_BUILD(result, expectedBuilt, expectedUpToDate) \
	do { \
		const CacheBuildResult r_ = (result); \
		CHECK_MSG(r_.success, r_.error); \
		CHECK_EQ(r_.built, size_t(expectedBuilt)); \
		CHECK_EQ(r_.upToDate, size_t(expectedUpToDate)); \
	} while (false)

TEST_CASE("BuildCache/Unchanged archives are skipped") {
	TempDir dir;
	WriteCacheSources(dir);

	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);
	const auto modified = fs::last_write_time(dir / "out/audio.sfpak");
	CHECK(fs::exists(dir / "cache/audio.sfcache"));
	CHECK(!fs::exists(dir / "out/audio.sfcache"));

	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 3);
	CHECK(fs::last_write_time(dir / "out/audio.sfpak") == modified);

	// without a cache directory everything is built and no cache file is written
	StaticForgeBuilder noCache{ "main", { dir / "src" }, dir / "out2" };
	noCache.SetCreateOutputDir(true);
	REQUIRE_MSG(noCache.Build(), noCache.GetError());
	CHECK_EQ(noCache.GetBuiltArchiveCount(), size_t(3));
	CHECK(!fs::exists(dir / "out2/audio.sfcache"));
}

TEST_CASE("BuildCache/Only changed archives are built") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	// changed content
	WriteFile(dir / "src/data/items.json", TextData(21000, 4));
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);
	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(dir / "out/data.sfpak", &archive));
	std::vector<std::byte> data;
	CHECK(archive.LoadAsset("data/items.json", &data) && Same(data, TextData(21000, 4)));

	// added and removed file
	WriteFile(dir / "src/textures/ui.png", RandomData(100, 5));
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);
	fs::remove(dir / "src/textures/ui.png");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);

	// changed meta file
	WriteFile(dir / "src/audio/audio.sfpak.meta", "archive = audio; compress = true;");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);

	// changed builder setting, data is already compressed
	CHECK_CACHE_BUILD(CacheBuild(dir, false, true), 1, 2);
	CHECK_CACHE_BUILD(CacheBuild(dir, false, true), 0, 3);
}

TEST_CASE("BuildCache/Changed modification time without changed content") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	Touch(dir / "src/audio/music.ogg");
	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 3);

	// the new time is stored, the content is not compared again
	const std::string cache = ReadFile(dir / "cache/audio.sfcache");
	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 3);
	CHECK(ReadFile(dir / "cache/audio.sfcache") == cache);
}

TEST_CASE("BuildCache/Missing or replaced archive is built") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	fs::remove(dir / "out/audio.sfpak");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);
	CHECK(fs::exists(dir / "out/audio.sfpak"));

	Touch(dir / "out/data.sfpak");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 2);
}

TEST_CASE("BuildCache/Invalid cache files and rebuild") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	WriteFile(dir / "cache/audio.sfcache", "garbage");
	const std::string data = ReadFile(dir / "cache/data.sfcache");
	WriteFile(dir / "cache/data.sfcache", data.substr(0, data.size() / 2));
	fs::remove(dir / "cache/textures.sfcache");
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);
	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 3);

	CHECK_CACHE_BUILD(CacheBuild(dir, true), 3, 0);
}

TEST_CASE("BuildCache/Failed build keeps the old cache") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	// audio changes, textures can't be written
	WriteFile(dir / "src/audio/music.ogg", RandomData(9500, 6));
	WriteFile(dir / "src/textures/hero.png", RandomData(5500, 7));
	fs::create_directories(dir / "out/textures.sfpak.tmp");
	const std::string audioCache = ReadFile(dir / "cache/audio.sfcache");
	CHECK(!CacheBuild(dir).success);
	CHECK(ReadFile(dir / "cache/audio.sfcache") == audioCache);

	fs::remove(dir / "out/textures.sfpak.tmp");
	CHECK_CACHE_BUILD(CacheBuild(dir), 2, 1);
	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 3);
}

TEST_CASE("BuildCache/Archives that are not built anymore are deleted") {
	TempDir dir;
	WriteCacheSources(dir);
	CHECK_CACHE_BUILD(CacheBuild(dir), 3, 0);

	// the audio files go into main now
	fs::remove(dir / "src/audio/audio.sfpak.meta");

	// cache files of another output directory are kept
	StaticForgeBuilder otherOutput{ "main", { dir / "src" }, dir / "out2" };
	otherOutput.SetCreateOutputDir(true).SetCacheDir(dir / "cache");
	REQUIRE_MSG(otherOutput.Build(), otherOutput.GetError());
	CHECK_EQ(otherOutput.GetRemovedArchiveCount(), size_t(0));
	CHECK(fs::exists(dir / "out/audio.sfpak"));
	CHECK(fs::exists(dir / "cache/audio.sfcache"));

	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetCacheDir(dir / "cache");
	REQUIRE_MSG(builder.Build(), builder.GetError());
	CHECK_EQ(builder.GetRemovedArchiveCount(), size_t(1));
	CHECK(!fs::exists(dir / "out/audio.sfpak"));
	CHECK(!fs::exists(dir / "cache/audio.sfcache"));
	CHECK(fs::exists(dir / "out/main.sfpak"));
	CHECK(fs::exists(dir / "out/data.sfpak"));
}

// names of the files in a directory as they are stored
static std::vector<std::string> ListNames(const fs::path& directory) {
	std::vector<std::string> names;
	for (const auto& entry : fs::directory_iterator(directory))
		names.push_back(entry.path().filename().u8string());
	std::sort(names.begin(), names.end());
	return names;
}

TEST_CASE("BuildCache/Archive name changed only in case") {
	TempDir dir;
	WriteFile(dir / "src/a/a.sfpak.meta", "archive = Audio;");
	WriteFile(dir / "src/a/a.txt", "a");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 0);

	WriteFile(dir / "src/a/a.sfpak.meta", "archive = audio;");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 0);
	CHECK(ListNames(dir / "out") == std::vector<std::string>{ "audio.sfpak" });
	CHECK(ListNames(dir / "cache") == std::vector<std::string>{ "audio.sfcache" });

	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 1);
}

TEST_CASE("BuildCache/File names with spaces") {
	TempDir dir;
	WriteFile(dir / "src/my file.txt", "a");
	WriteFile(dir / "src/sub dir/other file.txt", "b");
	CHECK_CACHE_BUILD(CacheBuild(dir), 1, 0);
	CHECK_CACHE_BUILD(CacheBuild(dir), 0, 1);
}
