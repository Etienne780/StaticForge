#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <algorithm>

#include "StaticForgeConcurrentArchive.h"
#include "Internal/ArchiveCore.h"
#include "Internal/MmapFile.h"
#include "Internal/ThreadErrors.h"

namespace StaticForge {

	static size_t ResolvePoolSize(size_t poolSize) {
		if (poolSize != DEFAULT_POOL_SIZE)
			return poolSize;

		const size_t hardware = std::thread::hardware_concurrency();
		return hardware != 0 ? hardware : FALLBACK_POOL_SIZE;
	}

	struct StaticForgeConcurrentArchive::State {
		// stream pool, the counters are atomic so they can be read without the lock
		std::mutex poolMutex;
		std::condition_variable poolCondition;
		std::vector<std::unique_ptr<std::ifstream>> freeStreams;
		std::atomic<size_t> poolSize{ ResolvePoolSize(DEFAULT_POOL_SIZE) };
		std::atomic<size_t> activeCount{ 0 };
		std::atomic<size_t> openCount{ 0 };

		Internal::ThreadErrors errors;

		// takes a free stream, opens a new one if the pool isn't full, or waits
		std::unique_ptr<std::ifstream> Acquire(const StaticForgePath& path, std::string* errorOut) {
			std::unique_lock<std::mutex> lock(poolMutex);
			poolCondition.wait(lock, [this] { 
				return !freeStreams.empty() || openCount < poolSize; 
			});

			activeCount++;
			if (!freeStreams.empty()) {
				auto stream = std::move(freeStreams.back());
				freeStreams.pop_back();
				return stream;
			}

			openCount++;
			lock.unlock(); // opening the file doesn't need the lock

			auto stream = std::make_unique<std::ifstream>(path, std::ios::binary | std::ios::in);
			if (stream->is_open())
				return stream;

			lock.lock();
			openCount--;
			activeCount--;
			lock.unlock();
			poolCondition.notify_one();

			*errorOut = "Failed to open archive file at '" + path.generic_u8string() + "'";
			return nullptr;
		}

		// gives a stream back, it is closed if the pool got smaller in the meantime
		void Release(std::unique_ptr<std::ifstream> stream) {
			{
				std::lock_guard<std::mutex> lock(poolMutex);
				activeCount--;
				if (openCount > poolSize)
					openCount--;
				else
					freeStreams.push_back(std::move(stream));
			}
			poolCondition.notify_one();
		}

		// removes free streams (all, or only those above the pool size)
		// they are closed when the returned vector is destroyed, outside of the lock
		std::vector<std::unique_ptr<std::ifstream>> TakeFreeStreams(bool onlyAbovePoolSize) {
			std::vector<std::unique_ptr<std::ifstream>> closing;
			std::lock_guard<std::mutex> lock(poolMutex);
			while (!freeStreams.empty() && (!onlyAbovePoolSize || openCount > poolSize)) {
				closing.push_back(std::move(freeStreams.back()));
				freeStreams.pop_back();
				openCount--;
			}
			return closing;
		}

		// gives the stream back to the pool, also if loading throws
		class Lease {
		public:
			Lease(State& state, std::unique_ptr<std::ifstream> stream)
				: m_state(state), m_stream(std::move(stream)) {
			}

			~Lease() {
				if (m_stream)
					m_state.Release(std::move(m_stream));
			}

			Lease(const Lease&) = delete;
			Lease& operator=(const Lease&) = delete;

			std::ifstream* Get() const {
				return m_stream.get();
			}

		private:
			State& m_state;
			std::unique_ptr<std::ifstream> m_stream;
		};
	};

	StaticForgeConcurrentArchive::StaticForgeConcurrentArchive()
		: m_state(std::make_unique<State>()) {
	}

	StaticForgeConcurrentArchive::~StaticForgeConcurrentArchive() = default;

	StaticForgeConcurrentArchive::StaticForgeConcurrentArchive(StaticForgeConcurrentArchive&& other)
		: m_data(std::move(other.m_data))
		, m_mmap(std::move(other.m_mmap))
		, m_state(std::move(other.m_state)) {
		// the moved-from object stays usable as an empty archive
		other.m_state = std::make_unique<State>();
	}

	StaticForgeConcurrentArchive& StaticForgeConcurrentArchive::operator=(StaticForgeConcurrentArchive&& other) {
		if (this != &other) {
			m_data = std::move(other.m_data);
			m_mmap = std::move(other.m_mmap);
			m_state = std::move(other.m_state);
			other.m_state = std::make_unique<State>();
		}
		return *this;
	}

	StaticForgeConcurrentArchive StaticForgeConcurrentArchive::Clone() const {
		StaticForgeConcurrentArchive clone;
		clone.m_data = m_data;
		clone.m_mmap = m_mmap;
		clone.m_state->poolSize = m_state->poolSize.load();
		return clone;
	}

	StaticForgeArchive StaticForgeConcurrentArchive::CloneArchive() const {
		StaticForgeArchive archive;
		archive.m_data = m_data;
		archive.m_mmap = m_mmap;
		return archive;
	}

	StaticForgeConcurrentArchive StaticForgeArchive::CloneConcurrent(size_t poolSize) const {
		StaticForgeConcurrentArchive concurrent;
		concurrent.m_data = m_data;
		concurrent.m_mmap = m_mmap;
		concurrent.SetPoolSize(poolSize);
		return concurrent;
	}

	void StaticForgeConcurrentArchive::SetPoolSize(size_t poolSize) {
		{
			std::lock_guard<std::mutex> lock(m_state->poolMutex);
			m_state->poolSize = ResolvePoolSize(poolSize);
		}

		// free streams above the new size are closed now, used ones when they are given back
		auto closing = m_state->TakeFreeStreams(true);

		// a larger pool can wake up waiting loads
		m_state->poolCondition.notify_all();
	}

	size_t StaticForgeConcurrentArchive::GetPoolSize() const {
		return m_state->poolSize;
	}

	size_t StaticForgeConcurrentArchive::GetActiveStreamCount() const {
		return m_state->activeCount;
	}

	size_t StaticForgeConcurrentArchive::GetOpenStreamCount() const {
		return m_state->openCount;
	}

	void StaticForgeConcurrentArchive::CloseFreeStreams() {
		auto closing = m_state->TakeFreeStreams(false);
		m_state->poolCondition.notify_all();
	}

	bool StaticForgeConcurrentArchive::LoadAsset(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, outData, nullptr, 0, &error)) {
			Report("Failed to load asset '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetAt(size_t fileIndex, std::vector<std::byte>* outData, std::string* errorOut) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			Report("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(entries[fileIndex], false, outData, nullptr, 0, &error)) {
			Report("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAsset(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, false, nullptr, buffer, bufferSize, &error)) {
			Report("Failed to load asset '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetAt(size_t fileIndex, std::byte* buffer, uint64_t bufferSize, std::string* errorOut) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			Report("Failed to load asset, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(entries[fileIndex], false, nullptr, buffer, bufferSize, &error)) {
			Report("Failed to load asset at index '" + std::to_string(fileIndex) + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetRange(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset range, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, false, offset, size, buffer, &error)) {
			Report("Failed to load range of asset '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetRangeAt(size_t fileIndex, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut) {
		const auto& entries = Data().indexEntries;
		if (fileIndex >= entries.size()) {
			Report("Failed to load asset range, index '" + std::to_string(fileIndex) + "' is out of range (file count "
				+ std::to_string(entries.size()) + ")", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntryRange(entries[fileIndex], false, offset, size, buffer, &error)) {
			Report("Failed to load range of asset at index '" + std::to_string(fileIndex) + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::Contains(const std::string& key) const {
		return Data().Find(key) != nullptr;
	}

	bool StaticForgeConcurrentArchive::GetAssetSize(const std::string& key, uint64_t* outSize) const {
		auto* entry = Data().Find(key);
		if (!entry)
			return false;

		*outSize = entry->fileSize;
		return true;
	}

	bool StaticForgeConcurrentArchive::VerifyAll(std::string* errorOut) {
		std::vector<std::string> errors;
		if (VerifyAllInto(&errors))
			return true;

		// errorOut gets one line per damaged asset
		if (errorOut) {
			std::string allErrors;
			for (const auto& error : errors)
				allErrors += (allErrors.empty() ? "" : "\n") + error;
			*errorOut = allErrors;
		}
		else {
			for (const auto& error : errors)
				Report(error, nullptr);
		}
		return false;
	}

	bool StaticForgeConcurrentArchive::VerifyAllInto(std::vector<std::string>* errorsOut) {
		std::string error;
		auto stream = m_state->Acquire(Data().path, &error);
		if (!stream) {
			errorsOut->push_back("Failed to verify: " + error);
			return false;
		}
		State::Lease lease(*m_state, std::move(stream));

		// read in chunks, so large assets don't have to be loaded completely
		std::vector<char> buffer(1024 * 1024);

		const auto& data = Data();
		bool allValid = true;
		for (size_t i = 0; i < data.indexEntries.size(); i++) {
			std::string reason;
			if (Internal::VerifyEntry(data, data.indexEntries[i], *lease.Get(), &buffer, &reason))
				continue;

			allValid = false;
			errorsOut->push_back("Asset '" + data.GetDisplayName(i) + "' is damaged (" + reason + ")");
		}

		return allValid;
	}

	bool StaticForgeConcurrentArchive::OpenMapped(std::string* errorOut) {
		if (IsMapped())
			return true;

		// a new mapping, clones that share the old one keep it
		auto mmap = std::make_shared<Internal::MmapFile>();
		std::string error;
		if (!mmap->Open(Data().path, &error)) {
			Report("Failed to open map: " + error, errorOut);
			return false;
		}

		m_mmap = std::move(mmap);
		return true;
	}

	void StaticForgeConcurrentArchive::CloseMapped() {
		// only releases this archive's reference, clones keep the mapping open
		m_mmap.reset();
	}

	bool StaticForgeConcurrentArchive::IsMapped() const {
		return m_mmap && m_mmap->IsOpen();
	}

	AssetView StaticForgeConcurrentArchive::GetAssetMapped(const std::string& key, std::string* errorOut) {
		if (!IsMapped()) {
			Report("Failed to get asset mapped for key '" + key + "', archive is not mapped", errorOut);
			return {};
		}

		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to get asset mapped, entry with key '" + key + "' not found", errorOut);
			return {};
		}

		std::string error;
		const std::byte* src = Internal::GetEntryMapped(Data(), *entry, *m_mmap, &error);
		if (!src) {
			Report("Failed to get asset mapped for key '" + key + "': " + error, errorOut);
			return {};
		}

		AssetView view{};
		view.data = src;
		view.size = static_cast<size_t>(entry->fileSize);
		return view;
	}

	bool StaticForgeConcurrentArchive::LoadAssetMapped(const std::string& key, std::vector<std::byte>* outData, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset mapped, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, outData, nullptr, 0, &error)) {
			Report("Failed to load asset mapped '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetMapped(const std::string& key, std::byte* buffer, uint64_t bufferSize, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset mapped, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntry(*entry, true, nullptr, buffer, bufferSize, &error)) {
			Report("Failed to load asset mapped '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::LoadAssetRangeMapped(const std::string& key, uint64_t offset, uint64_t size, std::byte* buffer, std::string* errorOut) {
		auto* entry = Data().Find(key);
		if (!entry) {
			Report("Failed to load asset range mapped, entry with key '" + key + "' not found", errorOut);
			return false;
		}

		std::string error;
		if (!LoadEntryRange(*entry, true, offset, size, buffer, &error)) {
			Report("Failed to load range of asset mapped '" + key + "': " + error, errorOut);
			return false;
		}

		return true;
	}

	bool StaticForgeConcurrentArchive::StoresNames() const {
		return Data().header.nameTableHeaderOffset != 0;
	}

	const StaticForgePath& StaticForgeConcurrentArchive::GetPath() const {
		return Data().path;
	}

	uint64_t StaticForgeConcurrentArchive::GetVersion() const {
		return Data().header.version;
	}

	size_t StaticForgeConcurrentArchive::GetFileCount() const {
		return Data().indexEntries.size();
	}

	uint64_t StaticForgeConcurrentArchive::GetIndexOffset() const {
		return Data().header.indexOffset;
	}

	uint64_t StaticForgeConcurrentArchive::GetIndexSize() const {
		return Data().header.indexSize;
	}

	uint64_t StaticForgeConcurrentArchive::GetDataOffset() const {
		return Data().header.dataOffset;
	}

	std::string StaticForgeConcurrentArchive::GetName(size_t index) const {
		if (!StoresNames())
			return {};

		const auto& names = Data().indexToName;
		auto it = names.find(index);
		if (it == names.end())
			return {};

		return it->second;
	}

	// the index getters return 0 for an invalid index
	uint64_t StaticForgeConcurrentArchive::GetHashName(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].hashName : 0;
	}

	uint64_t StaticForgeConcurrentArchive::GetFileOffset(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].fileOffset : 0;
	}

	uint64_t StaticForgeConcurrentArchive::GetFileSize(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].fileSize : 0;
	}

	uint64_t StaticForgeConcurrentArchive::GetFilePadding(size_t index) const {
		const auto& entries = Data().indexEntries;
		return index < entries.size() ? entries[index].filePadding : 0;
	}

	bool StaticForgeConcurrentArchive::IsValid() const {
		return m_state->errors.IsValid();
	}

	std::string StaticForgeConcurrentArchive::GetError() const {
		return m_state->errors.Get();
	}

	std::string StaticForgeConcurrentArchive::TakeError() {
		return m_state->errors.Take();
	}

	void StaticForgeConcurrentArchive::ClearErrors() {
		m_state->errors.Clear();
	}

	std::string StaticForgeConcurrentArchive::GetAllErrors() const {
		return m_state->errors.GetAll();
	}

	std::string StaticForgeConcurrentArchive::TakeAllErrors() {
		return m_state->errors.TakeAll();
	}

	void StaticForgeConcurrentArchive::ClearAllErrors() {
		m_state->errors.ClearAll();
	}

	const Internal::ArchiveData& StaticForgeConcurrentArchive::Data() const {
		static const Internal::ArchiveData empty{};
		return m_data ? *m_data : empty;
	}

	void StaticForgeConcurrentArchive::Report(const std::string& error, std::string* errorOut) const {
		if (errorOut)
			*errorOut = error;
		else
			m_state->errors.Add(error);
	}

	bool StaticForgeConcurrentArchive::LoadEntry(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		std::vector<std::byte>* outVector,
		std::byte* outBuffer,
		uint64_t bufferSize,
		std::string* errorOut
	) {
		Internal::EntrySource source;
		if (mapped) {
			if (!IsMapped()) {
				*errorOut = "Archive is not mapped (use OpenMapped() before this call)";
				return false;
			}
			source.mmap = m_mmap.get();
			return Internal::LoadEntry(Data(), entry, source, outVector, outBuffer, bufferSize, errorOut);
		}

		auto stream = m_state->Acquire(Data().path, errorOut);
		if (!stream)
			return false;
		State::Lease lease(*m_state, std::move(stream));

		source.stream = lease.Get();
		return Internal::LoadEntry(Data(), entry, source, outVector, outBuffer, bufferSize, errorOut);
	}

	bool StaticForgeConcurrentArchive::LoadEntryRange(
		const Internal::StaticForgeIndexEntry& entry,
		bool mapped,
		uint64_t offset,
		uint64_t size,
		std::byte* outBuffer,
		std::string* errorOut
	) {
		Internal::EntrySource source;
		if (mapped) {
			if (!IsMapped()) {
				*errorOut = "Archive is not mapped (use OpenMapped() before this call)";
				return false;
			}
			source.mmap = m_mmap.get();
			return Internal::LoadEntryRange(Data(), entry, source, offset, size, outBuffer, errorOut);
		}

		auto stream = m_state->Acquire(Data().path, errorOut);
		if (!stream)
			return false;
		State::Lease lease(*m_state, std::move(stream));

		source.stream = lease.Get();
		return Internal::LoadEntryRange(Data(), entry, source, offset, size, outBuffer, errorOut);
	}

	void StaticForgeConcurrentArchive::AddStreamToPool(std::ifstream stream) {
		std::lock_guard<std::mutex> lock(m_state->poolMutex);
		m_state->freeStreams.push_back(std::make_unique<std::ifstream>(std::move(stream)));
		m_state->openCount++;
	}

	void StaticForgeConcurrentArchive::Reset() {
		CloseMapped();
		m_data.reset();

		const size_t poolSize = m_state->poolSize;
		m_state = std::make_unique<State>();
		m_state->poolSize = poolSize;
	}

}
