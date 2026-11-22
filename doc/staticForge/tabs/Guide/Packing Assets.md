---
id: node_mu3td1ku93dq7vjxmvr2
name: Packing Assets
---

# Packing Assets

Use `StaticForgeTool` to pack a folder of assets into an archive.

{style=outline}
[toc]

## Basic usage

{copy}
```bash
StaticForgeTool --source ./assets --output ./build
```

This scans `./assets` recursively and writes the archive to `./build/main.sfpak`.

## Specifying an archive name

By default the archive is named `main`. Use `--name` to override:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --name textures
```

This produces `./build/textures.sfpak`. The name may contain dots (`textures.hd` produces `textures.hd.sfpak`) and non-ASCII characters, but no `/` or `\`.

## Archive extension

Archives get the extension `.sfpak` by default. Use `--extension` (or `--ext`) to give all archives another extension, the leading dot is optional:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --extension pak
```

This produces `./build/main.pak`. A single archive can also get its own extension in its meta file (`extension = pak;`, see [[meta-files]]). The value of the command line wins over the meta file, without both `.sfpak` is used.

- The extension is appended to the archive name, also if the name contains dots: `textures.hd` with `pak` produces `textures.hd.pak`.
- Multi-part extensions work (`.data.bin`).
- The extension must not be empty and must not contain `/`, `\` or characters that are invalid in Windows file names (`< > : " | ? *`). It must not end with `.` or a space, because Windows removes them.
- Two archives of one build must not end up with the same file name, e.g. archive `a` with `.b.c` and archive `a.b` with `.c`. The build fails with an error.

The reader accepts archives with any extension, it only checks the content of the file.

## Multiple source folders

`--source` can be repeated to include multiple directories:

{copy}
```bash
StaticForgeTool --source ./textures --source ./audio --output ./build
```

or

{copy}
```bash
StaticForgeTool --source "./textures, ./audio" --output ./build
```

Every source path has to be a directory. Asset paths inside the archive are relative to the source folder they were found in, so two source folders must not contain the same relative path.

## Output directory

`--output` is always a directory. The archive file names come from the archive name (`--name` or a meta file, see [[meta-files]]) and the extension. Directory names with dots (e.g. `./build/v1.0`) are fine, a path like `./build/main.sfpak` is a directory as well, the archive is written to `./build/main.sfpak/main.sfpak`. An existing file as output path is an error.

If the output directory doesn't exist yet, add `--mkdir`:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build/packed --mkdir
```

The archives are written to temporary files (`<archive file>.tmp`, e.g. `main.sfpak.tmp`) and renamed when all of them are written. If the build fails, the existing archives stay as they were, the temporary files are removed and a directory created by `--mkdir` is removed again.

## Compression

Add `--compress` to compress every file larger than 4096 bytes:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --compress
```

Compressed assets are decompressed automatically by `LoadAsset` and `LoadAssetMapped`, only the zero-copy view `GetAssetMapped` is not possible for them. Data that doesn't compress well, like JPG, PNG or OGG files, is stored uncompressed automatically (per block and per file, see `--compress-min-saving`), so compression never makes an archive noticeably larger. The compression can be tuned with `--compress-block-size`, `--compress-table-size`, `--compress-min-match` and `--compress-min-saving`, see [[cli-reference]]. The defaults are the most efficient values for typical game assets.

Already compressed formats (PNG, JPG, OGG, ...) barely get smaller, text and uncompressed data (shaders, scripts, JSON, meshes) compress well.

## Incremental builds

With `--cache-dir` only the archives whose files or settings changed are built, the others are skipped and keep their modification time:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build/packed --cache-dir ./build/intermediates/release/sfcache
```

{label="Output"}
```
Build was successful (1 archive built, 3 up to date)
```

The cache directory holds one small text file per archive (`<archive name>.sfcache`), it is created automatically. Keep it outside of the output directory, so the released build only contains the archives. Use one cache directory per build configuration:

{collapsible}
```tree
build/
  intermediates/
    debug/
      sfcache/          # cache of the debug build
    release/
      sfcache/          # cache of the release build
        audio.sfcache
        main.sfcache
  packed/               # only the archives
    audio.sfpak
    main.sfpak
```

An archive is built again if:

- a file was added, removed or changed (size, or content after a changed modification time)
- its settings changed (meta file or command line)
- its extension changed, the file with the old extension is deleted after the build
- the archive file is missing or was changed
- the cache file is missing or invalid, or the archive format version changed
- `--rebuild` is set

A file with a new modification time but the same content, e.g. after a `git checkout`, doesn't trigger a build.

An archive that is not built anymore, e.g. because its meta file was deleted, is deleted from the output directory together with its cache file after a successful build. The cache file stores the extension of the archive, so it is found also if it doesn't use `.sfpak`:

{label="Output"}
```
audio: not built anymore, archive and cache file are deleted

Build was successful (1 archive built, 2 up to date, 1 removed)
```

Only archives with a cache file of the same output directory are deleted. `--verbose` shows the reason for every archive:

{label="Output"}
```
===================================
Up to date check (cache: ./build/intermediates/release/sfcache)
===================================
audio: rebuild, archive extension changed (was '.sfpak')
data: rebuild, 'data/items.json' changed
main: up to date
textures: up to date
```

The cache files store the modification times of the local file system, so they belong to one machine and are not meant to be committed.

## Bool arguments

Bool arguments like `--mkdir`, `--verbose`, `--store-names` and `--compress` can be used without a value. `--verbose` is the same as `--verbose true`. Accepted values are `true`/`false`, `1`/`0`, `yes`/`no` and `y`/`n`, anything else is an error.

## Verbose output

Add `--verbose` (or `--v`) to print detailed information about every file processed:

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --verbose
```

The output has these sections:

- **Paths** - the output and source paths, with the absolute path in brackets.
- **Meta files** - every meta file found and the values it sets.
- **Files** - the files of every archive with their source file, and the excluded files.
- **Up to date check** - only with `--cache-dir`, the reason why an archive is built or skipped.
- **Writing archive** - one section per archive, sorted by name: the settings that are really used (command line, meta file or default), the layout of header, index and data, and for every file its hash, offset, size, how it is stored (raw or compressed, and why it is raw), padding and checksum. Archives with stored names also show the layout of the name table.

::: Example: Writing archive
{label="Output" indent=2}
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
:::

The offset of a file is relative to the start of the data, `in file` is the position in the archive file.

## Unicode

File names, directories, archive names and command line arguments can contain any Unicode characters (e.g. `纹理/背景.png`). Names are stored as UTF-8.
