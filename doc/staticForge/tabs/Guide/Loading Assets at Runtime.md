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

`Load` validates the whole archive structure (header, index and name table) against the file size, so a damaged or truncated archive is rejected with an error instead of crashing. Archives of another format version are rejected as well; rebuild them with the current tool.

The same archive object can be loaded again with another file. Everything of the previous archive (open stream, memory mapping, index, errors) is released first.

## Loading a file by path

Pass the **relative path** of the file exactly as it appeared under the source directory:

```cpp
std::vector<std::byte> data;
if (!archive.LoadAsset("textures/background.png", &data)) {
    std::cerr << archive.TakeError() << std::endl;
}
// data now contains the raw bytes of the file
```

> **Important:** The asset path must exactly match the path relative to the source root, it is case sensitive and UTF-8 encoded. Backslashes are converted to forward slashes, so `textures\background.png` works too.

## Loading all files

`LoadAssetAt` loads an asset by its index, which is useful to iterate over the whole archive, also if it stores no names:

```cpp
std::vector<std::byte> data;
for (size_t i = 0; i < archive.GetFileCount(); i++) {
    if (!archive.LoadAssetAt(i, &data)) {
        std::cerr << archive.TakeError() << std::endl;
        continue;
    }
    // archive.GetName(i) is the path if the archive stores names, otherwise empty
}
```

## Managing the file stream

After `StaticForgeReader::Load` succeeds, the archive's file stream is open. `LoadAsset` reopens it automatically if it was closed. You can control this manually:

```cpp
archive.CloseFileStream(); // release the file handle
archive.OpenFileStream();  // reopen when needed
bool open = archive.IsFileStreamOpen();
```

A failed load (e.g. a checksum mismatch) does not affect later loads of other assets.

## Memory-mapped access

For better read performance, the archive can be opened using memory mapping instead of a file stream. This avoids repeated `seek`/`read` calls and lets the OS manage page caching.

```cpp
if (!archive.OpenMapped()) {
    std::cerr << archive.TakeError() << std::endl;
}

// Zero-copy view — valid only while the archive stays mapped
StaticForge::AssetView view = archive.GetAssetMapped("textures/background.png");
if (view.data == nullptr) {
    std::cerr << archive.TakeError() << std::endl;
}

// Or copy into a buffer, same as LoadAsset
std::vector<std::byte> data;
if (!archive.LoadAssetMapped("textures/background.png", &data)) {
    std::cerr << archive.TakeError() << std::endl;
}

archive.CloseMapped();
bool mapped = archive.IsMapped();
```

`GetAssetMapped` returns a non-owning `AssetView` directly into mapped memory — no allocation or copy needed. The view becomes invalid as soon as `CloseMapped` is called. For an empty file the view has `size == 0` and a valid (non-null) `data` pointer.

> **Important:** Memory-mapped access is **not supported for compressed assets**. Calling `GetAssetMapped` or `LoadAssetMapped` on a compressed entry will return an error. Use `LoadAsset` (stream-based) instead, which handles decompression automatically.

## Compression

Assets can be stored compressed inside an archive. This is transparent when using the stream-based API — `LoadAsset` detects compressed entries and decompresses them automatically before returning the data:

```cpp
// Works for both compressed and uncompressed assets
std::vector<std::byte> data;
if (!archive.LoadAsset("maps/level1.map", &data)) {
    std::cerr << archive.TakeError() << std::endl;
}
```

Internally, compressed files are split into blocks (64 KB by default, configurable when packing). Each block is compressed independently and prefixed with its compressed and original size.

| Access method        | Uncompressed | Compressed          |
|----------------------|:------------:|:-------------------:|
| `LoadAsset`          | ✅            | ✅ (auto-decompress) |
| `LoadAssetAt`        | ✅            | ✅ (auto-decompress) |
| `LoadAssetMapped`    | ✅            | ❌ not supported     |
| `GetAssetMapped`     | ✅            | ❌ not supported     |

## Error handling

`StaticForgeReader` and `StaticForgeArchive` collect error messages. Always check the return value:

```cpp
if (!archive.LoadAsset("audio/music.ogg", &data)) {
    // possible reasons: key not found, seek failure, checksum mismatch,
    // decompression failure, or compressed asset accessed via mmap
    std::cerr << archive.TakeError() << std::endl;
}
```

| Function        | Description                                                  |
|-----------------|--------------------------------------------------------------|
| `IsValid()`     | `true` if no errors are stored                               |
| `GetError()`    | Returns all stored errors as one string                      |
| `TakeError()`   | Returns all stored errors and clears them                    |
| `ClearErrors()` | Clears all stored errors                                     |

Errors of the archive accumulate over several calls until you take or clear them, so you can load many assets and report all problems at once:

```cpp
archive.LoadAsset("a.png", &a);
archive.LoadAsset("b.png", &b);
if (!archive.IsValid())
    std::cerr << archive.TakeError();  // errors of both calls
```

Errors that no longer apply are cleared automatically: `reader.Load` clears the errors of the reader and of the archive it loads into.
