#include <sstream>
#include "Internal/InternalHelpers.h"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

namespace StaticForge::Internal {

	uint32_t FNV1a(const void* data, size_t size, uint32_t hash) {
		const uint8_t* bytes = static_cast<const uint8_t*>(data);

		for (size_t i = 0; i < size; i++) {
			hash ^= bytes[i];
			hash *= 16777619u;
		}

		return hash;
	}

	uint64_t AlignSize(uint64_t fileSize, uint64_t targetAligment) {
		return (fileSize + targetAligment - 1) & ~(targetAligment - 1);
	}

	uint64_t GetHeaderSize() {
		return AlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeHeader)), ALIGNMENT_HEADER);
	}

	uint64_t GetIndexEntrySize() {
		return sizeof(StaticForgeIndexEntry);
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