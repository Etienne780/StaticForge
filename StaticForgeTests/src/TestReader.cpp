#include <cstring>
#include <stdexcept>

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace StaticForge;
using namespace SFTest;

namespace fs = std::filesystem;

// a small archive with names and compression, returns its path
static fs::path BuildSmallArchive(const TempDir& dir, const std::vector<Asset>& assets) {
	WriteAssets(dir / "src", assets);
	StaticForgeBuilder builder{ "main", { dir / "src" }, dir / "out" };
	builder.SetCreateOutputDir(true).SetStoreNames(true).SetCompress(true);
	if (!builder.Build())
		throw std::runtime_error("build failed: " + builder.GetError());
	return dir / "out/main.sfpak";
}

static std::vector<Asset> SmallAssets() {
	return {
		{ "a.txt", TextData(10000, 1) },
		{ "b.bin", RandomData(5000, 2) },
		{ "c/c.txt", "c" },
	};
}

static uint64_t ReadU64(const fs::path& path, uint64_t offset) {
	const std::string content = ReadFile(path);
	uint64_t value = 0;
	std::memcpy(&value, content.data() + offset, sizeof(value));
	return value;
}

// copies the archive, lets modify change the copy and checks that loading fails
static void CheckRejected(const fs::path& original, const std::string& name, void (*modify)(const fs::path&)) {
	const fs::path copy = original.parent_path() / (name + ".sfpak");
	fs::copy_file(original, copy, fs::copy_options::overwrite_existing);
	modify(copy);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	const bool loaded = reader.Load(copy, &archive);
	CHECK_MSG(!loaded, name + " was loaded");
	CHECK_MSG(!reader.GetError().empty(), name + " has no error");
	CHECK_MSG(archive.GetFileCount() == 0, name + ": archive is not empty after the failed load");
}

TEST_CASE("Reader/Valid archive") {
	TempDir dir;
	const auto assets = SmallAssets();
	const fs::path path = BuildSmallArchive(dir, assets);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE_MSG(reader.Load(path, &archive), reader.GetError());
	CHECK(reader.IsValid());
	CHECK_EQ(archive.GetFileCount(), assets.size());
	CHECK_EQ(archive.GetPath(), path);
	CHECK(archive.IsFileStreamOpen());
}

TEST_CASE("Reader/Invalid paths") {
	TempDir dir;
	StaticForgeReader reader;
	StaticForgeArchive archive;

	CHECK(!reader.Load(dir / "missing.sfpak", &archive));
	CHECK(!reader.Load(dir / "nothing", &archive));

	WriteFile(dir / "wrong.zip", "x");
	CHECK(!reader.Load(dir / "wrong.zip", &archive));

	fs::create_directories(dir / "folder.sfpak");
	CHECK(!reader.Load(dir / "folder.sfpak", &archive));

	WriteFile(dir / "empty.sfpak", "");
	CHECK(!reader.Load(dir / "empty.sfpak", &archive));

	const fs::path valid = BuildSmallArchive(dir, SmallAssets());
	CHECK(!reader.Load(valid, static_cast<StaticForgeArchive*>(nullptr)));
	CHECK(!reader.Load(valid, static_cast<StaticForgeConcurrentArchive*>(nullptr)));
}

TEST_CASE("Reader/Damaged header is rejected") {
	TempDir dir;
	const fs::path path = BuildSmallArchive(dir, SmallAssets());

	CheckRejected(path, "magic", [](const fs::path& p) { WriteBytes(p, 0, "XXXX", 4); });
	CheckRejected(path, "old version", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_VERSION_OFFSET, 2); });
	CheckRejected(path, "new version", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_VERSION_OFFSET, VERSION + 1); });
	CheckRejected(path, "huge file count", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_FILE_COUNT_OFFSET, 0x0FFFFFFFFFFFFFFFull); });
	CheckRejected(path, "file count + 1", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_FILE_COUNT_OFFSET, ReadU64(p, HEADER_FILE_COUNT_OFFSET) + 1); });
	CheckRejected(path, "index size", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_INDEX_SIZE_OFFSET, 12345); });
	CheckRejected(path, "index offset", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_INDEX_OFFSET_OFFSET, 0xFFFFFFFFFFFFFF00ull); });
	CheckRejected(path, "data offset", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_DATA_OFFSET_OFFSET, 0xFFFFFFFFFFFFFF00ull); });
	CheckRejected(path, "name table offset", [](const fs::path& p) { WriteValue<uint64_t>(p, HEADER_NAME_TABLE_OFFSET, 0xFFFFFFFFFFFFFF00ull); });
}

TEST_CASE("Reader/Damaged index is rejected") {
	TempDir dir;
	const fs::path path = BuildSmallArchive(dir, SmallAssets());

	// the index starts at the index offset of the header
	static uint64_t indexOffset = 0;
	indexOffset = ReadU64(path, HEADER_INDEX_OFFSET_OFFSET);

	CheckRejected(path, "file offset", [](const fs::path& p) { WriteValue<uint64_t>(p, indexOffset + ENTRY_FILE_OFFSET_OFFSET, 0xFFFFFFFFFFFF0000ull); });
	CheckRejected(path, "file offset overflow", [](const fs::path& p) { WriteValue<uint64_t>(p, indexOffset + ENTRY_FILE_OFFSET_OFFSET, 0xFFFFFFFFFFFFFFFFull); });
	CheckRejected(path, "duplicate hash", [](const fs::path& p) {
		WriteValue<uint64_t>(p, indexOffset + INDEX_ENTRY_SIZE + ENTRY_HASH_OFFSET, ReadU64(p, indexOffset + ENTRY_HASH_OFFSET));
	});
	// sets the stored size of every entry far behind the end of the file
	CheckRejected(path, "stored sizes", [](const fs::path& p) {
		for (uint64_t i = 0; i < 3; i++) {
			const uint64_t entry = indexOffset + i * INDEX_ENTRY_SIZE;
			const bool compressed = ReadU64(p, entry + ENTRY_COMPRESSED_SIZE_OFFSET) != 0;
			WriteValue<uint64_t>(p, entry + (compressed ? ENTRY_COMPRESSED_SIZE_OFFSET : ENTRY_FILE_SIZE_OFFSET), 1ull << 40);
		}
	});
}

TEST_CASE("Reader/Damaged name table is rejected") {
	TempDir dir;
	const fs::path path = BuildSmallArchive(dir, SmallAssets());

	static uint64_t nameTable = 0;
	static uint64_t entries = 0;
	nameTable = ReadU64(path, HEADER_NAME_TABLE_OFFSET);
	entries = ReadU64(path, nameTable); // entryOffset

	CheckRejected(path, "entry offset", [](const fs::path& p) { WriteValue<uint64_t>(p, nameTable, 0xFFFFFFFFFFFF0000ull); });
	CheckRejected(path, "string data offset", [](const fs::path& p) { WriteValue<uint64_t>(p, nameTable + 8, 0xFFFFFFFFFFFF0000ull); });
	CheckRejected(path, "string data size", [](const fs::path& p) { WriteValue<uint64_t>(p, nameTable + 16, 0xFFFFFFFFFFFFull); });
	CheckRejected(path, "compressed string data size", [](const fs::path& p) { WriteValue<uint64_t>(p, nameTable + 24, 0xFFFFFFFFFFFFull); });
	// name entry: hash (8), length (4), offset (4)
	CheckRejected(path, "name length", [](const fs::path& p) { WriteValue<uint32_t>(p, entries + 8, 0xFFFFFFF0u); });
	CheckRejected(path, "name offset", [](const fs::path& p) { WriteValue<uint32_t>(p, entries + 12, 0xFFFFFFF0u); });
}

TEST_CASE("Reader/Every truncation is rejected") {
	TempDir dir;
	const fs::path path = BuildSmallArchive(dir, SmallAssets());
	const std::string content = ReadFile(path);
	const fs::path truncated = dir / "truncated.sfpak";

	// every length inside of header and index, then in steps through the data and the name table
	std::vector<size_t> lengths;
	for (size_t length = 0; length < content.size(); length += (length < 300 ? 1 : 211))
		lengths.push_back(length);

	// one copy that gets shorter, writing a new file every time is slow on Windows
	WriteFile(truncated, content);
	size_t tested = 0;
	for (auto it = lengths.rbegin(); it != lengths.rend(); ++it) {
		const size_t length = *it;
		fs::resize_file(truncated, length);
		StaticForgeReader reader;
		StaticForgeArchive archive;
		CHECK_MSG(!reader.Load(truncated, &archive), "length " + std::to_string(length) + " was loaded");
		tested++;
	}
	CHECK(tested > 300);
}

TEST_CASE("Reader/Damaged data is found when loading, not when opening") {
	TempDir dir;
	const auto assets = SmallAssets();
	const fs::path path = BuildSmallArchive(dir, assets);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	const uint64_t offset = archive.GetDataOffset() + archive.GetFileOffset(0) + 3;
	const std::string name = archive.GetName(0);
	archive.CloseFileStream();
	FlipByte(path, offset);

	StaticForgeArchive damaged;
	REQUIRE_MSG(reader.Load(path, &damaged), reader.GetError());
	std::vector<std::byte> data;
	CHECK(!damaged.LoadAsset(name, &data));
	CHECK_MSG(damaged.GetError().find("Checksum mismatch") != std::string::npos, damaged.GetError());
	CHECK(!damaged.VerifyAll());
}

TEST_CASE("Reader/Failed load resets the archive") {
	TempDir dir;
	const auto assets = SmallAssets();
	const fs::path path = BuildSmallArchive(dir, assets);
	WriteFile(dir / "bad.sfpak", "this is not an archive at all, but long enough for a header....");

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	archive.LoadAsset("missing", nullptr);
	CHECK(!archive.IsValid());

	CHECK(!reader.Load(dir / "bad.sfpak", &archive));
	CHECK(!reader.IsValid());
	CHECK_EQ(archive.GetFileCount(), size_t(0));
	CHECK(archive.IsValid());

	// a valid load clears the errors of the reader
	CHECK(reader.Load(path, &archive));
	CHECK(reader.IsValid());
	CHECK_EQ(archive.GetFileCount(), assets.size());
}

TEST_CASE("Reader/Invalid path doesn't touch the archive") {
	TempDir dir;
	const auto assets = SmallAssets();
	const fs::path path = BuildSmallArchive(dir, assets);

	StaticForgeReader reader;
	StaticForgeArchive archive;
	REQUIRE(reader.Load(path, &archive));
	CHECK(!reader.Load(dir / "missing.sfpak", &archive));
	CHECK_EQ(archive.GetFileCount(), assets.size());
}
