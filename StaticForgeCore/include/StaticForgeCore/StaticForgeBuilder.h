#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include "StaticForgeTypes.h"
#include "Internal/ErrorSupport.h"

namespace StaticForge {

	class StaticForgeBuilder : public Internal::ErrorSupport {
	public:
		StaticForgeBuilder();
		StaticForgeBuilder(const std::string& archiveName, const std::vector<StaticForgePath>& sourcePaths = {}, const StaticForgePath& outputPath = StaticForgePath{});
		~StaticForgeBuilder() = default;

		bool Build();
		
		StaticForgeBuilder& SetArchiveName(const std::string& archiveName);
		StaticForgeBuilder& SetSourcePath(const std::vector<StaticForgePath>& paths);
		StaticForgeBuilder& AddSourcePath(const StaticForgePath& path);
		StaticForgeBuilder& SetOutputPath(const StaticForgePath& path);
		StaticForgeBuilder& SetCreateOutputDir(bool value);
		StaticForgeBuilder& SetDebugMode(bool active);
		StaticForgeBuilder& SetStoreName(bool active);

	private:
		struct ArchiveGroup {
			std::string name;
			std::vector<Internal::StaticForgeFileEntry> files;
			std::unordered_map<size_t, std::string> seenHashes;

			uint64_t totalArchiveSize = 0;// gets set will building index and optionally updated will building name table
			uint64_t dataStart = 0;

			uint64_t nameTableStart = 0;
			uint64_t nameStringDataStart = 0;
			uint64_t nameStringDataSize = 0;
		};

		std::string m_archiveName;
		std::vector<StaticForgePath> m_srcPaths;
		StaticForgePath m_outputPath;
		bool m_createOutputDir = false;
		bool m_isDebugActive = false;
		bool m_storeName = false;

		std::unordered_map<std::string, ArchiveGroup> m_archiveGroups;

		bool CheckFilepaths(std::string* errorOut);
		bool ScanFiles(std::string* errorOut);
		bool BuildGroups(std::string* errorOut);
		bool BuildIndex(ArchiveGroup& archive, std::string* errorOut) const;
		bool BuildNameTable(ArchiveGroup& archive, std::string* errorOut) const;

		bool WriteFile(ArchiveGroup& archive, std::string* errorOut) const;
		bool WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteIndex(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		bool WriteNameTable(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteNameTableHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteNameTableData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteNameTableStringData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;


		Internal::StaticForgeMetaData StaticForgeBuilder::ResolveArchive(
			const StaticForgePath& filePath,
			const std::unordered_map<std::string, Internal::StaticForgeMetaData>& dirToArchiveMeta
		);
		bool ExcludedFileExtension(const StaticForgePath& extension, const std::vector<std::string>& extensions);
		static bool IsEnoughSpaceAvailable(const StaticForgePath& path, uint64_t fileSize);

		template<typename T>
		void WriteLE(std::ofstream& stream, T value) const {
			auto le = Internal::SwapEndian(value);
			stream.write(reinterpret_cast<const char*>(&le), sizeof(le));
		};
	};

}