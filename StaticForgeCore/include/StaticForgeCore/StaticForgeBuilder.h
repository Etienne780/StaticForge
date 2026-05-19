#pragma once
#include <vector>
#include <unordered_map>
#include <string>
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

	private:
		struct ArchiveGroup {
			std::string name;
			std::vector<Internal::StaticForgeFileEntry> files;
			std::unordered_map<size_t, std::string> seenHashes;

			uint64_t totalArchiveSize = 0;// gets set will building index
			uint64_t dataStart = 0;
		};

		std::string m_archiveName;
		std::vector<StaticForgePath> m_srcPaths;
		StaticForgePath m_outputPath;
		bool m_createOutputDir = false;
		bool m_isDebugActive = false;

		std::unordered_map<std::string, ArchiveGroup> m_archiveGroups;

		uint64_t m_totalArchiveSize = 0;// gets set will building index

		bool CheckFilepaths(std::string* errorOut);
		bool ScanFiles(std::string* errorOut);
		bool BuildGroups(std::string* errorOut);
		bool BuildIndex(ArchiveGroup& archive, std::string* errorOut) const;

		bool WriteFile(ArchiveGroup& archive, std::string* errorOut) const;
		bool WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteIndex(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;
		bool WriteData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;


		std::string StaticForgeBuilder::ResolveArchive(
			const StaticForgePath& filePath,
			const std::unordered_map<std::string, std::string>& dirToArchive
		);
		static bool IsEnoughSpaceAvailable(const StaticForgePath& path, uint64_t fileSize);
	};

}