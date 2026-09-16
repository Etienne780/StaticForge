---
id: node_mu3td1kuzgocka16m5db0
name: Loading Assets at Runtime
---

# Loading Assets at Runtime

At runtime, use `StaticForgeReader` to open a `.sfpak` file and `StaticForgeArchive` to load individual assets.

## Opening an archive

```cpp
#include <StaticForgeCore/StaticForge.h>

StaticForge::StaticForgeArchive archive;
StaticForge::StaticForgeReader reader;

if (!reader.Load("./build/textures.sfpak", &archive)) {
    // reader.GetError() contains a description of what went wrong
    std::cerr << reader.GetError() << std::endl;
}
```

## Loading a file by path

Pass the **relative path** of the file exactly as it appeared under the source directory:

```cpp
std::vector<std::byte> data;
if (!archive.LoadAsset("textures/background.png", data)) {
    std::cerr << archive.GetError() << std::endl;
}
// data now contains the raw bytes of the file
```

> **Important:** The asset path must exactly match the path relative to the source root. Slashes are normalized internally, but typos or case mismatches will prevent the asset from being found.

## Managing the file stream

After `StaticForgeReader::Load` succeeds, the archive's file stream is open. `LoadAsset` reopens it automatically if it was closed. You can control this manually:

```cpp
archive.CloseFileStream(); // release the file handle
archive.OpenFileStream();  // reopen when needed
bool open = archive.IsFileStreamOpen();
```

## Memory-mapped access

For better read performance, the archive can be opened using memory mapping instead of a file stream. This avoids repeated `seek`/`read` calls and lets the OS manage page caching.

```cpp
if (!archive.OpenMapped()) {
    std::cerr << archive.GetError() << std::endl;
}

// Zero-copy view — valid only while the archive stays mapped
StaticForge::AssetView view = archive.GetAssetMapped("textures/background.png");
if (view.data == nullptr) {
    std::cerr << archive.GetError() << std::endl;
}

// Or copy into a buffer, same as LoadAsset
std::vector<std::byte> data;
if (!archive.LoadAssetMapped("textures/background.png", data)) {
    std::cerr << archive.GetError() << std::endl;
}

archive.CloseMapped();
bool mapped = archive.IsMapped();
```

`GetAssetMapped` returns a non-owning `AssetView` directly into mapped memory — no allocation or copy needed. The view becomes invalid as soon as `CloseMapped` is called.

> **Important:** Memory-mapped access is **not supported for compressed assets**. Calling `GetAssetMapped` or `LoadAssetMapped` on a compressed entry will return an error. Use `LoadAsset` (stream-based) instead, which handles decompression automatically.

## Compression

Assets can be stored compressed inside an archive. This is transparent when using the stream-based API — `LoadAsset` detects compressed entries and decompresses them automatically before returning the data:

```cpp
// Works for both compressed and uncompressed assets
std::vector<std::byte> data;
if (!archive.LoadAsset("maps/level1.map", data)) {
    std::cerr << archive.GetError() << std::endl;
}
```

Internally, compressed archives use per-block framing: each 4096-byte chunk is compressed independently and prefixed with its compressed and original size. This means the decompressor never needs to hold the entire file in memory at once.

Whether an entry is compressed can be determined from the index — if `compressedFileSize != 0` the entry is compressed, otherwise it is stored raw.

| Access method        | Uncompressed | Compressed          |
|----------------------|:------------:|:-------------------:|
| `LoadAsset`          | ✅            | ✅ (auto-decompress) |
| `LoadAssetMapped`    | ✅            | ❌ not supported     |
| `GetAssetMapped`     | ✅            | ❌ not supported     |

## Error handling

Both `StaticForgeReader` and `StaticForgeArchive` expose error messages. Always check the return value and read `GetError()` when it returns `false`:

```cpp
if (!archive.LoadAsset("audio/music.ogg", data)) {
    // possible reasons: key not found, seek failure, checksum mismatch,
    // decompression failure, or compressed asset accessed via mmap
    std::cerr << archive.GetError() << std::endl;
}
```