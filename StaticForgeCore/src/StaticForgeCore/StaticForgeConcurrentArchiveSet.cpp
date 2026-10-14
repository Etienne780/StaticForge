#include "StaticForgeConcurrentArchiveSet.h"
#include "Internal/ArchiveCore.h"
#include "Internal/ArchiveSetIndex.h"
#include "Internal/MmapFile.h"
#include "Internal/ThreadErrors.h"

namespace StaticForge {

	struct StaticForgeConcurrentArchiveSet::Impl {
		Internal::ArchiveSetIndex<StaticForgeConcurrentArchive> index;
		Internal::ThreadErrors errors;
	};

	StaticForgeConcurrentArchiveSet::StaticForgeConcurrentArchiveSet()
		: m_impl(std::make_unique<Impl>()) {
	}

	StaticForgeConcurrentArchiveSet::~StaticForgeConcurrentArchiveSet() = default;

	StaticForgeConcurrentArchiveSet::StaticForgeConcurrentArchiveSet(StaticForgeConcurrentArchiveSet&&) noexcept = default;
	StaticForgeConcurrentArchiveSet& StaticForgeConcurrentArchiveSet::operator=(StaticForgeConcurrentArchiveSet&&) noexcept = default;

	bool StaticForgeConcurrentArchiveSet::Add(StaticForgeConcurrentArchive* archive, std::string* errorOut) {
		std::string error;
		if (!m_impl->index.Add(archive, archive ? archive->m_data : nullptr, &error)) {
			Report("Failed to add archive: " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::Remove(StaticForgeConcurrentArchive* archive) {
		return m_impl->index.Remove(archive);
	}

	void StaticForgeConcurrentArchiveSet::Clear() {
		m_impl->index.Clear();
	}

	size_t StaticForgeConcurrentArchiveSet::GetArchiveCount() const {
		return m_impl->index.GetSlots().size();
	}

	StaticForgeConcurrentArchive* StaticForgeConcurrentArchiveSet::FindArchive(const std::string& key) const {
		Internal::ArchiveSetIndex<StaticForgeConcurrentArchive>::Location location;
		return m_impl->index.Find(key, &location) ? location.archive : nullptr;
	}

	bool StaticForgeConcurrentArchiveSet::Contains(const std::string& key) const {
		return FindArchive(key) != nullptr;
	}

	bool StaticForgeConcurrentArchiveSet::GetAssetSize(const std::string& key, uint64_t* outSize) const {
		Internal::ArchiveSetIndex<StaticForgeConcurrentArchive>::Location location;
		if (!m_impl->index.Find(key, &location))
			return false;

		*outSize = location.entry->fileSize;
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAsset(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, false, outData, nullptr, 0, &error)) {
			Report("Failed to load asset '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, false, nullptr, buffer, bufferSize, &error)) {
			Report("Failed to load asset '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset range", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntryRange(*entry, false, offset, size, buffer, &error)) {
			Report("Failed to load range of asset '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	AssetView StaticForgeConcurrentArchiveSet::GetAssetMapped(const std::string& key, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "get asset mapped", errorOut, &archive, &entry))
			return {};

		if (!archive->IsMapped()) {
			Report("Failed to get asset mapped for key '" + key + "', archive '" + archive->GetPath().u8string() + "' is not mapped", errorOut);
			return {};
		}

		std::string error;
		const std::byte* src = Internal::GetEntryMapped(archive->Data(), *entry, *archive->m_mmap, &error);
		if (!src) {
			Report("Failed to get asset mapped for key '" + key + "': " + error, errorOut);
			return {};
		}

		AssetView view{};
		view.data = src;
		view.size = static_cast<size_t>(entry->fileSize);
		return view;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset mapped", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, true, outData, nullptr, 0, &error)) {
			Report("Failed to load asset mapped '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset mapped", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, true, nullptr, buffer, bufferSize, &error)) {
			Report("Failed to load asset mapped '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut) {
		StaticForgeConcurrentArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset range mapped", errorOut, &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntryRange(*entry, true, offset, size, buffer, &error)) {
			Report("Failed to load range of asset mapped '" + key + "': " + error, errorOut);
			return false;
		}
		return true;
	}

	bool StaticForgeConcurrentArchiveSet::VerifyAll(std::string* errorOut) {
		bool allValid = true;
		std::string allErrors;
		for (const auto& slot : m_impl->index.GetSlots()) {
			std::vector<std::string> errors;
			if (slot.archive->VerifyAllInto(&errors))
				continue;

			allValid = false;
			for (const auto& error : errors) {
				const std::string message = "Archive '" + slot.archive->GetPath().u8string() + "': " + error;

				// errorOut gets one line per damaged asset
				if (errorOut)
					allErrors += (allErrors.empty() ? "" : "\n") + message;
				else
					Report(message, nullptr);
			}
		}

		if (errorOut && !allValid)
			*errorOut = allErrors;

		return allValid;
	}

	bool StaticForgeConcurrentArchiveSet::IsValid() const {
		return m_impl->errors.IsValid();
	}

	std::string StaticForgeConcurrentArchiveSet::GetError() const {
		return m_impl->errors.Get();
	}

	std::string StaticForgeConcurrentArchiveSet::TakeError() {
		return m_impl->errors.Take();
	}

	void StaticForgeConcurrentArchiveSet::ClearErrors() {
		m_impl->errors.Clear();
	}

	std::string StaticForgeConcurrentArchiveSet::GetAllErrors() const {
		return m_impl->errors.GetAll();
	}

	std::string StaticForgeConcurrentArchiveSet::TakeAllErrors() {
		return m_impl->errors.TakeAll();
	}

	void StaticForgeConcurrentArchiveSet::ClearAllErrors() {
		m_impl->errors.ClearAll();
	}

	void StaticForgeConcurrentArchiveSet::Report(const std::string& error, std::string* errorOut) const {
		if (errorOut)
			*errorOut = error;
		else
			m_impl->errors.Add(error);
	}

	bool StaticForgeConcurrentArchiveSet::FindEntry(
		const std::string& key,
		const std::string& action,
		std::string* errorOut,
		StaticForgeConcurrentArchive** archiveOut,
		const Internal::StaticForgeIndexEntry** entryOut
	) const {
		Internal::ArchiveSetIndex<StaticForgeConcurrentArchive>::Location location;
		if (!m_impl->index.Find(key, &location)) {
			Report("Failed to " + action + ", entry with key '" + key + "' not found in any archive", errorOut);
			return false;
		}

		// the entry belongs to the data the archive had when it was added
		if (location.archive->m_data.get() != location.data) {
			Report("Failed to " + action + " '" + key + "': archive '" + location.data->path.u8string()
				+ "' was loaded again after it was added to the set, remove and add it again", errorOut);
			return false;
		}

		*archiveOut = location.archive;
		*entryOut = location.entry;
		return true;
	}

}
