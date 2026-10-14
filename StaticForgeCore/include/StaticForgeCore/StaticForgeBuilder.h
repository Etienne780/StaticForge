#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>
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

		/**
		 * @brief Sets the size of the blocks a file gets split into for compression.
		 *
		 * Larger blocks compress better, the compression can only reference data inside the same block.
		 * Must be between MIN_COMPRESSION_BLOCK_SIZE and MAX_COMPRESSION_BLOCK_SIZE.
		 *
		 * @param size Block size in bytes (default DEFAULT_COMPRESSION_BLOCK_SIZE).
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCompressionBlockSize(uint64_t size);

		/**
		 * @brief Sets the size of the compressor match table (entries = 1 << sizeLog2).
		 *
		 * Must be between MIN_COMPRESSION_TABLE_SIZE_LOG2 and MAX_COMPRESSION_TABLE_SIZE_LOG2.
		 *
		 * @param sizeLog2 Table size as power of two (default DEFAULT_COMPRESSION_TABLE_SIZE_LOG2).
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCompressionTableSizeLog2(uint64_t sizeLog2);

		/**
		 * @brief Sets the shortest repeated sequence the compressor stores as a reference.
		 *
		 * Must be between MIN_COMPRESSION_MIN_MATCH_LENGTH and MAX_COMPRESSION_MIN_MATCH_LENGTH.
		 *
		 * @param length Length in bytes (default DEFAULT_COMPRESSION_MIN_MATCH_LENGTH).
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCompressionMinMatchLength(uint64_t length);

		/**
		 * @brief Sets how many percent smaller a block or file has to get, otherwise it is stored uncompressed.
		 *
		 * Must be between MIN_COMPRESSION_MIN_SAVING and MAX_COMPRESSION_MIN_SAVING.
		 *
		 * @param percent Minimum saving in percent (default DEFAULT_COMPRESSION_MIN_SAVING).
		 * @return Reference to the builder instance.
		 */
		StaticForgeBuilder& SetCompressionMinSaving(uint64_t percent);

	private:
		/**
		 * @brief Internal archive build data container.
		 */
		struct ArchiveGroup {
			std::string name;
			bool storeNames = false;
			bool compress = false;
			uint64_t compressionBlockSize = DEFAULT_COMPRESSION_BLOCK_SIZE;
			uint32_t compressionTableSizeLog2 = DEFAULT_COMPRESSION_TABLE_SIZE_LOG2;
			uint32_t compressionMinMatchLength = DEFAULT_COMPRESSION_MIN_MATCH_LENGTH;
			uint32_t compressionMinSaving = DEFAULT_COMPRESSION_MIN_SAVING;
			std::vector<Internal::StaticForgeFileEntry> files;

			uint64_t totalArchiveSize = 0;
			uint64_t dataStart = 0;

			uint64_t nameTableStart = 0;
			uint64_t nameStringDataStart = 0;
			uint64_t nameStringDataSize = 0;
			std::vector<std::byte> nameStringDataCompressed;
		};

		std::string m_archiveName = "main";
		std::vector<StaticForgePath> m_srcPaths;
		StaticForgePath m_outputPath;
		bool m_createOutputDir = false;
		bool m_isDebugActive = false;
		bool m_storeNames = false;
		bool m_compress = false;
		// set values override the meta files, otherwise the meta file value or the default is used
		std::optional<uint64_t> m_compressionBlockSize;
		std::optional<uint64_t> m_compressionTableSizeLog2;
		std::optional<uint64_t> m_compressionMinMatchLength;
		std::optional<uint64_t> m_compressionMinSaving;

		std::unordered_map<std::string, ArchiveGroup> m_archiveGroups;
		std::unique_ptr<Internal::LZ4Compression> m_compressor;

		/**
		 * @brief Clears all data of a previous build.
		 */
		void Reset();

		/**
		 * @brief Validates compression settings, values that are not set are skipped.
		 */
		bool CheckCompressionSettings(
			const std::optional<uint64_t>& blockSize,
			const std::optional<uint64_t>& tableSizeLog2,
			const std::optional<uint64_t>& minMatchLength,
			const std::optional<uint64_t>& minSaving,
			std::string* errorOut
		) const;

		/**
		 * @brief Sets the compression settings of an archive (builder value -> meta value -> default).
		 */
		void ApplyCompressionSettings(ArchiveGroup& archive, const Internal::StaticForgeMetaData& meta) const;

		/**
		 * @brief Validates all configured file paths.
		 */
		bool CheckFilepaths(std::string* errorOut);

		/**
		 * @brief Collects all regular files inside a directory (recursive).
		 */
		bool ListFiles(const StaticForgePath& directory, std::vector<StaticForgePath>* outFiles, std::string* errorOut) const;

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
		 * @brief Checks whether a file ends with one of the excluded extensions (case insensitive).
		 *
		 * @param filePath File to test.
		 * @param extensions List of excluded extensions (e.g. ".png" or ".tar.gz").
		 * @return true if the file is excluded.
		 */
		bool ExcludedFileExtension(
			const StaticForgePath& filePath,
			const std::vector<std::string>& extensions
		) const;
	};

}