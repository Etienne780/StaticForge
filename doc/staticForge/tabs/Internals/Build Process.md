---
id: node_mu3td1kulgv50d577zxf
name: Build Process
---

# Build Process

`StaticForgeBuilder::Build()` runs three internal phases in sequence.

## 1. CheckFilepaths

Validates that all source paths exist and that the output path is a directory (no file extension). If `SetCreateOutputDir(true)` was called and the directory is missing, it is created here.

## 2. ScanFiles

Recursively iterates every source path in two passes:

**Pass 1 — Meta collection:**  
Collects all `.sfpak.meta` files and builds a `directory → ArchiveMeta` mapping. Each entry stores the archive name, excluded extensions, and the `storenames` and `compress` flags.

**Pass 2 — File assignment:**  
Iterates all non-meta files, computes their relative path, hashes it with FNV-1a, and assigns each file to the appropriate `ArchiveGroup` by walking up the directory tree until a meta entry is found. Falls back to the default archive name if no meta file applies.

For each file the builder also:
- Detects hash collisions between files in the same group and reports them as errors.
- Applies the `storenames` and `compress` overrides: if a global flag is set, it is forced on even when the meta file sets it to `false`.

## 3. CreateGroups

For each `ArchiveGroup` the following phases run in order:

**CalculateDataStart** — computes the byte offset at which file data begins. This is the sum of the aligned header size and the index table size, rounded up to the next 4096-byte boundary.

**WriteData** — opens the output file and writes `dataStart` bytes of zero-padding to reserve space for the header and index, then writes all file data blocks. During this pass, `blockOffset`, `filePadding`, `checksum`, and `compressedFileSize` are filled in on each file entry so the index can be written correctly afterwards.

**WriteHeader** — seeks back to offset `0` and writes the `StaticForgeHeader` struct, padded to 32 bytes.

**WriteIndex** — writes one `StaticForgeIndexEntry` per file directly after the header. Checksums computed during `WriteData` are included here — no second seek is needed.

**BuildNameTable** *(optional, when storenames is enabled)* — computes the byte layout of the Name Table Header, Name Table Entries, and String Data region. Offsets are appended after the last data block.

**WriteNameTable** *(optional)* — seeks to `nameTableStart` (set at the end of `WriteData`) and writes the Name Table Header, Name Table Entries, and raw UTF-8 string data.

## Write phases summary

| Phase             | Seeks to          | What it writes                                                               |
|-------------------|-------------------|------------------------------------------------------------------------------|
| `WriteData`       | `0` (start)       | `dataStart` zero-padding, then all file data blocks in order                 |
| `WriteHeader`     | `0` (seekp back)  | `StaticForgeHeader` struct padded to 32 bytes                                |
| `WriteIndex`      | after header      | One `StaticForgeIndexEntry` per file, including computed checksums           |
| `WriteNameTable`  | `nameTableStart` (seekp forward) | Name Table Header, Name Table Entries, raw UTF-8 string data |

## Compression

When compression is enabled for an archive group (via the meta file or the global `SetCompress` flag), files larger than one 4096-byte block are compressed. Compression is applied per-block during `WriteData`:

- Each 4096-byte read chunk is compressed independently.
- Each compressed block is prefixed with a framing header: `[uint32_t compressedSize][uint32_t originalSize]`.
- The raw compressed bytes follow immediately after the framing header.
- `compressedFileSize` in the index entry is set to the total number of compressed bytes written (excluding framing headers). A value of `0` means the entry is stored uncompressed.

The per-block approach means the decompressor can process one chunk at a time without buffering the entire file.

The FNV-1a checksum covers the framing headers and compressed block data as written, so integrity verification happens before decompression.

## Overflow safety

All size and offset arithmetic uses checked helpers that return `false` on overflow instead of wrapping silently:

| Helper          | Description                                            |
|-----------------|--------------------------------------------------------|
| `SafeAddU64`    | Addition with overflow check                           |
| `SafeMulU64`    | Multiplication with overflow check                     |
| `SafeAlignSize` | Rounds up to a power-of-two alignment, overflow-safe   |
| `GetHeaderSize` | Returns the aligned header size via `SafeAlignSize`    |

Any overflow causes `Build()` to return `false` with a descriptive error.