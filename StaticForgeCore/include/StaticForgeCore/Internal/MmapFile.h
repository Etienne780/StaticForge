#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <filesystem>

namespace StaticForge::Internal {

#if defined(_WIN32)
	using FileHandle = void*;
	using MappingHandle = void*;
// if you have problems with the usings. comment them out and comment windwos.h in
// #	include <windows.h>
#endif

	/**
	 * @class MmapFile
	 * @brief Cross-platform memory-mapped file abstraction.
	 *
	 * Provides a RAII-style wrapper for memory-mapped file access.
	 * On Windows it uses CreateFile / CreateFileMapping / MapViewOfFile,
	 * on Unix-like systems it uses open / mmap.
	 *
	 * The file is mapped read-only into virtual memory, allowing fast
	 * sequential or random access without explicit I/O calls.
	 */
	class MmapFile {
	public:
		using FilePath = std::filesystem::path;

		/**
		 * @brief Constructs an empty, unopened memory-mapped file.
		 */
		MmapFile();

		/**
		 * @brief Destroys the object and unmaps any mapped memory.
		 */
		~MmapFile();

		MmapFile(const MmapFile&) = delete;
		MmapFile& operator=(const MmapFile&) = delete;

		/**
		 * @brief Move constructor.
		 * @param other Source object to move from.
		 */
		MmapFile(MmapFile&&) noexcept;

		/**
		 * @brief Move assignment operator.
		 * @param other Source object to move from.
		 * @return Reference to this object.
		 */
		MmapFile& operator=(MmapFile&&) noexcept;

		/**
		 * @brief Opens and memory-maps a file.
		 *
		 * The file is mapped read-only into memory.
		 *
		 * @param path Path to the file to open.
		 * @param errorOut Optional output string for error messages.
		 * @return true if the file was successfully opened and mapped,
		 *         false otherwise.
		 */
		bool Open(const FilePath& path, std::string* errorOut);

		/**
		 * @brief Closes the file and unmaps memory.
		 */
		void Close();

		/**
		 * @brief Checks whether a file is currently mapped.
		 * @return true if a file is open and mapped.
		 */
		bool IsOpen() const;

		/**
		 * @brief Returns a pointer to the mapped file data.
		 * @return Pointer to the beginning of the mapped memory.
		 */
		const uint8_t* Data() const;

		/**
		 * @brief Returns the mapped data pointer as a typed view.
		 *
		 * @tparam T Target type for reinterpretation.
		 * @return Pointer to data cast to T (no bounds or alignment checks).
		 */
		template<typename T>
		const T* DataAs() const {
			return static_cast<const T*>(m_data);
		}

		/**
		 * @brief Returns the size of the mapped file in bytes.
		 * @return File size in bytes.
		 */
		uint64_t Size() const;

		/**
		 * @brief Returns a pointer to a specific offset in the file.
		 * @param offset Byte offset into the file.
		 * @return Pointer to the requested position, or nullptr if out of range.
		 */
		const uint8_t* At(uint64_t offset) const;

		/**
		 * @brief Checks whether a memory range is valid inside the mapped file.
		 * @param offset Start offset.
		 * @param size Number of bytes.
		 * @return true if the range is fully inside the mapped file.
		 */
		bool InRange(uint64_t offset, uint64_t size) const;

	private:
		const uint8_t* m_data = nullptr;
		uint64_t m_size = 0;

#if defined(_WIN32)
		FileHandle m_file;// INVALID_HANDLE_VALUE
		MappingHandle m_mapping;// INVALID_HANDLE_VALUE
#else
		int m_fd = -1;
#endif
	};

}