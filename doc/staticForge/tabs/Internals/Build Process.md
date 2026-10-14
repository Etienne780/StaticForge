---
id: node_mu3td1kulgv50d577zxf
name: Build Process
---

# Build Process

`StaticForgeBuilder::Build()` resets the data of a previous build, then runs the internal phases below in sequence.

## 1. CheckCompressionSettings

Validates the compression settings set on the builder (block size, table size, min match length) against the `MIN_COMPRESSION_*` / `MAX_COMPRESSION_*` ranges. Settings that are not set are skipped.

## 2. CheckFilepaths

Validates that:

- the default archive name is a valid file name (no `/` or `\`, not `.` or `..`)
- all source paths exist and are directories
- the output path is not an archive file path (ending with `.sfpak`) and, if it exists, is a directory

A missing output directory is only an error if `SetCreateOutputDir(true)` was not called. The directory is created later in `CreateOutputDir`, so a build that fails while scanning leaves nothing behind.

## 3. ScanFiles

Each source directory is read **once** into a list of files. Read errors are reported as build errors. The list is sorted by the UTF-8 path, because every file system returns the files in a different order. Then two passes run over these lists:

**Pass 1 — Meta collection:**  
Collects all files ending with `.sfpak.meta` and builds a `directory → StaticForgeMetaData` mapping. Each entry stores the archive name, excluded extensions, the `store-names` and `compress` flags and the optional compression settings. Archive names and compression settings are validated here.

**Pass 2 — File assignment:**  
Iterates all non-meta files, computes their relative path (from the path itself, so a symbolic link to a file is stored under its own path with the content of its target), hashes it with 64-bit FNV-1a, and assigns each file to the appropriate `ArchiveGroup` by walking up the directory tree until a meta entry is found. Falls back to the default archive name if no meta file applies.

For each file the builder also:
- Skips files whose name ends with one of the excluded extensions (case insensitive).
- Reports an error if the same relative path occurs more than once in the build, also if the files go into different archives (e.g. two `--source` folders that both contain `textures/hero.png`). It can't be decided which of the files is the right one.
- Reports an error on hash collisions between different paths.
- Applies the overrides when an archive group is created:
  - `store-names` and `compress`: if the global flag is set, it is forced on even when the meta file sets it to `false`.
  - compression settings: builder value → meta file value → default.

At the end the files of every archive group are sorted by their relative path (UTF-8 byte order), also when they come from several source paths. The same sources therefore always give the same archive, byte for byte, on every system.

## 4. CheckUpToDate

Only runs with a cache directory (`SetCacheDir`). For every archive group the cache file `<cache dir>/<archive name>.sfcache` is read and compared in this order, the first difference is the reason for the build:

1. `SetRebuild(true)` was called
2. the cache file is missing or can't be parsed, or has another cache version
3. the archive format version (`VERSION`), the absolute output path or the settings are different
4. the archive file is missing, or its size or modification time is different
5. the file list is different (both lists are sorted by path)
6. the size of a file is different
7. the modification time of a file is different and the 64-bit FNV-1a hash of its content as well

If only the modification time of a file changed, the archive is up to date and only its cache file is written again with the new time. Archives that are up to date are skipped by all following phases.

Cache files in the cache directory without an archive group of this build belong to archives that are not built anymore. If the cache file is valid and contains the same output directory, the archive and its cache file are deleted in `RemoveOldArchives` after the build.

The cache file is text:

```
StaticForgeCache 1
format 3
output D:/dev/game/build/packed
settings store-names=1 compress=1 block-size=65536 table-size=16 min-match=8 min-saving=10 exclude=.psd
archive 41100 134364826999478632
files 3
20000 134364826980778712 af8f0deb8b372d7b textures/hero.png
9000 134364826981220378 87ddee1928e25947 textures/ui/button.png
23492 134364826981425961 499fa1f519db8fe5 textures/ui/theme.txt
```

| Line | Content |
|------|---------|
| `StaticForgeCache` | version of the cache file format (`BUILD_CACHE_VERSION`) |
| `format` | archive format version |
| `output` | absolute output directory |
| `settings` | settings of the archive that are used for the build |
| `archive` | size and modification time of the archive file |
| `files` | number of files, followed by one line per file: size, modification time, content hash (hex) and the relative path (rest of the line) |

Modification times are the raw values of the file clock (`std::filesystem::file_time_type`), so a cache file is only valid on the system that wrote it. The content hash is calculated while the file is written into the archive, no extra read is needed.

## 5. CreateOutputDir

Creates the output directory and its missing parents if `SetCreateOutputDir(true)` was called. The created directories are remembered, so they can be removed again if the build fails.

## 6. CreateGroups

The archive groups are written in order of their name. For each `ArchiveGroup` the temporary file `<output>/<archive name>.sfpak.tmp` is created and the following phases run in order:

**CalculateDataStart** — computes the byte offset at which file data begins. This is the sum of the aligned header size and the index table size, rounded up to the next 4096-byte boundary.

**WriteData** — writes `dataStart` bytes of zero-padding to reserve space for the header and index, then writes all file data blocks. During this pass, `blockOffset`, `filePadding`, `checksum`, and `compressedFileSize` are filled in on each file entry so the index can be written correctly afterwards.

**WriteHeader** — seeks back to offset `0` and writes the `StaticForgeHeader` struct, padded to a multiple of 32 bytes.

**WriteIndex** — writes one `StaticForgeIndexEntry` per file directly after the header. Checksums computed during `WriteData` are included here — no second seek is needed.

**BuildNameTable** *(optional, when store-names is enabled)* — computes the byte layout of the Name Table Header, Name Table Entries, and String Data region, and compresses all names as one block. Offsets are appended after the last data block.

**WriteNameTable** *(optional)* — seeks to `nameTableStart` (set at the end of `WriteData`) and writes the Name Table Header, Name Table Entries, and the compressed UTF-8 string data.

## 7. CommitArchives

When all archives are written, every temporary file is renamed to `<archive name>.sfpak` and replaces the old archive.

## 8. WriteCacheFiles

Only runs with a cache directory. Writes the cache files of the built archives and of the archives whose cache changed, each to a temporary file that is renamed when it is complete. The cache directory is created if it is missing.

## 9. RemoveOldArchives

Only runs with a cache directory. Deletes the archives found by `CheckUpToDate` that are not built anymore, together with their cache files.

If any phase from `CreateOutputDir` on fails, all temporary files are removed and the directories created by `CreateOutputDir` are removed again if they are empty. The old archives stay as they were, unless the error happens while renaming: the archives renamed until then are already replaced. The cache files of a failed build stay as they were, so the next build checks the archives again.

## Write phases summary

| Phase             | Seeks to          | What it writes                                                               |
|-------------------|-------------------|------------------------------------------------------------------------------|
| `WriteData`       | `0` (start)       | `dataStart` zero-padding, then all file data blocks in order                 |
| `WriteHeader`     | `0` (seekp back)  | `StaticForgeHeader` struct padded to a multiple of 32 bytes                  |
| `WriteIndex`      | after header      | One `StaticForgeIndexEntry` per file, including computed checksums           |
| `WriteNameTable`  | `nameTableStart` (seekp forward) | Name Table Header, Name Table Entries, compressed UTF-8 string data |

## Compression

When compression is enabled for an archive group (via the meta file or the global `SetCompress` flag), files larger than 4096 bytes are compressed. Compression is applied per block during `WriteData`:

- The file is read in blocks of the compression block size (default 64 KB) and each block is compressed independently.
- Each compressed block is prefixed with a framing header: `[uint32_t compressedSize][uint32_t originalSize]` (little-endian).
- The raw compressed bytes follow immediately after the framing header.
- A block that doesn't get at least `compress-min-saving` percent smaller (default 5 %) is stored raw instead. It is marked by `compressedSize == originalSize`; a compressed block never has this size, because it would not be small enough.
- If the whole file (framing headers included) doesn't get at least `compress-min-saving` percent smaller, the builder seeks back to the start of the file in the archive and writes it again uncompressed (`compressedFileSize = 0`). Only files that don't compress well are read twice. Because the compressed attempt can be a few bytes longer, the archive is cut to its real size with `std::filesystem::resize_file` after writing.
- `compressedFileSize` in the index entry is set to the total number of bytes written for the file, **including** the framing headers. A value of `0` means the entry is stored uncompressed.

The per-block approach means the decompressor can process one block at a time.

The FNV-1a checksum covers the framing headers and compressed block data as written, so integrity verification happens before decompression.

The name table string data uses the same rule: if it doesn't get small enough, it is stored raw and `stringDataCompressedSize == stringDataSize`.

### Compressor

The compressor (`Internal::LZ4Compression`) is an LZ77 compressor inspired by LZ4, with its own simple format:

| Block   | Layout                              | Meaning                                              |
|---------|-------------------------------------|------------------------------------------------------|
| literal | `[0x01][len:u16][len bytes]`        | Bytes stored as they are                             |
| match   | `[0x02][distance:u16][len:u16]`     | Copy `len` bytes from `distance` bytes back          |
| end     | `[0x00]`                            | End of the compressed block                          |

For every position the compressor looks up the last position of the same 4-byte sequence in a hash table (`compress-table-size`). A match is extended as far as possible and only stored if it is at least `compress-min-match` bytes long — shorter matches would take more space than storing the bytes as literals. A match can only reference data inside the same block and at most 65535 bytes back.

The hash table is only allocated when compressing and is reset per block with a generation counter instead of clearing the whole table.

## Overflow safety

All size and offset arithmetic uses checked helpers that return `false` on overflow instead of wrapping silently:

| Helper          | Description                                            |
|-----------------|--------------------------------------------------------|
| `SafeAddU64`    | Addition with overflow check                           |
| `SafeMulU64`    | Multiplication with overflow check                     |
| `SafeAlignSize` | Rounds up to a power-of-two alignment, overflow-safe   |
| `GetHeaderSize` | Returns the aligned header size via `SafeAlignSize`    |

Any overflow causes `Build()` to return `false` with a descriptive error.
