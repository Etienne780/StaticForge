---
id: node_mu3td1ku1w2zp279b2ssx
name: Inspecting Archives
---

# Inspecting Archives

Use `--info` to display header and index information of one or more existing archives, `--verify` to check them for damage, and `--extract` to turn an archive back into normal files. All three accept archives with any extension, the content of the file decides if it is an archive.

## Basic info

{copy}
```bash
StaticForgeTool --info ./build/textures.sfpak
```

Output:

{label="Output"}
```
Archive: textures.sfpak
Version: 3
File count: 2
Index offset: 64
Index size: 80
Data offset: 4096
```

## Inspecting multiple archives at once

`--info` can be repeated to inspect several archives in one invocation:

{copy}
```bash
StaticForgeTool --info ./build/textures.sfpak --info ./build/audio.snd
```

Each archive is printed in sequence, separated by an empty line. With more than one archive the full path is shown instead of only the file name.

## Verbose index listing

Add `--verbose` to also print every index entry:

{copy}
```bash
StaticForgeTool --info ./build/textures.sfpak --verbose
```

Output includes a line per file showing its hash (hexadecimal), byte offset relative to the data section, and original size:

{label="Output" indent=2}
```
Index entries:
  [0] hash: a01c85352df03fae, offset: 0, size: 9
  [1] hash: 0254d0414b3d0eaf, offset: 4096, size: 200000
```

If the archive was built with `--store-names` (or `store-names = true` in the meta file), each line also shows the original relative path:

{label="Output" indent=2}
```
Index entries:
  [0] name: textures/background.png, hash: 0254d0414b3d0eaf, offset: 0, size: 200000
  [1] name: textures/ui.png, hash: a6cf994350b589ad, offset: 200704, size: 3
```

## Verifying archives

`--verify` checks the checksums of all files in one or more archives:

{copy}
```bash
StaticForgeTool --verify "./build/textures.sfpak, ./build/main.sfpak"
```

Output:

{label="Output"}
```
[Error] ./build/textures.sfpak: Asset 'textures/ui.png' is damaged (checksum mismatch)
[OK]    ./build/main.sfpak (12 files)
```

Several damaged files of one archive are listed below it:

{label="Output"}
```
[Error] ./build/levels.sfpak:
- Asset 'levels/level_01.map' is damaged (checksum mismatch)
- Asset 'levels/level_02.map' is damaged (checksum mismatch)
```

All archives are checked, also if one of them is damaged. The exit code is not `0` if any archive is damaged or can't be loaded, so `--verify` can be used in build or install scripts. Without stored names a damaged file is reported by its hash. A file that is not an archive is reported as `Invalid archive magic`.

## Extracting archives

`--extract` writes every file of an archive back to disk. Each archive gets its own folder `<output>/<archive file name without its extension>/`:

{copy}
```bash
StaticForgeTool --extract ./build/textures.sfpak --output ./unpacked --mkdir
```

All archives are loaded and checked before anything is written. The output directory is only created when all of them can be extracted.

With stored names the original folder structure is restored (the path inside the archive is relative to the source folder):

```tree
unpacked/
    textures/                   # from textures.sfpak
        textures/
            background.png
            ui.png
    main/                       # from main.sfpak, no names stored
        a01c85352df03fae.bin
        0254d0414b3d0eaf.bin
```

Without stored names the original paths are unknown, so the files are named after their hash, matching the `--info --verbose` output: `./unpacked/main/a01c85352df03fae.bin`.

Several archives can be extracted at once:

{copy}
```bash
StaticForgeTool --extract "./build/textures.sfpak, ./build/main.sfpak" --output ./unpacked --mkdir --verbose
```

Rules:

- Existing files are never overwritten. If one of the target files already exists, nothing is extracted. Empty the output folder first.
- Names in the archive must be relative paths. An archive with names like `../file` or `C:/file` is rejected, so a manipulated archive can't write outside of the output folder.
- Only the last extension is removed for the folder name: `textures.hd.sfpak` goes to `textures.hd/`, `main.data.bin` to `main.data/`. Two archives that would get the same folder are an error.

> **Note:** Without name storage the index only contains hashes. The original path is not recoverable from the archive alone — keep your source assets, or pack with `--store-names` if you need to extract with the original names.
