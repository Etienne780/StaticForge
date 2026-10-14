---
id: node_mu3td1ku93dq7vjxmvr2
name: Packing Assets
---

# Packing Assets

Use `StaticForgeTool` to pack a folder of assets into a `.sfpak` archive.

## Basic usage

```bash
StaticForgeTool --source ./assets --output ./build
```

This scans `./assets` recursively and writes the archive to `./build/main.sfpak`.

## Specifying an archive name

By default the archive is named `main`. Use `--name` to override:

```bash
StaticForgeTool --source ./assets --output ./build --name textures
```

This produces `./build/textures.sfpak`. The name may contain dots (`textures.hd` produces `textures.hd.sfpak`) and non-ASCII characters, but no `/` or `\`.

## Multiple source folders

`--source` can be repeated to include multiple directories:

```bash
StaticForgeTool --source ./textures --source ./audio --output ./build
```
or
```bash
StaticForgeTool --source "./textures, ./audio" --output ./build
```

Every source path has to be a directory. Asset paths inside the archive are relative to the source folder they were found in, so two source folders must not contain the same relative path.

## Output directory

`--output` is always a directory. The archive file name comes from `--name` or from a meta file (see *Meta Files*), so a path like `./build/main.sfpak` is rejected. Directory names with dots (e.g. `./build/v1.0`) are fine.

If the output directory doesn't exist yet, add `--mkdir`:

```bash
StaticForgeTool --source ./assets --output ./build/packed --mkdir
```

## Compression

Add `--compress` to compress every file larger than 4096 bytes:

```bash
StaticForgeTool --source ./assets --output ./build --compress
```

Compressed assets are decompressed automatically by `LoadAsset` and `LoadAssetMapped`, only the zero-copy view `GetAssetMapped` is not possible for them. Data that doesn't compress well, like JPG, PNG or OGG files, is stored uncompressed automatically (per block and per file, see `--compress-min-saving`), so compression never makes an archive noticeably larger. The compression can be tuned with `--compress-block-size`, `--compress-table-size`, `--compress-min-match` and `--compress-min-saving`, see *CLI Reference*. The defaults are the most efficient values for typical game assets.

Already compressed formats (PNG, JPG, OGG, ...) barely get smaller, text and uncompressed data (shaders, scripts, JSON, meshes) compress well.

## Bool arguments

Bool arguments like `--mkdir`, `--verbose`, `--store-names` and `--compress` can be used without a value. `--verbose` is the same as `--verbose true`. Accepted values are `true`/`false`, `1`/`0`, `yes`/`no` and `y`/`n`, anything else is an error.

## Verbose output

Add `--verbose` (or `--v`) to print detailed information about every file processed:

```bash
StaticForgeTool --source ./assets --output ./build --verbose
```

The output has these sections:

- **Paths** - the output and source paths, with the absolute path in brackets.
- **Meta files** - every meta file found and the values it sets.
- **Files** - the files of every archive with their source file, and the excluded files.
- **Writing archive** - one section per archive, sorted by name: the settings that are really used (command line, meta file or default), the layout of header, index and data, and for every file its hash, offset, size, how it is stored (raw or compressed, and why it is raw), padding and checksum. Archives with stored names also show the layout of the name table.

```
===================================
Writing archive 'textures' -> ./build/textures.sfpak
===================================
store-names: true
compress: true
compression: block size 65536, table size 2^16, min match 8, min saving 10%
header: offset 0, 64 bytes (52 + padding)
index: offset 64, 3 x 40 = 120 bytes
data: offset 4096
files (3):
  [0] textures/hero.png
      - hash: f798a942ed32fd87
      - offset: 0 (in file 4096)
      - size: 20000 bytes
      - stored: raw, compressed 20008 bytes, saving below 10%
      - padding: 480 bytes (aligned 20480 bytes)
      - checksum: fbd33a7b
  ...
  [2] textures/ui/theme.txt
      - hash: 8e586dbde1d8ef94
      - offset: 32768 (in file 36864)
      - size: 23492 bytes
      - stored: 3633 bytes compressed (-84%), 1 block
      - padding: 463 bytes (aligned 4096 bytes)
      - checksum: a08daa81
name table:
  - header: offset 40960, 32 bytes
  - entries: offset 40992, 3 x 16 = 48 bytes
  - names: offset 41040, 60 bytes, raw
archive size: 41100 bytes
```

The offset of a file is relative to the start of the data, `in file` is the position in the archive file.

## Unicode

File names, directories, archive names and command line arguments can contain any Unicode characters (e.g. `纹理/背景.png`). Names are stored as UTF-8.
