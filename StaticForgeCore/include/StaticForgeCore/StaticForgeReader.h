#pragma once

#include <fstream>

#include "StaticForgeTypes.h"
#include "ErrorSupport.h"

namespace StaticForge {

	namespace Internal {

		struct ArchiveData;

	}

	class StaticForgeArchive;

	/**
	 * @brief Loads and validates StaticForge archive files.
	 *
	 * Reads archive headers, index tables, and optional filename tables.
	 */
	class StaticForgeReader : public ErrorSupport {
	public:
		/**
		 * @brief Creates a new archive reader instance.
		 */
		StaticForgeReader();

		/**
		 * @brief Default destructor.
		 */
		~StaticForgeReader() = default;

		/**
		 * @brief Loads an archive file into a StaticForgeArchive instance.
		 *
		 * @param path Path to the archive file.
		 * @param archiveOut Target archive object that receives the loaded data.
		 * @return true if the archive was loaded successfully; otherwise false.
		 */
		bool Load(
			const StaticForgePath& path,
			StaticForgeArchive* archiveOut
		);

	private:
		/**
		 * @brief Validates the archive file path.
		 *
		 * @param path Archive file path.
		 * @param errorOut Receives error details on failure.
		 * @return true if the path is valid.
		 */
		bool ValidatePackPath(const StaticForgePath& path, std::string* errorOut);

		/**
		 * @brief Opens the archive file and reads header, index and name table.
		 *
		 * @param path Archive file path.
		 * @param fileSize Size of the archive file.
		 * @param dataOut Receives the archive data.
		 * @param streamOut Receives the opened stream, used afterwards for loading assets.
		 * @return true if the archive was read successfully.
		 */
		bool LoadData(const StaticForgePath& path, uint64_t fileSize, Internal::ArchiveData* dataOut, std::ifstream* streamOut);

		/**
		 * @brief Reads the archive header.
		 *
		 * @param stream Opened archive file.
		 * @param data Target archive data.
		 * @param fileSize Size of the archive file, every offset has to be inside of it.
		 * @param errorOut Receives error details on failure.
		 * @return true if the header was read successfully.
		 */
		bool ReadHeader(std::ifstream& stream, Internal::ArchiveData* data, uint64_t fileSize, std::string* errorOut) const;

		/**
		 * @brief Reads the archive index table.
		 *
		 * @param stream Opened archive file.
		 * @param data Target archive data.
		 * @param fileSize Size of the archive file, every offset has to be inside of it.
		 * @param errorOut Receives error details on failure.
		 * @return true if the index was read successfully.
		 */
		bool ReadIndex(std::ifstream& stream, Internal::ArchiveData* data, uint64_t fileSize, std::string* errorOut) const;

		/**
		 * @brief Reads the optional filename table.
		 *
		 * @param stream Opened archive file.
		 * @param data Target archive data.
		 * @param fileSize Size of the archive file, every offset has to be inside of it.
		 * @param errorOut Receives error details on failure.
		 * @return true if the name table was read successfully.
		 */
		bool ReadNameTable(std::ifstream& stream, Internal::ArchiveData* data, uint64_t fileSize, std::string* errorOut) const;

		/**
		 * @brief Reads a 64-bit little-endian value from a stream.
		 *
		 * @param stream Input stream.
		 * @param out Receives the converted value.
		 * @return true if the value was read successfully.
		 */
		bool ReadLE64(std::ifstream& stream, uint64_t& out) const;

		/**
		 * @brief Reads a 32-bit little-endian value from a stream.
		 *
		 * @param stream Input stream.
		 * @param out Receives the converted value.
		 * @return true if the value was read successfully.
		 */
		bool ReadLE32(std::ifstream& stream, uint32_t& out) const;
	};

}