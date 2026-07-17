#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>

#include "StaticForgeTypes.h"
#include "ErrorSupport.h"

namespace StaticForge {

	namespace Internal {
	
		class LZ4Compression;

	}

	/**
	 * @brief Builds StaticForge archive files from directories and files.
	 *
	 * Supports grouping files into archives, generating indices,
	 * optional filename tables, and writing the final package files.
	 */
	class StaticForgeBuilder : public ErrorSupport {
	public:
		/**
		 * @brief Creates an empty builder instance.
		 */
		StaticForgeBuilder();

		/**
		 * @brief Creates a builder with initial configuration.
		 *
		 * @param archiveName Default archive name.
		 * @param sourcePaths Source directories to scan.
		 * @param outputPath Output directory for generated archives.
		 */
		StaticForgeBuilder(
			const std::string& archiveName,
			const std::vector<StaticForgePath>& sourcePaths = {},
			const StaticForgePath& outputPath = StaticForgePath{}
		);

		~StaticForgeBuilder();

		/**
		 * @brief Builds all configured archives.
		 *
		 * @return true if the build succeeded; otherwise false.
		 */
		bool Build();

		/**
		 * @brief Sets the default archive name.
		 *
		 * @param archiveName Archive name to use.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetArchiveName(const std::string& archiveName);

		/**
		 * @brief Replaces all source paths.
		 *
		 * @param paths Source directories to scan.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetSourcePath(const std::vector<StaticForgePath>& paths);

		/**
		 * @brief Adds a source directory.
		 *
		 * @param path Source directory path.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& AddSourcePath(const StaticForgePath& path);

		/**
		 * @brief Sets the output directory.
		 *
		 * @param path Output directory path.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetOutputPath(const StaticForgePath& path);

		/**
		 * @brief Enables or disables automatic output directory creation.
		 *
		 * @param value True to create missing directories.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCreateOutputDir(bool value);

		/**
		 * @brief Enables or disables debug logging.
		 *
		 * @param active True to enable debug output.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetDebugMode(bool active);

		/**
		 * @brief Enables or disables storing original filenames in the archive.
		 *
		 * @param active True to store filenames.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetStoreNames(bool active);

		/**
		 * @brief Enables or disables file compression.
		 *
		 * @param active True to compress files.
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCompress(bool active);

	private:
		/**
		 * @brief Internal archive build data container.
		 */
		struct ArchiveGroup {
			std::string name;
			bool storeNames = false;
			bool compress = false;
			std::vector<Internal::StaticForgeFileEntry> files;
			std::unordered_map<size_t, std::string> seenHashes;

			uint64_t totalArchiveSize = 0;
			uint64_t dataStart = 0;

			uint64_t nameTableStart = 0;
			uint64_t nameStringDataStart = 0;
		};

		std::string m_archiveName = "main";
		std::vector<StaticForgePath> m_srcPaths;
		StaticForgePath m_outputPath;
		bool m_createOutputDir = false;
		bool m_isDebugActive = false;
		bool m_storeNames = false;
		bool m_compress = false;

		std::unordered_map<std::string, ArchiveGroup> m_archiveGroups;
		std::unique_ptr<Internal::LZ4Compression> m_compressor;

		/**
		 * @brief Validates all configured file paths.
		 */
		bool CheckFilepaths(std::string* errorOut);

		/**
		 * @brief Scans source directories and collects files.
		 */
		bool ScanFiles(std::string* errorOut);

		/**
		 * @brief Builds all archive groups and writes them to disk.
		 */
		bool CreateGroups(std::string* errorOut);

		bool CalculateDataStart(ArchiveGroup& archive, std::string* errorOut) const;

		/**
		 * @brief Builds the optional filename table.
		 */
		bool BuildNameTable(ArchiveGroup& archive, std::string* errorOut) const;

		/**
		 * @brief Writes the archive header.
		 */
		bool WriteHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes the archive index entries.
		 */
		bool WriteIndex(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes all file data blocks.
		 */
		bool WriteData(ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes the complete filename table.
		 */
		bool WriteNameTable(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes the filename table header.
		 */
		bool WriteNameTableHeader(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes filename table entries.
		 */
		bool WriteNameTableData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Writes filename string data.
		 */
		bool WriteNameTableStringData(const ArchiveGroup& archive, std::ofstream& stream, std::string* errorOut) const;

		/**
		 * @brief Resolves archive metadata for a file path.
		 *
		 * @param filePath File path to resolve.
		 * @param dirToArchiveMeta Directory metadata map.
		 * @return Resolved archive metadata.
		 */
		Internal::StaticForgeMetaData ResolveArchive(
			const StaticForgePath& filePath,
			const std::unordered_map<std::string, Internal::StaticForgeMetaData>& dirToArchiveMeta
		) const;

		/**
		 * @brief Checks whether a file extension is excluded.
		 *
		 * @param extension File extension to test.
		 * @param extensions List of excluded extensions.
		 * @return true if the extension is excluded.
		 */
		bool ExcludedFileExtension(
			const StaticForgePath& extension,
			const std::vector<std::string>& extensions
		);
	};

}