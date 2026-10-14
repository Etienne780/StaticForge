---
id: node_mu3td1ku1t629jd11ytz7k
name: StaticForgeArchive
---

# StaticForgeArchive

`StaticForgeArchive` is the runtime handle for a loaded `.sfpak` file. It is populated by `StaticForgeReader::Load` and then used to retrieve asset data.

## Ownership and move semantics

`StaticForgeArchive` is move-only. Copying is deleted because it owns an open `std::ifstream`.

```cpp
StaticForge::StaticForgeArchive a;
StaticForge::StaticForgeArchive b = std::move(a); // OK
StaticForge::StaticForgeArchive c = a;            // compile error
```

An archive object can be loaded again with `StaticForgeReader::Load`. The reader calls the private `Reset()` first, which closes the stream and the memory mapping and clears the header, index, name table and errors of the previous archive.

## Internal state

| Member              | Description                                                         |
|---------------------|---------------------------------------------------------------------|
| `m_path`            | Path to the `.sfpak` file on disk                                   |
| `m_stream`          | `std::ifstream` used for stream-based reads                         |
| `m_mmap`            | `std::unique_ptr<Internal::MmapFile>` used for memory mapping       |
| `m_compressor`      | `std::unique_ptr<Internal::LZ4Compression>` used for decompression  |
| `m_header`          | Parsed `StaticForgeHeader` struct                                   |
| `m_indexEntries`    | `std::vector<StaticForgeIndexEntry>` — one per file                 |
| `m_hashNameToEntry` | `std::unordered_map<uint64_t, size_t>` for O(1) lookup              |
| `m_indexToName`     | `std::unordered_map<size_t, std::string>` for reverse name lookup   |

## LoadAsset flow

1. Normalize the slashes of the key and hash it with `HashFilename` (64-bit FNV-1a).
2. Look up the hash in `m_hashNameToEntry` to find the index position.
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
3. **Reject compressed entries** — if `compressedFileSize != 0`, add an error and return `false` / empty view. Use `LoadAsset` instead for compressed assets.
4. Get the pointer with `MmapFile::At(offset, size)`, which returns `nullptr` if the range `[dataOffset + fileOffset, fileSize]` is not inside the mapped file. An empty range at the end of the file is valid.
5. `GetAssetMapped` returns a non-owning `AssetView` pointing directly into mapped memory — no copy.  
   `LoadAssetMapped` validates the checksum first, then copies the bytes into `outData`.

> The `AssetView` is only valid while the archive remains mapped. Call `CloseMapped` only after you are done with all views.

`OpenMapped` works with relative and absolute paths. On Windows the path is made absolute and prefixed with `\\?\`, so long paths work too.

## Access method comparison

| Method                           | Compressed | Allocates | Copies data | Checksum |
|----------------------------------|:----------:|:---------:|:-----------:|:--------:|
| `LoadAsset` / `LoadAssetAt` (vector) | ✅      | ✅         | ✅           | ✅        |
| `LoadAsset` / `LoadAssetAt` (buffer) | ✅      | ❌ ¹       | ✅           | ✅        |
| `LoadAssetRange` / `LoadAssetRangeAt` | ✅     | ❌ ¹       | ✅           | ❌        |
| `LoadAssetMapped` (vector)       | ❌          | ✅         | ✅           | ✅        |
| `LoadAssetMapped` (buffer)       | ❌          | ❌         | ✅           | ✅        |
| `LoadAssetRangeMapped`           | ❌          | ❌         | ✅           | ❌        |
| `GetAssetMapped`                 | ❌          | ❌         | ❌           | ❌        |

¹ No allocation for uncompressed assets. For compressed assets a temporary buffer for the stored data (full load) or for one block (range) is used.

## Accessor methods

All getters read directly from `m_header` or `m_indexEntries` and are cheap:

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
    // additional possible reason: entry is compressed
    std::cerr << archive.TakeError() << std::endl;
}
```
