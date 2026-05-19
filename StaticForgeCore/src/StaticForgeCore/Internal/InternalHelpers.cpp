#include <sstream>
#include "Internal/InternalHelpers.h"

namespace StaticForge::Internal {

	uint64_t AlignSize(uint64_t fileSize, uint64_t targetAligment) {
		return (fileSize + targetAligment - 1) & ~(targetAligment - 1);
	}

	uint64_t GetHeaderSize() {
		return AlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeHeader)), ALIGNMENT_HEADER);
	}

	uint64_t GetIndexEntrySize() {
		return AlignSize(static_cast<uint64_t>(sizeof(Internal::StaticForgeIndexEntry)), ALIGNMENT_INDEX_ENTRY);
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

		typeIndex = std::min(typeIndex, sizeof(types) - 1);

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

}