#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#	include <sys/mman.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <unistd.h>
#else
#	error "StaticForge::MmapFile: unsupported platform"
#endif

#include <cstring>
#include "Internal/MmapFile.h"

namespace StaticForge::Internal {

	MmapFile::MmapFile()
#if defined(_WIN32)
		: m_file(INVALID_HANDLE_VALUE) 
		, m_mapping(INVALID_HANDLE_VALUE) 
#endif
	{
	}

	MmapFile::~MmapFile() {
		Close();
	}

	MmapFile::MmapFile(MmapFile&& other) noexcept
		: m_data(other.m_data)
		, m_size(other.m_size)
#if defined(_WIN32)
		, m_file(other.m_file)
		, m_mapping(other.m_mapping)
#else
		, m_fd(other.m_fd)
#endif
	{
		other.m_data = nullptr;
		other.m_size = 0;
#if defined(_WIN32)
		other.m_file = INVALID_HANDLE_VALUE;
		other.m_mapping = nullptr;
#else
		other.m_fd = -1;
#endif
	}

	MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
		if (this != &other) {
			Close();
			m_data = other.m_data;
			m_size = other.m_size;
#if defined(_WIN32)
			m_file = other.m_file;
			m_mapping = other.m_mapping;
			other.m_file = INVALID_HANDLE_VALUE;
			other.m_mapping = nullptr;
#else
			m_fd = other.m_fd;
			other.m_fd = -1;
#endif
			other.m_data = nullptr;
			other.m_size = 0;
		}
		return *this;
	}

#if defined(_WIN32)
	static std::string WideToUtf8(const std::wstring& wstr) {
		if (wstr.empty())
			return {};

		int sizeNeeded = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			wstr.data(),
			(int)wstr.size(),
			nullptr,
			0,
			nullptr,
			nullptr
		);

		std::string result(sizeNeeded, 0);

		::WideCharToMultiByte(
			CP_UTF8,
			0,
			wstr.data(),
			(int)wstr.size(),
			result.data(),
			sizeNeeded,
			nullptr,
			nullptr
		);

		return result;
	}

	static std::string GetWinErrorMessage(DWORD error) {
		LPWSTR buffer = nullptr;

		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			0,
			(LPWSTR)&buffer,
			0,
			nullptr
		);

		std::wstring msg = buffer ? buffer : L"unknown error";
		LocalFree(buffer);

		return WideToUtf8(msg);
	}

	bool MmapFile::Open(const FilePath& path, std::string* errorOut) {
		// https://learn.microsoft.com/de-de/windows/win32/api/fileapi/nf-fileapi-createfilea#parameters
		// https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw#parameters
		// https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile#parameters

		Close();

		std::wstring pathStr = path.wstring();

		// normalize slashes
		for (auto& c : pathStr) {
			if (c == L'/')
				c = L'\\';
		}

		if (pathStr.rfind(L"\\\\", 0) == 0) {
			pathStr = L"\\\\?\\UNC\\" + pathStr.substr(2);
		}
		else if (pathStr.rfind(L"\\\\?\\", 0) != 0) {
			pathStr = L"\\\\?\\" + pathStr;
		}

		m_file = ::CreateFileW(
			pathStr.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			nullptr
		);

		if (m_file == INVALID_HANDLE_VALUE) {
			DWORD err = ::GetLastError();
			*errorOut = std::string("CreateFileW failed (error ") + std::to_string(err) + "): " + GetWinErrorMessage(err);

			return false;
		}

		LARGE_INTEGER size{};
		if (!::GetFileSizeEx(m_file, &size)) {
			DWORD err = ::GetLastError();
			*errorOut = std::string("GetFileSizeEx failed (error ") + std::to_string(err) + "): " + GetWinErrorMessage(err);

			::CloseHandle(m_file);
			m_file = INVALID_HANDLE_VALUE;
			return false;
		}

		m_size = static_cast<uint64_t>(size.QuadPart);

		if (m_size == 0)
			return true;

		m_mapping = ::CreateFileMappingW(
			m_file,
			nullptr,
			PAGE_READONLY,
			0, 0,
			nullptr
		);

		if (!m_mapping) {
			DWORD err = ::GetLastError();
			*errorOut = std::string("CreateFileMappingW failed (error ") + std::to_string(err) + "): " + GetWinErrorMessage(err);

			::CloseHandle(m_file);
			m_file = INVALID_HANDLE_VALUE;
			m_size = 0;
			return false;
		}

		LPVOID view = ::MapViewOfFile(
			m_mapping,
			FILE_MAP_READ,
			0, 0,
			0
		);

		if (!view) {
			DWORD err = ::GetLastError();
			*errorOut = std::string("MapViewOfFile failed (error ") + std::to_string(err) + "): " + GetWinErrorMessage(err);

			::CloseHandle(m_mapping);
			::CloseHandle(m_file);
			m_mapping = nullptr;
			m_file = INVALID_HANDLE_VALUE;
			m_size = 0;
			return false;
		}

		m_data = static_cast<const uint8_t*>(view);
		return true;
	}

	void MmapFile::Close() {
		if (m_data) {
			::UnmapViewOfFile(m_data);
			m_data = nullptr;
		}

		if (m_mapping) {
			::CloseHandle(m_mapping);
			m_mapping = nullptr;
		}

		if (m_file != INVALID_HANDLE_VALUE) {
			::CloseHandle(m_file);
			m_file = INVALID_HANDLE_VALUE;
		}

		m_size = 0;
	}

	bool MmapFile::IsOpen() const {
		return m_data != nullptr;
	}
#else
	bool MmapFile::Open(const FilePath& path, std::string* errorOut) {
		Close();

		m_fd = ::open(path.c_str(), O_RDONLY);
		if (m_fd == -1) {
			*errorOut = std::string("open failed: ") + ::strerror(errno);
			return false;
		}

		struct stat st {};
		if (::fstat(m_fd, &st) != 0) {
			*errorOut = std::string("fstat failed: ") + ::strerror(errno);
			::close(m_fd);
			m_fd = -1;
			return false;
		}

		m_size = static_cast<uint64_t>(st.st_size);

		if (m_size == 0)
			return true;

		void* ptr = ::mmap(
			nullptr,
			static_cast<size_t>(m_size),
			PROT_READ,
			MAP_PRIVATE,
			m_fd,
			0
		);

		if (ptr == MAP_FAILED) {
			*errorOut = std::string("mmap failed: ") + ::strerror(errno);
			::close(m_fd);
			m_fd = -1;
			m_size = 0;
			return false;
		}

		m_data = static_cast<const uint8_t*>(ptr);
		return true;
	}
	
	void MmapFile::Close() {
		if (m_data) {
			::munmap(const_cast<uint8_t*>(m_data), static_cast<size_t>(m_size));
			m_data = nullptr;
		}
		if (m_fd != -1) {
			::close(m_fd);
			m_fd = -1;
		}
		m_size = 0;
	}
	
	bool MmapFile::IsOpen() const {
		return m_fd != -1;
	}
#endif

	const uint8_t* MmapFile::Data() const {
		return m_data;
	}

	uint64_t MmapFile::Size() const {
		return m_size;
	}

	const uint8_t* MmapFile::At(uint64_t offset) const {
		if (!m_data || offset >= m_size)
			return nullptr;
		return m_data + offset;
	}

	bool MmapFile::InRange(uint64_t offset, uint64_t size) const {
		if (!m_data || size == 0)
			return false;

		if (offset > m_size)
			return false;
		
		return (m_size - offset) >= size;
	}

}