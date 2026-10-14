#include "StaticForgeArchiveSet.h"
#include "Internal/ArchiveCore.h"
#include "Internal/ArchiveSetIndex.h"
#include "Internal/MmapFile.h"

namespace StaticForge {

	struct StaticForgeArchiveSet::Impl {
		Internal::ArchiveSetIndex<StaticForgeArchive> index;
	};

	StaticForgeArchiveSet::StaticForgeArchiveSet()
		: m_impl(std::make_unique<Impl>()) {
	}

	StaticForgeArchiveSet::~StaticForgeArchiveSet() = default;

	StaticForgeArchiveSet::StaticForgeArchiveSet(StaticForgeArchiveSet&&) noexcept = default;
	StaticForgeArchiveSet& StaticForgeArchiveSet::operator=(StaticForgeArchiveSet&&) noexcept = default;

	bool StaticForgeArchiveSet::Add(StaticForgeArchive* archive) {
		std::string error;
		if (!m_impl->index.Add(archive, archive ? archive->m_data : nullptr, &error)) {
			AddError("Failed to add archive: " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::Remove(StaticForgeArchive* archive) {
		return m_impl->index.Remove(archive);
	}

	void StaticForgeArchiveSet::Clear() {
		m_impl->index.Clear();
	}

	size_t StaticForgeArchiveSet::GetArchiveCount() const {
		return m_impl->index.GetSlots().size();
	}

	StaticForgeArchive* StaticForgeArchiveSet::FindArchive(const std::string& key) const {
		Internal::ArchiveSetIndex<StaticForgeArchive>::Location location;
		return m_impl->index.Find(key, &location) ? location.archive : nullptr;
	}

	bool StaticForgeArchiveSet::Contains(const std::string& key) const {
		return FindArchive(key) != nullptr;
	}

	bool StaticForgeArchiveSet::GetAssetSize(const std::string& key, uint64_t* outSize) const {
		Internal::ArchiveSetIndex<StaticForgeArchive>::Location location;
		if (!m_impl->index.Find(key, &location))
			return false;

		*outSize = location.entry->fileSize;
		return true;
	}

	bool StaticForgeArchiveSet::LoadAsset(const std::string& key, std::vector<std::byte>* outData) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, false, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, false, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset '" + key + "': " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset range", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntryRange(*entry, false, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset '" + key + "': " + error);
			return false;
		}
		return true;
	}

	AssetView StaticForgeArchiveSet::GetAssetMapped(const std::string& key) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "get asset mapped", &archive, &entry))
			return {};

		if (!archive->IsMapped()) {
			AddError("Failed to get asset mapped for key '" + key + "', archive '" + archive->GetPath().generic_u8string() + "' is not mapped");
			return {};
		}

		std::string error;
		const std::byte* src = Internal::GetEntryMapped(archive->Data(), *entry, *archive->m_mmap, &error);
		if (!src) {
			AddError("Failed to get asset mapped for key '" + key + "': " + error);
			return {};
		}

		AssetView view{};
		view.data = src;
		view.size = static_cast<size_t>(entry->fileSize);
		return view;
	}

	bool StaticForgeArchiveSet::LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset mapped", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, true, outData, nullptr, 0, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset mapped", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntry(*entry, true, nullptr, buffer, bufferSize, &error)) {
			AddError("Failed to load asset mapped '" + key + "': " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer) {
		StaticForgeArchive* archive = nullptr;
		const Internal::StaticForgeIndexEntry* entry = nullptr;
		if (!FindEntry(key, "load asset range mapped", &archive, &entry))
			return false;

		std::string error;
		if (!archive->LoadEntryRange(*entry, true, offset, size, buffer, &error)) {
			AddError("Failed to load range of asset mapped '" + key + "': " + error);
			return false;
		}
		return true;
	}

	bool StaticForgeArchiveSet::VerifyAll() {
		bool allValid = true;
		for (const auto& slot : m_impl->index.GetSlots()) {
			std::vector<std::string> errors;
			if (slot.archive->VerifyAllInto(&errors))
				continue;

			allValid = false;
			for (const auto& error : errors)
				AddError("Archive '" + slot.archive->GetPath().generic_u8string() + "': " + error);
		}
		return allValid;
	}

	bool StaticForgeArchiveSet::FindEntry(
		const std::string& key,
		const std::string& action,
		StaticForgeArchive** archiveOut,
		const Internal::StaticForgeIndexEntry** entryOut
	) {
		Internal::ArchiveSetIndex<StaticForgeArchive>::Location location;
		if (!m_impl->index.Find(key, &location)) {
			AddError("Failed to " + action + ", entry with key '" + key + "' not found in any archive");
			return false;
		}

		// the entry belongs to the data the archive had when it was added
		if (location.archive->m_data.get() != location.data) {
			AddError("Failed to " + action + " '" + key + "': archive '" + location.data->path.generic_u8string()
				+ "' was loaded again after it was added to the set, remove and add it again");
			return false;
		}

		*archiveOut = location.archive;
		*entryOut = location.entry;
		return true;
	}

}
