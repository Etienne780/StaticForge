#include "TestFramework.h"
#include "TestHelpers.h"
#include "Internal/StaticForgeMeta.h"

using namespace StaticForge;
using namespace StaticForge::Internal;
using namespace SFTest;

namespace fs = std::filesystem;

// writes a meta file and loads it
static bool LoadMeta(const TempDir& dir, const std::string& content, StaticForgeMeta* meta, const std::string& name = "test.sfpak.meta") {
	const fs::path path = dir / name;
	WriteFile(path, content);
	return meta->Load(path);
}

TEST_CASE("Meta/All parameters and comments") {
	TempDir dir;
	StaticForgeMeta meta;
	const std::string content =
		"# comment line\n"
		"archive = textures; # comment after a statement\n"
		"exclude = .png, .jpg;\n"
		"store-names = true;\n"
		"compress = TRUE;\n"
		"compress-block-size = 16384;\n"
		"compress-table-size = 12;\n"
		"compress-min-match = 6;\n"
		"compress-min-saving = 20;\n";
	REQUIRE_MSG(LoadMeta(dir, content, &meta), meta.GetError());

	const auto& data = meta.GetLoadedMetaData();
	CHECK_EQ(data.archiveName, std::string("textures"));
	CHECK_EQ(data.excludedExtensions.size(), size_t(2));
	CHECK(data.storeNames);
	CHECK(data.compress);
	CHECK(data.compressionBlockSize && *data.compressionBlockSize == 16384);
	CHECK(data.compressionTableSizeLog2 && *data.compressionTableSizeLog2 == 12);
	CHECK(data.compressionMinMatchLength && *data.compressionMinMatchLength == 6);
	CHECK(data.compressionMinSaving && *data.compressionMinSaving == 20);
}

TEST_CASE("Meta/Values that are not set") {
	TempDir dir;
	StaticForgeMeta meta;
	REQUIRE_MSG(LoadMeta(dir, "archive = main;", &meta), meta.GetError());

	const auto& data = meta.GetLoadedMetaData();
	CHECK(!data.storeNames);
	CHECK(!data.compress);
	CHECK(data.excludedExtensions.empty());
	CHECK(!data.compressionBlockSize);
	CHECK(!data.compressionTableSizeLog2);
	CHECK(!data.compressionMinMatchLength);
	CHECK(!data.compressionMinSaving);
}

TEST_CASE("Meta/Exclude is normalized") {
	TempDir dir;
	StaticForgeMeta meta;
	REQUIRE_MSG(LoadMeta(dir, "archive = a; exclude = PNG, .Jpg, tar.gz;", &meta), meta.GetError());

	const auto& ext = meta.GetLoadedMetaData().excludedExtensions;
	REQUIRE_MSG(ext.size() == 3, ext.size());
	CHECK_EQ(ext[0], std::string(".png"));
	CHECK_EQ(ext[1], std::string(".jpg"));
	CHECK_EQ(ext[2], std::string(".tar.gz"));
}

TEST_CASE("Meta/UTF-8 BOM and Unicode names") {
	TempDir dir;
	StaticForgeMeta meta;
	REQUIRE_MSG(LoadMeta(dir, "\xEF\xBB\xBF" u8"archive = テクスチャ;", &meta), meta.GetError());
	CHECK_EQ(meta.GetLoadedMetaData().archiveName, std::string(u8"テクスチャ"));

	StaticForgeMeta meta2;
	REQUIRE_MSG(LoadMeta(dir, u8"archive = main;", &meta2, u8"日本語.sfpak.meta"), meta2.GetError());
}

TEST_CASE("Meta/Syntax errors") {
	TempDir dir;
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "missing semicolon", "archive = main" },
		{ "missing value", "archive = ;" },
		{ "missing assign", "archive main;" },
		{ "unexpected character", "archive = ma@in;" },
		{ "unknown parameter", "archiv = main;" },
		{ "bool not a bool", "archive = a; compress = 5;" },
		{ "uint with unit", "archive = a; compress-block-size = 64k;" },
		{ "uint negative", "archive = a; compress-block-size = -5;" },
		{ "uint not a number", "archive = a; compress-min-saving = many;" },
	};

	for (const auto& [name, content] : cases) {
		StaticForgeMeta meta;
		const bool ok = LoadMeta(dir, content, &meta);
		CHECK_MSG(!ok, name + " was accepted");
		CHECK_MSG(!meta.GetError().empty(), name + " has no error message");
	}
}

TEST_CASE("Meta/Unknown parameter gets a suggestion") {
	TempDir dir;
	StaticForgeMeta meta;
	CHECK(!LoadMeta(dir, "archive = a; compres = true;", &meta));
	const std::string error = meta.GetError();
	CHECK_MSG(error.find("compres") != std::string::npos && error.find("compress") != std::string::npos, error);
}

TEST_CASE("Meta/Invalid files") {
	TempDir dir;
	StaticForgeMeta meta;
	CHECK(!meta.Load(dir / "missing.sfpak.meta"));
	CHECK(!meta.GetError().empty());

	WriteFile(dir / "wrong.meta", "archive = a;");
	StaticForgeMeta meta2;
	CHECK(!meta2.Load(dir / "wrong.meta"));
}

TEST_CASE("Meta/Object can be loaded again") {
	TempDir dir;
	StaticForgeMeta meta;
	CHECK(!LoadMeta(dir, "archive = ;", &meta, "bad.sfpak.meta"));
	REQUIRE_MSG(LoadMeta(dir, "archive = good; compress = true;", &meta, "good.sfpak.meta"), meta.GetError());
	CHECK(meta.IsValid());
	CHECK_EQ(meta.GetLoadedMetaData().archiveName, std::string("good"));
	CHECK(meta.GetLoadedMetaData().compress);
}
