---
id: node_mu3td1kuuhnvwc15ryskk
name: File Format
---

# File Format

A `.sfpak` file consists of three required regions and three optional regions written sequentially: **Header**, **Index**, **Data**, and — when name storage is enabled — **Name Table Header**, **Name Table Entries**, and **Name Table String Data**.

All integers are stored little-endian on all platforms.

## Layout

```
[ HEADER                 ]  52 bytes, padded to a multiple of ALIGNMENT_HEADER = 32 (64 bytes)
[ INDEX                  ]  N × 40 bytes, then padded to ALIGNMENT_FILE = 4096
[ DATA BLOCKS            ]  files written back-to-back, each padded to ALIGNMENT_FILE = 4096
[ NAME TABLE HEADER      ]  optional — present when store-names is enabled
[ NAME TABLE ENTRIES     ]  optional — one entry per file
[ NAME TABLE STRING DATA ]  optional — raw UTF-8 path strings packed together
```

## Header

Magic bytes `SFPK` followed by six `uint64_t` fields.

| Field                   | Type       | Description                                                     |
|-------------------------|------------|-----------------------------------------------------------------|
| `magic`                 | `char[4]`  | Always `S`, `F`, `P`, `K`                                       |
| `version`               | `uint64_t` | Format version (currently `3`)                                  |
| `fileCount`             | `uint64_t` | Number of files in this archive                                 |
| `indexOffset`           | `uint64_t` | Byte offset to the start of the index                           |
| `indexSize`             | `uint64_t` | Total size of the index in bytes (`fileCount × 40`)             |
| `dataOffset`            | `uint64_t` | Byte offset to the start of data                                |
| `nameTableHeaderOffset` | `uint64_t` | Byte offset to the Name Table Header, or `0` if not present     |

The reader only accepts archives with exactly its own `version`. The format is not backward compatible, archives are rebuilt with the application that uses them.

## Index entry

One 40-byte entry per file, stored contiguously starting at `indexOffset`.

| Field                | Type       | Description                                                                              |
|----------------------|------------|------------------------------------------------------------------------------------------|
| `hashName`           | `uint64_t` | 64-bit FNV-1a hash of the relative file path                                             |
| `fileOffset`         | `uint64_t` | Byte offset of the file's data block relative to `dataOffset`                            |
| `fileSize`           | `uint64_t` | Original (uncompressed) file size in bytes                                               |
| `compressedFileSize` | `uint64_t` | Total bytes on disk including the framing headers. `0` means stored uncompressed         |
| `filePadding`        | `uint32_t` | Padding bytes appended after the file data to reach the next 4096-byte boundary          |
| `checksum`           | `uint32_t` | 32-bit FNV-1a checksum of the data as written on disk (covers framing headers + compressed data) |

## Data blocks

Each file occupies one data block starting at `dataOffset + fileOffset`. The block is always padded with zero bytes to the next 4096-byte boundary; the padding size is stored in `filePadding`.

When `compressedFileSize == 0` the block contains the raw file bytes verbatim.

When `compressedFileSize != 0` the block contains a sequence of independently compressed chunks:

```
[ CHUNK 0 ][ CHUNK 1 ][ ... ][ CHUNK N ]
```

Each chunk has the following layout:

```
[ compressedSize : uint32_t ]  size of the compressed payload that follows
[ originalSize   : uint32_t ]  size of the decompressed output for this chunk
[ payload        : bytes    ]  compressedSize bytes of compressed data
```

Chunks are produced by splitting the original file into pieces of the compression block size (default 64 KB, at most `MAX_COMPRESSION_BLOCK_SIZE`) and compressing each piece independently. The decompressor reads chunks sequentially until all `compressedFileSize` bytes are consumed. Because each chunk carries its own original size, the reader does not need to know the block size that was used for packing.

The FNV-1a checksum covers all chunk bytes as written on disk — framing headers included — so integrity is verified before any decompression takes place.

> **Note:** Compressed data blocks cannot be accessed via memory mapping. Use the stream-based `LoadAsset` API which handles decompression automatically.

## Name Table Header

Present only when the archive was built with name storage enabled. Located at `nameTableHeaderOffset`.

| Field                      | Type       | Description                                                  |
|----------------------------|------------|--------------------------------------------------------------|
| `entryOffset`              | `uint64_t` | Byte offset to the first Name Table Entry                    |
| `stringDataOffset`         | `uint64_t` | Byte offset to the start of the compressed String Data       |
| `stringDataSize`           | `uint64_t` | Size of the String Data after decompression                  |
| `stringDataCompressedSize` | `uint64_t` | Size of the compressed String Data in the file               |

## Name Table Entry

One 16-byte entry per file, stored contiguously starting at `entryOffset`, in the same order as the index.

| Field        | Type       | Description                                                                    |
|--------------|------------|--------------------------------------------------------------------------------|
| `hash`       | `uint64_t` | FNV-1a hash of the path (matches `hashName` in the corresponding index entry)  |
| `nameLength` | `uint32_t` | Length of the UTF-8 path string in bytes                                       |
| `nameOffset` | `uint32_t` | Byte offset within the decompressed String Data where this path starts         |

## Name Table String Data

UTF-8 path strings packed end-to-end with no separators. The whole block is always compressed as one piece with the same compression as the file data (without framing header), because paths usually share long prefixes and compress very well (often to 20 - 50 %).

To read a filename, read `stringDataCompressedSize` bytes at `stringDataOffset`, decompress them to `stringDataSize` bytes, then take `nameLength` bytes at `nameOffset`. Paths use `/` as separator and are relative to the source folder.

## Alignment

- The header region is padded to a multiple of 32 bytes (`ALIGNMENT_HEADER`), which makes it 64 bytes.
- Every file data block is padded to 4096 bytes (`ALIGNMENT_FILE`) — one filesystem page.
- Padding bytes are always zero.

This alignment ensures that file offsets are always multiples of 4096, which allows direct memory-mapped access for uncompressed assets without additional alignment work.

## Validation

`StaticForgeReader::Load` checks the archive against the real file size before allocating any memory:

- `indexSize` must match `fileCount × 40`, and the index must be inside the file.
- `dataOffset` and the name table must be inside the file.
- The data of every index entry must be inside the file, and every hash must be unique.
- The compressed string data must be inside the file, and `stringDataSize` must be possible for its compressed size (at most `compressedSize × 13108`).
- Every name must be inside the decompressed string data.

When loading a compressed entry, every chunk's `originalSize` must be at most `MAX_COMPRESSION_BLOCK_SIZE` and the sum must match `fileSize`. A corrupted archive is rejected with an error instead of crashing.
