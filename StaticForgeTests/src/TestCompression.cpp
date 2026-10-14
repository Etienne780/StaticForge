#include <random>

#include "TestFramework.h"
#include "TestHelpers.h"
#include "Internal/LZ4Compression.h"
#include "Internal/InternalHelpers.h"

using namespace StaticForge;
using namespace StaticForge::Internal;
using namespace SFTest;

static LZ4Compression MakeCompressor(uint32_t tableSizeLog2 = DEFAULT_COMPRESSION_TABLE_SIZE_LOG2,
	uint32_t minMatch = DEFAULT_COMPRESSION_MIN_MATCH_LENGTH) {
	return LZ4Compression(IsLittleEndian(), !LZ4_STORE_HEADER, tableSizeLog2, minMatch);
}

static bool RoundTrip(LZ4Compression& compressor, const std::string& data, std::string* errorOut) {
	auto compressed = compressor.Compress(data.data(), data.size());
	if (!compressor.IsValid()) {
		*errorOut = "compress: " + compressor.GetError();
		return false;
	}

	std::vector<std::byte> out(data.size());
	if (!compressor.DecompressTo(compressed.data(), compressed.size(), out.data(), out.size(), errorOut))
		return false;

	if (!Same(out, data)) {
		*errorOut = "data differs";
		return false;
	}

	auto vec = compressor.Decompress(compressed.data(), compressed.size(), data.size());
	if (!compressor.IsValid() || !Same(vec, data)) {
		*errorOut = "Decompress (vector): " + compressor.GetError();
		return false;
	}
	return true;
}

TEST_CASE("Compression/RoundTrip different data") {
	auto compressor = MakeCompressor();
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "empty", "" },
		{ "1 byte", "x" },
		{ "4 bytes", "abcd" },
		{ "8 bytes", "abcdabcd" },
		{ "9 bytes", "abcdabcda" },
		{ "text", TextData(70000, 1) },
		{ "random", RandomData(70000, 2) },
		{ "mixed", MixedData(70000, 3) },
		{ "zeros", std::string(70000, '\0') },
		{ "long repeat", std::string(200000, 'a') },
	};

	for (const auto& [name, data] : cases) {
		std::string error;
		CHECK_MSG(RoundTrip(compressor, data, &error), name + ": " + error);
	}
}

TEST_CASE("Compression/RoundTrip all settings") {
	const std::string text = TextData(40000, 4);
	const std::string mixed = MixedData(40000, 5);

	for (uint32_t table : { MIN_COMPRESSION_TABLE_SIZE_LOG2, 12u, DEFAULT_COMPRESSION_TABLE_SIZE_LOG2, 20u, MAX_COMPRESSION_TABLE_SIZE_LOG2 }) {
		for (uint32_t minMatch : { MIN_COMPRESSION_MIN_MATCH_LENGTH, 6u, DEFAULT_COMPRESSION_MIN_MATCH_LENGTH, 32u, 1000u, MAX_COMPRESSION_MIN_MATCH_LENGTH }) {
			auto compressor = MakeCompressor(table, minMatch);
			std::string error;
			const std::string setting = "table " + std::to_string(table) + ", min match " + std::to_string(minMatch);
			CHECK_MSG(RoundTrip(compressor, text, &error), setting + " text: " + error);
			CHECK_MSG(RoundTrip(compressor, mixed, &error), setting + " mixed: " + error);
		}
	}
}

TEST_CASE("Compression/Compressor can be reused") {
	auto compressor = MakeCompressor();
	for (uint32_t i = 0; i < 50; i++) {
		std::string error;
		const std::string data = i % 2 ? TextData(5000 + i * 100, i) : MixedData(3000 + i * 50, i);
		CHECK_MSG(RoundTrip(compressor, data, &error), std::to_string(i) + ": " + error);
	}
}

TEST_CASE("Compression/Text gets smaller, settings are clamped") {
	auto compressor = MakeCompressor();
	// lines that repeat with small changes, like typical config or data files
	std::string text;
	for (int i = 0; text.size() < 65536; i++)
		text += "background layer " + std::to_string(i) + ": color=#336699 alpha=0.5 blend=normal\n";
	auto compressed = compressor.Compress(text.data(), text.size());
	CHECK_MSG(compressed.size() < text.size() / 2, compressed.size());

	compressor.SetTableSizeLog2(1000);
	CHECK_EQ(compressor.GetTableSizeLog2(), MAX_COMPRESSION_TABLE_SIZE_LOG2);
	compressor.SetTableSizeLog2(0);
	CHECK_EQ(compressor.GetTableSizeLog2(), MIN_COMPRESSION_TABLE_SIZE_LOG2);
	compressor.SetMinMatchLength(1);
	CHECK_EQ(compressor.GetMinMatchLength(), MIN_COMPRESSION_MIN_MATCH_LENGTH);
}

TEST_CASE("Compression/DecompressTo needs the exact size") {
	auto compressor = MakeCompressor();
	const std::string text = TextData(10000, 7);
	auto compressed = compressor.Compress(text.data(), text.size());

	std::vector<std::byte> smaller(text.size() - 1);
	std::vector<std::byte> larger(text.size() + 1);
	std::string error;
	CHECK(!compressor.DecompressTo(compressed.data(), compressed.size(), smaller.data(), smaller.size(), &error));
	CHECK(!error.empty());
	error.clear();
	CHECK(!compressor.DecompressTo(compressed.data(), compressed.size(), larger.data(), larger.size(), &error));
	CHECK(!error.empty());
	CHECK(!compressor.DecompressTo(nullptr, 10, smaller.data(), smaller.size(), &error));
	CHECK(!compressor.DecompressTo(compressed.data(), compressed.size(), nullptr, 10, &error));

	// errorOut is optional
	CHECK(!compressor.DecompressTo(compressed.data(), compressed.size(), smaller.data(), smaller.size(), nullptr));
}

TEST_CASE("Compression/Truncated streams fail") {
	auto compressor = MakeCompressor();
	const std::string text = MixedData(3000, 8);
	auto compressed = compressor.Compress(text.data(), text.size());
	std::vector<std::byte> out(text.size());

	for (size_t length = 0; length < compressed.size(); length++) {
		std::string error;
		// without the end marker the output is too short, so every prefix must fail
		bool ok = compressor.DecompressTo(compressed.data(), length, out.data(), out.size(), &error);
		if (ok && Same(out, text))
			continue; // the end marker is the last byte, all data can be complete without it
		CHECK_MSG(!ok, "prefix " + std::to_string(length) + " was accepted");
	}
}

TEST_CASE("Compression/Random input never crashes") {
	auto compressor = MakeCompressor();
	std::mt19937 rng(9);
	std::vector<std::byte> out(4096);

	for (int i = 0; i < 2000; i++) {
		std::string input = RandomData(1 + rng() % 300, rng());
		// valid block types make it more likely to get further into the decoder
		for (size_t j = 0; j < input.size(); j += 1 + rng() % 7)
			input[j] = static_cast<char>(rng() % 3);

		std::string error;
		compressor.DecompressTo(reinterpret_cast<const std::byte*>(input.data()), input.size(), out.data(), rng() % out.size(), &error);
	}
	CHECK(true);
}
