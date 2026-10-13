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

Compressed assets are decompressed automatically by `LoadAsset`, but they can't be accessed with the memory-mapped API. The compression can be tuned with `--compress-block-size`, `--compress-table-size` and `--compress-min-match`, see *CLI Reference*. The defaults are the most efficient values for typical game assets.

Already compressed formats (PNG, JPG, OGG, ...) barely get smaller, text and uncompressed data (shaders, scripts, JSON, meshes) compress well.

## Bool arguments

Bool arguments like `--mkdir`, `--verbose`, `--store-names` and `--compress` can be used without a value. `--verbose` is the same as `--verbose true`. Accepted values are `true`/`false`, `1`/`0`, `yes`/`no` and `y`/`n`, anything else is an error.

## Verbose output

Add `--verbose` (or `--v`) to print detailed information about every file processed:

```bash
StaticForgeTool --source ./assets --output ./build --verbose
```

## Unicode

File names, directories, archive names and command line arguments can contain any Unicode characters (e.g. `纹理/背景.png`). Names are stored as UTF-8.
