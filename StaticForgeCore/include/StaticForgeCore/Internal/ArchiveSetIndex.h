#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "Internal/ArchiveCore.h"
#include "Internal/InternalHelpers.h"

namespace StaticForge::Internal {

	/**
	 * @brief Lookup of the archives of a set: asset hash -> archive and entry.
	 *
	 * Doesn't own the archives. Find() can be called from several threads, Add/Remove/Clear not.
	 */
	template<typename Archive>
	class ArchiveSetIndex {
	public:
		struct Slot {
			Archive* archive = nullptr;
			// data the archive had when it was added, to detect if it was loaded again
			std::shared_ptr<const ArchiveData> data;
		};

		struct Location {
			Archive* archive = nullptr;
			const ArchiveData* data = nullptr;
			const StaticForgeIndexEntry* entry = nullptr;
		};

		/**
		 * @brief Adds an archive, nothing is added on failure.
		 *
		 * Fails if the archive is null, not loaded, already added, or contains a path that an added archive has too.
		 */
		bool Add(Archive* archive, std::shared_ptr<const ArchiveData> data, std::string* errorOut) {
			if (!archive) {
				*errorOut = "Archive is null";
				return false;
			}

			if (!data) {
				*errorOut = "Archive is not loaded";
				return false;
			}

			for (const auto& slot : m_slots) {
				if (slot.archive == archive) {
					*errorOut = "Archive '" + data->path.u8string() + "' was already added";
					return false;
				}
			}

			// check everything first, so nothing is added on failure
			for (size_t i = 0; i < data->indexEntries.size(); i++) {
				auto it = m_lookup.find(data->indexEntries[i].hashName);
				if (it == m_lookup.end())
					continue;

				const auto& other = m_slots[it->second.first];
				*errorOut = "Asset '" + data->GetDisplayName(i) + "' of archive '" + data->path.u8string()
					+ "' already exists in archive '" + other.data->path.u8string() + "'";
				return false;
			}

			m_slots.push_back({ archive, std::move(data) });
			Insert(m_slots.size() - 1);
			return true;
		}

		/**
		 * @brief Removes an archive, false if it wasn't added.
		 */
		bool Remove(Archive* archive) {
			auto it = std::find_if(m_slots.begin(), m_slots.end(), [archive](const Slot& s) { return s.archive == archive; });
			if (it == m_slots.end())
				return false;

			// the slot indices change, so the lookup is built again
			m_slots.erase(it);
			m_lookup.clear();
			for (size_t i = 0; i < m_slots.size(); i++)
				Insert(i);
			return true;
		}

		void Clear() {
			m_slots.clear();
			m_lookup.clear();
		}

		const std::vector<Slot>& GetSlots() const {
			return m_slots;
		}

		/**
		 * @brief Finds the archive and entry of an asset key.
		 */
		bool Find(const std::string& key, Location* out) const {
			auto it = m_lookup.find(HashFilename(NormalizeFilepathSlashes(key)));
			if (it == m_lookup.end())
				return false;

			const Slot& slot = m_slots[it->second.first];
			out->archive = slot.archive;
			out->data = slot.data.get();
			out->entry = &slot.data->indexEntries[it->second.second];
			return true;
		}

	private:
		std::vector<Slot> m_slots;
		// hash -> (slot index, entry index)
		std::unordered_map<uint64_t, std::pair<size_t, size_t>> m_lookup;

		void Insert(size_t slotIndex) {
			const auto& entries = m_slots[slotIndex].data->indexEntries;
			m_lookup.reserve(m_lookup.size() + entries.size());
			for (size_t i = 0; i < entries.size(); i++)
				m_lookup[entries[i].hashName] = { slotIndex, i };
		}
	};

}
