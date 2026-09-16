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

1. Hash the key string with `HashFilename`.
2. Look up the hash in `m_hashNameToEntry` to find the index position.
3. Call `LoadEntry`:
- If `compressedFileSize == 0` (uncompressed): seek to `dataOffset + fileOffset`, read `fileSize` bytes, validate the FNV-1a checksum, return the raw bytes.
- If `compressedFileSize != 0` (compressed): seek to `dataOffset + fileOffset`, read `compressedFileSize` bytes, validate the checksum, then decompress block by block. Each block is prefixed with `[uint32_t compressedSize][uint32_t originalSize]` and decompressed independently until all bytes are consumed. The reassembled output must equal `fileSize` bytes.
4. Return the decompressed (or raw) bytes in `outData`.

## LoadAssetMapped / GetAssetMapped flow

1. Verify the archive is memory-mapped (`IsMapped()`).
2. Hash the key and look up the index entry.
3. **Reject compressed entries** — if `compressedFileSize != 0`, add an error and return `false` / empty view. Use `LoadAsset` instead for compressed assets.
4. Verify the range `[dataOffset + fileOffset, fileSize]` is within the mapped region.
5. `GetAssetMapped` returns a non-owning `AssetView` pointing directly into mapped memory — no copy.  
   `LoadAssetMapped` validates the checksum first, then copies the bytes into `outData`.

> The `AssetView` is only valid while the archive remains mapped. Call `CloseMapped` only after you are done with all views.

## Access method comparison

| Method            | Compressed | Allocates | Copies data |
|-------------------|:----------:|:---------:|:-----------:|
| `LoadAsset`       | ✅          | ✅         | ✅           |
| `LoadAssetMapped` | ❌          | ✅         | ✅           |
| `GetAssetMapped`  | ❌          | ❌         | ❌           |

## Accessor methods

All getters read directly from `m_header` or `m_indexEntries` and are cheap:

```cpp
archive.GetVersion();       // uint64_t
archive.GetFileCount();     // size_t
archive.GetIndexOffset();   // uint64_t
archive.GetIndexSize();     // uint64_t
archive.GetDataOffset();    // uint64_t
archive.StoresNames();      // bool — true if name table is present
archive.GetHashName(i);     // uint64_t
archive.GetFileOffset(i);   // uint64_t
archive.GetFileSize(i);     // uint64_t
archive.GetFilePadding(i);  // uint64_t
archive.GetName(i);         // std::string — empty if name table not present
```

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

Both `StaticForgeReader` and `StaticForgeArchive` support error messages. Always check the return value and read `GetError()` when it returns `false`:

```cpp
if (!archive.LoadAsset("audio/music.ogg", data)) {
    // possible reasons: key not found, seek/read failure,
    // checksum mismatch, decompression failure
    std::cerr << archive.GetError() << std::endl;
}

if (!archive.LoadAssetMapped("textures/ui.png", data)) {
    // additional possible reason: entry is compressed
    std::cerr << archive.GetError() << std::endl;
}
```