---
id: node_mu3td1ku1t629jd11ytz7k
name: StaticForgeArchive
---

# StaticForgeArchive

`StaticForgeArchive` is the runtime handle for a loaded `.sfpak` file. It is populated by `StaticForgeReader::Load` and then used to retrieve asset data.

## Ownership, move and Clone

`StaticForgeArchive` can be moved but not copied, so a copy doesn't open another file handle by accident. Use `Clone()` for a second archive object:

```cpp
StaticForge::StaticForgeArchive a;
StaticForge::StaticForgeArchive b = std::move(a); // OK
StaticForge::StaticForgeArchive c = a;            // compile error
StaticForge::StaticForgeArchive d = b.Clone();    // OK, shares the loaded data
```

An archive object can be loaded again with `StaticForgeReader::Load`. The reader calls the private `Reset()` first, which closes the stream and releases the mapping, the loaded data and the errors of the previous archive. Clones keep the old data.

## Internal state

| Member   | Description                                                                                |
|----------|--------------------------------------------------------------------------------------------|
| `m_data` | `std::shared_ptr<const Internal::ArchiveData>`, the loaded data, shared with clones       |
| `m_mmap` | `std::shared_ptr<Internal::MmapFile>`, the memory mapping, shared with clones              |
| `m_stream` | `std::ifstream` used for stream-based reads, every archive object has its own            |

`Internal::ArchiveData` (in `Internal/ArchiveCore.h`) holds everything the reader loads:

| Field             | Description                                                     |
|-------------------|-----------------------------------------------------------------|
| `path`            | Path to the `.sfpak` file on disk                               |
| `header`          | Parsed `StaticForgeHeader` struct                               |
| `indexEntries`    | `std::vector<StaticForgeIndexEntry>` — one per file             |
| `hashNameToEntry` | `std::unordered_map<uint64_t, size_t>` for O(1) lookup          |
| `indexToName`     | `std::unordered_map<size_t, std::string>` for reverse name lookup |

The data is never changed after loading, so it can be shared between archive objects and threads without locking. If nothing is loaded `m_data` is `nullptr` and the getters use empty data.

`Clone()` copies both `shared_ptr`s. The clone gets its own stream (opened on the first load) and its own errors. `CloseMapped()` only releases the archive's own reference to the mapping, it is unmapped when the last archive releases it. `OpenMapped()` on an archive that isn't mapped creates a new mapping.

The loading logic (`LoadEntry`, `LoadEntryRange`, `DecompressBlocks`, `VerifyEntry`) lives in `Internal/ArchiveCore` as free functions. They get the data, the entry and an `EntrySource` (the stream or the mapping), so they can be used by other archive types too. The decompressor is shared, its `DecompressTo` doesn't change the object and reports errors through a parameter.

## LoadAsset flow

1. Normalize the slashes of the key and hash it with `HashFilename` (64-bit FNV-1a).
2. Look up the hash in `hashNameToEntry` to find the index position.
3. Call `LoadEntry`, which is shared by all full loads (vector or buffer, stream or mapped):
- Clear the stream state, so a previous failed read does not break this one.
- If `compressedFileSize == 0` (uncompressed): read `fileSize` bytes at `dataOffset + fileOffset` directly into the target and validate the FNV-1a checksum.
- If `compressedFileSize != 0` (compressed): read `compressedFileSize` bytes, validate the checksum, then decompress block by block (`DecompressBlocks`). Each block is prefixed with `[uint32_t compressedSize][uint32_t originalSize]` and decompressed independently, directly into the target.
4. With a vector, the block sizes are checked before the vector is resized to `fileSize`, so a damaged size in the index can't cause a huge allocation. The vector is only changed on success.

`LoadAssetAt(index, ...)` skips steps 1 and 2 and loads the entry at the given index directly.

The buffer variants (`LoadAsset(key, buffer, bufferSize)`, ...) fail with an error if `bufferSize` is smaller than `fileSize`. Only `fileSize` bytes are written.

## LoadAssetRange flow

`LoadEntryRange` checks that `[offset, offset + size)` is inside of `fileSize` (overflow safe). The checksum is **not** validated, because it covers the whole stored data.

- Uncompressed: read `size` bytes at `dataOffset + fileOffset + offset`.
- Compressed: `DecompressBlocks` walks the block frames. Blocks before the range are skipped by reading only their 8 byte frame. Blocks completely inside of the range are decompressed directly into the buffer; the first and last block, if only partly inside, are decompressed into a temporary block buffer and the needed part is copied. The walk stops after the last block of the range.

Reading a range near the end of a large compressed asset reads one frame per block before it, which is small compared to decompressing them.

## LoadAssetMapped / GetAssetMapped flow

1. Verify the archive is memory-mapped (`IsMapped()`).
2. Hash the key and look up the index entry.
3. Get the pointer with `MmapFile::At(offset, size)`, which returns `nullptr` if the range `[dataOffset + fileOffset, storedSize]` is not inside the mapped file. An empty range at the end of the file is valid.
4. `GetAssetMapped` returns a non-owning `AssetView` pointing directly into mapped memory — no copy. Compressed entries are rejected, because their original data is not in memory.  
   `LoadAssetMapped` uses the same `LoadEntry` as the stream version, only the stored bytes come from the mapping instead of the stream. It validates the checksum, then copies the bytes or decompresses them directly from the mapping into the target.

`LoadAssetRangeMapped` works like `LoadAssetRange`, the block frames are read directly from the mapping.

> The `AssetView` is only valid while the archive remains mapped. Call `CloseMapped` only after you are done with all views.

`OpenMapped` works with relative and absolute paths. On Windows the path is made absolute and prefixed with `\\?\`, so long paths work too.

## Access method comparison

| Method                           | Compressed | Allocates | Copies data | Checksum |
|----------------------------------|:----------:|:---------:|:-----------:|:--------:|
| `LoadAsset` / `LoadAssetAt` (vector) | ✅      | ✅         | ✅           | ✅        |
| `LoadAsset` / `LoadAssetAt` (buffer) | ✅      | ❌ ¹       | ✅           | ✅        |
| `LoadAssetRange` / `LoadAssetRangeAt` | ✅     | ❌ ¹       | ✅           | ❌        |
| `LoadAssetMapped` (vector)       | ✅          | ✅         | ✅           | ✅        |
| `LoadAssetMapped` (buffer)       | ✅          | ❌         | ✅           | ✅        |
| `LoadAssetRangeMapped`           | ✅          | ❌ ²       | ✅           | ❌        |
| `GetAssetMapped`                 | ❌          | ❌         | ❌           | ❌        |

¹ No allocation for uncompressed assets. For compressed assets a temporary buffer for the stored data (full load) or for one block (range) is used.  
² For compressed assets a temporary buffer for a block that is only partly inside of the range.

## Accessor methods

All getters read directly from the header or the index entries and are cheap:

```cpp
archive.GetVersion();       // uint64_t
archive.GetFileCount();     // size_t
archive.GetIndexOffset();   // uint64_t
archive.GetIndexSize();     // uint64_t
archive.GetDataOffset();    // uint64_t
archive.StoresNames();      // bool — true if name table is present
archive.GetHashName(i);     // uint64_t — 0 if i is invalid
archive.GetFileOffset(i);   // uint64_t — 0 if i is invalid
archive.GetFileSize(i);     // uint64_t — 0 if i is invalid
archive.GetFilePadding(i);  // uint64_t — 0 if i is invalid
archive.GetName(i);         // std::string — empty if name table not present or i is invalid

archive.Contains(key);              // bool — asset exists
archive.GetAssetSize(key, &size);   // bool — false if the asset doesn't exist, size is the original size
```

`Contains` and `GetAssetSize` only use the index in memory, they don't read the file and don't add errors.

## VerifyAll

`VerifyAll` opens the file stream if needed and, for every index entry, reads the stored bytes (`compressedFileSize` for compressed entries, otherwise `fileSize`) in 1 MB chunks while computing the FNV-1a checksum. A mismatch or missing data adds one error per asset (`Asset '<name>' is damaged (...)`, or the hash if no names are stored). All entries are checked, the function returns `false` if any of them is damaged. Nothing is decompressed.

## Managing the file stream

After `StaticForgeReader::Load` succeeds the archive's file stream is open. `LoadAsset` reopens it automatically if it was closed. You can control this manually:

```cpp
archive.CloseFileStream(); // release the file handle
archive.OpenFileStream();  // reopen when needed
bool open = archive.IsFileStreamOpen();
```

## Managing memory mapping

Memory mapping is independent of the file stream and must be opened explicitly:

```cpp
archive.OpenMapped();  // map the file into memory
archive.IsMapped();    // bool
archive.CloseMapped(); // unmap — invalidates all AssetViews
```

## Error handling

`StaticForgeArchive` collects errors (see *ErrorSupport*). Always check the return value and read the error when it returns `false`:

```cpp
if (!archive.LoadAsset("audio/music.ogg", &data)) {
    // possible reasons: key not found, seek/read failure,
    // checksum mismatch, decompression failure
    std::cerr << archive.TakeError() << std::endl;
}

if (!archive.LoadAssetMapped("textures/ui.png", &data)) {
    // additional possible reason: archive is not mapped
    std::cerr << archive.TakeError() << std::endl;
}

if (archive.GetAssetMapped("textures/ui.png").data == nullptr) {
    // additional possible reason: entry is compressed
    std::cerr << archive.TakeError() << std::endl;
}
```

## StaticForgeConcurrentArchive

`StaticForgeConcurrentArchive` uses the same `ArchiveData` and the same loading functions of `Internal/ArchiveCore`, only the stream handling and the errors are different. It doesn't derive from `ErrorSupport`.

| Member    | Description                                                                 |
|-----------|-----------------------------------------------------------------------------|
| `m_data`  | `std::shared_ptr<const Internal::ArchiveData>`, shared with clones          |
| `m_mmap`  | `std::shared_ptr<Internal::MmapFile>`, shared with clones                   |
| `m_state` | `std::unique_ptr<State>` with the stream pool and the errors (defined in the `.cpp`) |

`State` is behind a pointer, because a `std::mutex` can't be moved. The move constructor gives the moved-from object a new empty `State`, so it stays usable as an empty archive.

### Stream pool

- `freeStreams`: open streams that are not used, protected by `poolMutex`.
- `poolSize`, `activeCount`, `openCount`: `std::atomic`, so the getters can read them without the lock. They are only changed while the lock is held.
- **Acquire:** waits on `poolCondition` until a free stream exists or `openCount < poolSize`. A free stream is taken, otherwise a new one is opened (outside of the lock).
- **Release:** gives the stream back and wakes up one waiting load. If the pool got smaller in the meantime (`openCount > poolSize`), the stream is closed instead.
- A `Lease` object gives the stream back in its destructor, so it isn't lost if loading throws.
- `SetPoolSize` closes the free streams above the new size and wakes up all waiting loads, so it can be called while loading.
- `StaticForgeReader::Load` puts the stream it used for reading into the pool as the first free stream.

Mapped loads use `m_mmap` directly and don't touch the pool. `m_mmap` itself is not protected, so `OpenMapped`/`CloseMapped` must not run while other threads load.

### Errors

Every function takes an optional `std::string* errorOut`. `Report()` writes the error there, or adds it to the list of the calling thread in `Internal::ThreadErrors` (`Internal/ThreadErrors.h`): a `std::unordered_map<std::thread::id, ...>` protected by a mutex. Each thread gets a number when it reports its first error. The number is kept when its errors are cleared, so the same thread keeps the same number. The format of a thread's list is the same as `ErrorSupport` (`[Error]: ` for the first line, `>        ` for the following ones). `GetAllErrors()` sorts the threads by number and prefixes every line with `[Thread <n>] `.

## Archive sets

`StaticForgeArchiveSet` and `StaticForgeConcurrentArchiveSet` share `Internal::ArchiveSetIndex<Archive>` (`Internal/ArchiveSetIndex.h`):

- A list of slots: the archive pointer and the `shared_ptr` to the `ArchiveData` the archive had when it was added.
- A lookup `asset hash -> (slot, entry index)`, so finding an asset stays O(1) for any number of archives.
- `Add` checks all hashes of the new archive against the lookup before anything is added. A hash that already exists means the same path (or a hash collision) in two archives and is an error.
- `Remove` rebuilds the lookup, because the slot indices change.

The sets are friends of their archive class and call the private `LoadEntry`/`LoadEntryRange`/`VerifyAllInto` with an error string, so errors end up in the set and not in the archive. Before each load the set compares the archive's current `m_data` with the stored one. If the archive was loaded again in the meantime, the stored entry belongs to the old file and an error is reported instead.

The sets only keep pointers and never touch the archives in their destructor. `StaticForgeArchiveSet` derives from `ErrorSupport`, `StaticForgeConcurrentArchiveSet` uses `Internal::ThreadErrors` like the concurrent archive. Its lookup is only read while loading, so loads need no lock; `Add`, `Remove` and `Clear` change it and must not run at the same time.
