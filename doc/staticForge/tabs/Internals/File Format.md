---
id: node_mu3td1kuuhnvwc15ryskk
name: File Format
---

# File Format

A `.sfpak` file consists of three required regions and two optional regions written sequentially: **Header**, **Index**, **Data**, and — when name storage is enabled — **Name Table Header**, **Name Table Entries**, and **Name Table String Data**.

## Layout

```
[ HEADER                 ]  aligned to ALIGNMENT_HEADER = 32 bytes
[ INDEX                  ]  N × sizeof(IndexEntry), then padded to ALIGNMENT_FILE = 4096
[ DATA BLOCKS            ]  files written back-to-back, each padded to ALIGNMENT_FILE = 4096
[ NAME TABLE HEADER      ]  optional — present when storenames is enabled
[ NAME TABLE ENTRIES     ]  optional — one entry per file
[ NAME TABLE STRING DATA ]  optional — raw UTF-8 path strings packed together
```

## Header

Magic bytes `SFPK` followed by six `uint64_t` fields. Written little-endian on all platforms.

| Field                   | Type       | Description                                                     |
|-------------------------|------------|-----------------------------------------------------------------|
| `magic`                 | `char[4]`  | Always `S`, `F`, `P`, `K`                                      |
| `version`               | `uint64_t` | Format version (currently `2`)                                  |
| `fileCount`             | `uint64_t` | Number of files in this archive                                 |
| `indexOffset`           | `uint64_t` | Byte offset to the start of the index                           |
| `indexSize`             | `uint64_t` | Total size of the index in bytes                                |
| `dataOffset`            | `uint64_t` | Byte offset to the start of data                                |
| `nameTableHeaderOffset` | `uint64_t` | Byte offset to the Name Table Header, or `0` if not present     |

## Index entry

One entry per file, stored contiguously starting at `indexOffset`.

| Field                | Type       | Description                                                                              |
|----------------------|------------|------------------------------------------------------------------------------------------|
| `hashName`           | `uint64_t` | FNV-1a hash of the relative file path                                                    |
| `fileOffset`         | `uint64_t` | Byte offset of the file's data block relative to `dataOffset`                            |
| `fileSize`           | `uint64_t` | Original (uncompressed) file size in bytes                                               |
| `compressedFileSize` | `uint64_t` | Total compressed bytes on disk, excluding framing headers. `0` means stored uncompressed |
| `filePadding`        | `uint32_t` | Padding bytes appended after the file data to reach the next 4096-byte boundary          |
| `checksum`           | `uint32_t` | FNV-1a checksum of the data as written on disk (covers framing headers + compressed data)|

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

Chunks are produced by splitting the original file into 4096-byte pieces and compressing each piece independently. The decompressor reads chunks sequentially until `compressedSize` bytes consumed equals `compressedFileSize`. Because each chunk carries its own original size, no global decompressed-size context is needed per chunk.

The FNV-1a checksum covers all chunk bytes as written on disk — framing headers included — so integrity is verified before any decompression takes place.

> **Note:** Compressed data blocks cannot be accessed via memory mapping. Use the stream-based `LoadAsset` API which handles decompression automatically.

## Name Table Header

Present only when the archive was built with name storage enabled. Located at `nameTableHeaderOffset`.

| Field              | Type       | Description                                            |
|--------------------|------------|--------------------------------------------------------|
| `entryOffset`      | `uint64_t` | Byte offset to the first Name Table Entry              |
| `stringDataOffset` | `uint64_t` | Byte offset to the start of Name Table String Data     |

## Name Table Entry

One entry per file, stored contiguously starting at `entryOffset`.

| Field        | Type       | Description                                                                    |
|--------------|------------|--------------------------------------------------------------------------------|
| `hash`       | `uint64_t` | FNV-1a hash of the path (matches `hashName` in the corresponding index entry)  |
| `nameLength` | `uint32_t` | Length of the UTF-8 path string in bytes                                       |
| `nameOffset` | `uint32_t` | Byte offset within the String Data region where this path starts               |

## Name Table String Data

Raw UTF-8 path strings packed end-to-end with no separators. To read a filename, seek to `stringDataOffset + nameOffset` and read `nameLength` bytes.

## Alignment

- The header region is padded to 32 bytes (`ALIGNMENT_HEADER`).
- Every file data block is padded to 4096 bytes (`ALIGNMENT_FILE`) — one filesystem page.
- Padding bytes are always zero.

This alignment ensures that file offsets are always multiples of 4096, which allows direct memory-mapped access for uncompressed assets without additional alignment work.