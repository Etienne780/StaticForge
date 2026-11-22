---
id: node_mu3td1ku1d4g35y4wv21q
name: Meta Files
---

# Meta Files

Meta files let you split assets from a single source tree into multiple archives automatically.

## File format

A meta file is any file whose name ends with `.sfpak.meta` (e.g. `textures.sfpak.meta` or `my.textures.sfpak.meta`). It uses a simple semicolon-terminated key-value syntax:

{label="sfpak.meta"}
```
archive = textures;
exclude = .png, .jpg;
store-names = true;
```

Each statement is a `key = value;` pair. Whitespace around `=` and `;` is ignored.

Comments start with `#` and run to the end of the line:

{label="sfpak.meta"}
```
# pack all UI assets together
archive = ui;
exclude = .psd; # skip source files
```

Meta files are read as UTF-8, values can contain any Unicode characters (e.g. `archive = 纹理;`). A UTF-8 BOM at the start of the file is ignored. The details of the parser are described in [[meta-file-parser]].

## Supported parameters

{hover=row; striped; col 0: width=210}
| Key                   | Type     | Required | Description                                                                 |
|-----------------------|----------|----------|-----------------------------------------------------------------------------|
| `archive`             | string   | yes      | Name of the archive produced from this directory                            |
| `extension`           | string   | no       | File extension of the archive (e.g. `pak` or `.pak`, default `.sfpak`)      |
| `exclude`             | string[] | no       | Comma-separated list of file extensions to skip (e.g. `.png`, `.tar.gz`)    |
| `store-names`         | bool     | no       | Store original filenames in the archive for debug inspection and extraction |
| `compress`            | bool     | no       | Compress files larger than 4096 bytes                                       |
| `compress-block-size` | int      | no       | Block size in bytes used for compression (1024 - 67108864, default 65536)   |
| `compress-table-size` | int      | no       | Compressor match table size as power of two (8 - 24, default 16)            |
| `compress-min-match`  | int      | no       | Shortest repeated sequence stored as a reference (4 - 65535, default 8)     |
| `compress-min-saving` | int      | no       | Percent a block or file must get smaller to be stored compressed (0 - 99, default 5) |

Bool values are `true` or `false` (case insensitive). Int values are plain decimal numbers without units (`65536`, not `64k`).

Example with all parameters:

{label="sfpak.meta"}
```
archive = main;
extension = pak;
exclude = .png, .jpg, .vert, .help;
store-names = true;
compress = true;
compress-block-size = 65536;
compress-table-size = 16;
compress-min-match = 8;
compress-min-saving = 5;
```

## Archive names

The archive name becomes the file name of the archive (`archive = textures;` produces `textures.sfpak`). It may contain dots (`textures.hd` produces `textures.hd.sfpak`) but no `/` or `\`, and it must not be `.` or `..`. Two archive names of one build must not only differ in case (`Audio` and `audio`), because they would be the same file on Windows.

## Archive extension

`extension` sets the file extension of this archive, the leading dot is optional (`pak` is the same as `.pak`). It is appended to the archive name:

{label="sfpak.meta"}
```
archive = audio;
extension = snd;    # produces audio.snd
```

- Without `extension` the archive gets the extension of the command line (`--extension`) or `.sfpak`.
- `--extension` on the command line (or `SetArchiveExtension` in code) wins over the meta file.
- The extension must not end with `.`, and it can only contain the characters of a meta value (letters, digits, `_`, `.`, `-` and Unicode characters).
- Two archives of one build must not get the same file name (`a` + `.b.c` and `a.b` + `.c`).

The meta file itself always ends with `.sfpak.meta`, also if the archive gets another extension.

## Excluding files

A file is excluded if its name **ends** with one of the `exclude` entries:

- The comparison is case insensitive (`.PNG` excludes `image.png`).
- Multi-part extensions work (`.tar.gz` excludes `data.tar.gz`, but not `data.gz`).
- A missing dot is added (`png` is the same as `.png`), so `png` does not exclude `image.xpng`.

## How it works

When the tool scans your source directories it looks for `.sfpak.meta` files. Every asset file in the same directory (or any subdirectory) is then packed into the archive named in that meta file. If a subdirectory has its own meta file, that one is used for the subdirectory. Excluded extensions are skipped entirely. Each directory can contain only one meta file.

```tree
assets/
    textures/
        textures.sfpak.meta     # archive = textures; exclude = .psd;
        background.png
        ui.png
    audio/
        audio.sfpak.meta        # archive = audio; extension = snd;
        music.ogg
        sfx.wav
    scripts/
        game.lua                # no meta file → goes into the default archive
```

Running the tool on `assets/` produces:

- `textures.sfpak`
- `audio.snd`
- `main.sfpak` (for everything without a meta file)

Asset paths inside the archive are always relative to the source folder, not to the meta file. `background.png` above is loaded as `textures/background.png`.

## Fallback

Files that are not covered by any meta file are packed into the archive name set with `--name` (default: `main`), with the extension set with `--extension` (default: `.sfpak`).

## Command line overrides

Values set on the command line win over the meta file:

- `--store-names` and `--compress` force the option on for **all** archives, even those whose meta file sets it to `false`. The meta value only takes effect when the CLI flag is not set.
- `--extension`, `--compress-block-size`, `--compress-table-size`, `--compress-min-match` and `--compress-min-saving` replace the meta file value for all archives. Without the CLI argument the meta value is used, without both the default.
