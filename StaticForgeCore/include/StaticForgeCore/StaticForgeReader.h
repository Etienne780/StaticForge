#pragma once
#include "StaticForgeTypes.h"
#include "Internal/ErrorSupport.h"

namespace StaticForge {

	class StaticForgeArchive;

	/**
	 * @brief Loads and validates StaticForge archive files.
	 *
	 * Reads archive headers, index tables, and optional filename tables.
	 */
	class StaticForgeReader : public Internal::ErrorSupport {
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
		 * @brief Reads the archive header.
		 *
		 * @param archive Target archive instance.
		 * @param errorOut Receives error details on failure.
		 * @return true if the header was read successfully.
		 */
		bool ReadHeader(StaticForgeArchive* archive, std::string* errorOut) const;

		/**
		 * @brief Reads the archive index table.
		 *
		 * @param archive Target archive instance.
		 * @param errorOut Receives error details on failure.
		 * @return true if the index was read successfully.
		 */
		bool ReadIndex(StaticForgeArchive* archive, std::string* errorOut) const;

		/**
		 * @brief Reads the optional filename table.
		 *
		 * @param archive Target archive instance.
		 * @param errorOut Receives error details on failure.
		 * @return true if the name table was read successfully.
		 */
		bool ReadNameTable(StaticForgeArchive* archive, std::string* errorOut) const;

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