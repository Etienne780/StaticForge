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

## Checking assets without loading

`Contains` checks whether an asset exists, `GetAssetSize` returns its original (uncompressed) size. Neither loads any data or adds errors:

```cpp
if (archive.Contains("textures/background.png")) {
    // ...
}

uint64_t size = 0;
if (archive.GetAssetSize("textures/background.png", &size)) {
    buffer.reserve(size);
}
```

## Loading into an existing buffer

Instead of a `std::vector`, an asset can be loaded into any existing memory, e.g. a reused buffer or a GPU upload buffer. The buffer must hold at least the asset size:

```cpp
uint64_t size = 0;
if (archive.GetAssetSize("textures/background.png", &size)) {
    std::byte* buffer = AllocateUploadBuffer(size);
    if (!archive.LoadAsset("textures/background.png", buffer, size)) {
        std::cerr << archive.TakeError() << std::endl;
    }
}
```

If the buffer is too small, nothing is loaded and an error is added. The checksum is validated like with the vector version.

## Loading a part of an asset

`LoadAssetRange` loads only `size` bytes starting at `offset`, e.g. to stream audio or to read the header of a file:

```cpp
std::byte chunk[64 * 1024];
uint64_t offset = 0;
uint64_t total = 0;
archive.GetAssetSize("audio/music.ogg", &total);

while (offset < total) {
    uint64_t size = std::min<uint64_t>(sizeof(chunk), total - offset);
    if (!archive.LoadAssetRange("audio/music.ogg", offset, size, chunk)) {
        std::cerr << archive.TakeError() << std::endl;
        break;
    }
    // use chunk
    offset += size;
}
```

- `offset` and `size` refer to the original (uncompressed) data, the range must be inside of the asset.
- For compressed assets only the blocks that contain the range are decompressed.
- The checksum is **not** validated, because it covers the whole asset. Use `VerifyAll` (see below) to check the archive at startup.

`LoadAssetAt` and `LoadAssetRangeAt` do the same by index.

## Verifying the archive at startup

`VerifyAll` checks the checksum of every asset. Use it e.g. at startup to detect a damaged or partly replaced installation and show a clear message instead of failing later while loading:

```cpp
if (!archive.VerifyAll()) {
    // one error per damaged asset, e.g. "Asset 'textures/ui.png' is damaged (checksum mismatch)"
    std::cerr << archive.TakeError() << std::endl;
    // e.g. ask the user to reinstall or verify the game files
}
```

The data is read in 1 MB chunks, so large assets are not loaded into memory completely. Verifying reads the whole archive once, so for very large archives it takes as long as reading all files.

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

// Also available: LoadAssetMapped(key, buffer, bufferSize) and LoadAssetRangeMapped(key, offset, size, buffer)

archive.CloseMapped();
bool mapped = archive.IsMapped();
```

`GetAssetMapped` returns a non-owning `AssetView` directly into mapped memory — no allocation or copy needed. The view becomes invalid as soon as `CloseMapped` is called. For an empty file the view has `size == 0` and a valid (non-null) `data` pointer.

`LoadAssetMapped` and `LoadAssetRangeMapped` also work for compressed assets, they decompress directly from the mapped memory.

> **Important:** `GetAssetMapped` is **not possible for compressed assets**, because only the compressed data is in memory. It returns an empty view with an error. Use `LoadAssetMapped` instead.

## Compression

Assets can be stored compressed inside an archive. This is transparent for all functions except `GetAssetMapped` — `LoadAsset` detects compressed entries and decompresses them automatically before returning the data:

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
| `LoadAssetRange`     | ✅            | ✅ (needed blocks)   |
| `LoadAssetMapped`    | ✅            | ✅ (auto-decompress) |
| `LoadAssetRangeMapped` | ✅          | ✅ (needed blocks)   |
| `GetAssetMapped`     | ✅            | ❌ not possible      |

## Error handling

`StaticForgeReader` and `StaticForgeArchive` collect error messages. Always check the return value:

```cpp
if (!archive.LoadAsset("audio/music.ogg", &data)) {
    // possible reasons: key not found, seek failure, checksum mismatch,
    // decompression failure, or buffer too small
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
