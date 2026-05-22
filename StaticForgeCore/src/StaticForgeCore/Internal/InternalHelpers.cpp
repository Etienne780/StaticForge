#include <sstream>
#include <iomanip>
#include "Internal/InternalHelpers.h"

namespace StaticForge::Internal {

	uint32_t FNV1a(const void* data, size_t size, uint32_t hash) {
		const uint8_t* bytes = static_cast<const uint8_t*>(data);

		for (size_t i = 0; i < size; i++) {
			hash ^= bytes[i];
			hash *= 16777619u;
		}

		return hash;
	}

	std::string NormalizeFilepathSlashes(const std::string& path) {
		std::string result;
		result.reserve(path.size());

		char prev = '\0';

		for (char c : path) {
			if (c == '\\') {
				c = '/';
			}

			if (c == '/' && prev == '/') {
				continue;
			}

			result.push_back(c);
			prev = c;
		}

		return result;
	}

	bool MatchesSuggestion(const std::string input, const std::string& suggestion) {
		constexpr size_t maxOffChars = 3;

		auto normalizeName = [](std::string str) -> std::string {
			std::string out;
			out.reserve(str.size());

			for (char c : str) {
				// Ignore separators
				if (c == '-' || c == '_' || c == ' ')
					continue;

				// Case insensitive compare
				out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}

			return out;
		};

		std::string a = normalizeName(input);
		std::string b = normalizeName(suggestion);

		size_t nameSize = a.size();
		size_t otherSize = b.size();

		size_t maxSize = std::max(nameSize, otherSize);
		size_t minSize = std::min(nameSize, otherSize);

		size_t lengthDiff = maxSize - minSize;

		if (lengthDiff > maxOffChars)
			return false;

		size_t offChars = 0;
		for (size_t i = 0; i < minSize; i++) {
			if (a[i] != b[i])
				offChars++;

			if (offChars > maxOffChars)
				return false;
		}

		if (minSize == 0 || offChars >= minSize - 1)
			return false;

		return true;
	}

	bool IsPowerOfTwoU64(uint64_t value) {
		return value != 0 && (value & (value - 1)) == 0;
	}

	bool SafeAddU64(uint64_t a, uint64_t b, uint64_t* out) {
		if (a > UINT64_MAX - b)
			return false;

		*out = a + b;
		return true;
	}

	bool SafeMulU64(uint64_t a, uint64_t b, uint64_t* out) {
		if (a == 0 || b == 0) {
			*out = 0;
			return true;
		}

		if (a > UINT64_MAX / b)
			return false;

		*out = a * b;
		return true;
	}

	bool SafeAlignSize(uint64_t size, uint64_t alignment, uint64_t* out) {
		if (alignment == 0)
			return false;

		if (!IsPowerOfTwoU64(alignment))
			return false;

		uint64_t tmp;
		if (!SafeAddU64(size, alignment - 1, &tmp))
			return false;

		*out = tmp & ~(alignment - 1);
		return true;
	}

	bool GetHeaderSize(uint64_t* out) {
		return SafeAlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeHeader)), ALIGNMENT_HEADER, out);
	}

	uint64_t HashFilename(const std::string& filename) {
		return FNV1a(filename.data(), filename.size());
	}

	std::string GetFullExtension(const StaticForgePath& path) {
		std::string filename = path.filename().u8string();
		auto pos = filename.find('.');
		if (pos == std::string::npos)
			return "";
		return filename.substr(pos);
	}

	std::string GetFormatedSizeStr(uint64_t bytes) {
		const std::string_view types[] = {
			"Bytes",
			"KB",
			"MB",
			"GB",
			"TB"
		};

		double size = static_cast<double>(bytes);
		size_t typeIndex = 0;

		while (size >= 1024.0 && typeIndex < std::size(types) - 1) {
			size /= 1024.0;
			++typeIndex;
		}

		typeIndex = std::min(typeIndex, std::size(types) - 1);

		std::ostringstream stream;

		if (typeIndex == 0) {
			stream << static_cast<uint64_t>(size) << ' ' << types[typeIndex];
		}
		else {
			stream << std::fixed << std::setprecision(2)
				<< size << ' ' << types[typeIndex];
		}

		return stream.str();
	}

	uint16_t SwapEndian(uint16_t v) {
		return (v >> 8) | (v << 8);
	}

	uint32_t SwapEndian(uint32_t v) {
		return ((v >> 24) & 0xFF) |
			((v >> 8) & 0xFF00) |
			((v << 8) & 0xFF0000) |
			((v << 24) & 0xFF000000);
	}

	uint64_t SwapEndian(uint64_t v) {
		return ((v >> 56) & 0xFFULL) |
			((v >> 40) & 0xFF00ULL) |
			((v >> 24) & 0xFF0000ULL) |
			((v >> 8) & 0xFF000000ULL) |
			((v << 8) & 0xFF00000000ULL) |
			((v << 24) & 0xFF0000000000ULL) |
			((v << 40) & 0xFF000000000000ULL) |
			((v << 56) & 0xFF00000000000000ULL);
	}

	bool IsLittleEndian() noexcept {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
		return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
#elif defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__NT__)
		return true;
#elif defined(__APPLE__) && defined(__MACH__)
		return true;
#else
		static const uint32_t test = 1;
		return (*reinterpret_cast<const uint8_t*>(&test) == 1);
#endif
	}

}