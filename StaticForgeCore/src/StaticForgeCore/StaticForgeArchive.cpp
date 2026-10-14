#include "StaticForgeArchive.h"
#include "Internal/ArchiveCore.h"
#include "Internal/MmapFile.h"

namespace StaticForge {

	StaticForgeArchive::StaticForgeArchive()
		: ErrorSupport(HAS_HEADER) {
	}

	StaticForgeArchive::~StaticForgeArchive() {
		CloseFileStream();
	}

	StaticForgeArchive StaticForgeArchive::Clone() const {
		StaticForgeArchive clone;
		clone.m_data = m_data;
		clone.m_mmap = m_mmap;
		return clone;
	}

	bool StaticForgeArchive::OpenFileStream() {
		m_stream.open(Data().path, std::ios::binary | std::ios::in);
		return m_stream.is_open();
	}

	void StaticForgeArchive::CloseFileStream() {
		m_stream.close();
	}

	bool StaticForgeArchive::IsFileStreamOpen() const {
		return m_stream.is_open();
	}

	bool StaticForgeArchive::LoadAsset(const std::string& key, std::vector<std::byte>* outData) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetAt(size_t fileIndex, std::vector<std::byte>* outData) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			AddError("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntry(entries[fileIndex], false, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetAt(size_t fileIndex, std::byte* buffer, uint64_t bufferSize) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			AddError("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntry(entries[fileIndex], false, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset range, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, false, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRangeAt(size_t fileIndex, uint64_t offset, uint64_t size, std::byte* buffer) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			AddError("Failed to load asset range, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(entries[fileIndex], false, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset at index '" + std::to_string(fileIndex) + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::Contains(const std::string& key) const {
		return Data().Find(key) != nullptr;
	}

	bool StaticForgeArchive::GetAssetSize(const std::string& key, uint64_t* outSize) const {
		auto* entry = Data().Find(key);
		if (!entry)
			return false;

		*outSize = entry->fileSize;
		return true;
	}

	bool StaticForgeArchive::VerifyAll() {
		std::vector<std::string> errors;
		if (VerifyAllInto(&errors))
			return true;

		for (const auto& error : errors)
			AddError(error);
		return false;
	}

	bool StaticForgeArchive::VerifyAllInto(std::vector<std::string>* errorsOut) {
		if (!IsFileStreamOpen() && !OpenFileStream()) {
			errorsOut->push_back("Failed to verify, archive file '" + Data().path.u8string() + "' could not be opened");
			return false;
		}

		// read in chunks, so large assets don't have to be loaded completely
		std::vector<char> buffer(1024 * 1024);

		const auto& data = Data();
		bool allValid = true;
		for (size_t i = 0; i < data.indexEntries.size(); i++) {
			std::string reason;
			if (Internal::VerifyEntry(data, data.indexEntries[i], m_stream, &buffer, &reason))
				continue;

			allValid = false;
			errorsOut->push_back("Asset '" + data.GetDisplayName(i) + "' is damaged (" + reason + ")");
		}

		return allValid;
	}

	bool StaticForgeArchive::OpenMapped() {
		if (IsMapped())
			return true;

		// a new mapping, clones that share the old one keep it
		auto mmap = std::make_shared<Internal::MmapFile>();
		std::string error;
		if (!mmap->Open(Data().path, &error)) {
			AddError("Failed to open map: " + error);
			return false;
		}

		m_mmap = std::move(mmap);
		return true;
	}

	void StaticForgeArchive::CloseMapped() {
		// only releases this archive's reference, clones keep the mapping open
		m_mmap.reset();
	}

	bool StaticForgeArchive::IsMapped() const {
		return m_mmap && m_mmap->IsOpen();
	}

	AssetView StaticForgeArchive::GetAssetMapped(const std::string& key) {
		if (!IsMapped()) {
			AddError("Failed to get asset mapped for key '" + key + "', archive is not mapped");
			return {};
		}

		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to get asset mapped, entry with key '" + key + "' not found");
			return {};
		}

		std::string error;
		const std::byte* src = Internal::GetEntryMapped(Data(), *entry, *m_mmap, &error);
		if (!src) {
			AddError("Failed to get asset mapped for key '" + key + "': " + error);
			return {};
		}

		AssetView view{};
		view.data = src;
		view.size = static_cast<size_t>(entry->fileSize);
		return view;
	}

	bool StaticForgeArchive::LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		auto* entry = Data().Find(key);
		if (!entry) {
			AddError("Failed to load asset range mapped, entry with key '" + key + "' not found");
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, true, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset mapped '" + key + "': " + error);
			return false;
		}

		return true;
	}

	bool StaticForgeArchive::StoresNames() const {
		return Data().header.nameTableHeaderOffset != 0;
	}

	const StaticForgePath& StaticForgeArchive::GetPath() const {
		return Data().path;
	}

	uint64_t StaticForgeArchive::GetVersion() const {
		return Data().header.version;
	}

	size_t StaticForgeArchive::GetFileCount() const {
		return Data().indexEntries.size();
	}

	uint64_t StaticForgeArchive::GetIndexOffset() const {
		return Data().header.indexOffset;
	}

	uint64_t StaticForgeArchive::GetIndexSize() const {
		return Data().header.indexSize;
	}

	uint64_t StaticForgeArchive::GetDataOffset() const {
		return Data().header.dataOffset;
	}

	std::string StaticForgeArchive::GetName(size_t index) const {
		if (!StoresNames())
			return {};

		const auto& names = Data().indexToName;
		auto it = names.find(index);
		if (it == names.end())
			return {};

		return it->second;
	}

	// the index getters return 0 for an invalid index
	uint64_t StaticForgeArchive::GetHashName(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].hashName : 0;
	}

	uint64_t StaticForgeArchive::GetFileOffset(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].fileOffset : 0;
	}

	uint64_t StaticForgeArchive::GetFileSize(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].fileSize : 0;
	}

	uint64_t StaticForgeArchive::GetFilePadding(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].filePadding : 0;
	}

	const Internal::ArchiveData& StaticForgeArchive::Data() const {
		static const Internal::ArchiveData empty{};
		return m_data ? *m_data : empty;
	}

	bool StaticForgeArchive::PrepareSource(bool mapped, Internal::EntrySource* sourceOut, std::string* errorOut) {
		if (mapped) {
			if (!IsMapped()) {
				*errorOut = "Archive is not mapped (use OpenMapped() before this call)";
				return false;
			}
			sourceOut->mmap = m_mmap.get();
			return true;
		}

		if (!IsFileStreamOpen() && !OpenFileStream()) {
			*errorOut = "Failed to open archive file at '" + Data().path.u8string() + "'";
			return false;
		}
		sourceOut->stream = &m_stream;
		return true;
	}

	bool StaticForgeArchive::LoadEntry(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		std::vector<std::byte>* outVector,
		std::byte* outBuffer,
		uint64_t bufferSize,
		std::string* errorOut
	) {
		Internal::EntrySource source;
		if (!PrepareSource(mapped, &source, errorOut))
			return false;

		return Internal::LoadEntry(Data(), entry, source, outVector, outBuffer, bufferSize, errorOut);
	}

	bool StaticForgeArchive::LoadEntryRange(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		uint64_t offset,
		uint64_t size,
		std::byte* outBuffer,
		std::string* errorOut
	) {
		Internal::EntrySource source;
		if (!PrepareSource(mapped, &source, errorOut))
			return false;

		return Internal::LoadEntryRange(Data(), entry, source, offset, size, outBuffer, errorOut);
	}

	void StaticForgeArchive::Reset() {
		ClearErrors();
		CloseFileStream();
		m_stream.clear();
		CloseMapped();
		m_data.reset();
	}

}
