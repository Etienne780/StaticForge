---
id: node_mu3td1ku1d4g35y4wv21q
name: Meta Files
---

# Meta Files

Meta files let you split assets from a single source tree into multiple `.sfpak` archives automatically.

## File format

A meta file has the extension `.sfpak.meta` and uses a simple semicolon-terminated key-value syntax:

```
archive = textures;
exclude = .png, .jpg;
storenames = true;
```

Each statement is a `key = value;` pair. Whitespace around `=` and `;` is ignored.

Comments start with `#` and run to the end of the line:

```
# pack all UI assets together
archive = ui;
exclude = .psd; # skip source files
```

## Supported parameters

| Key          | Type     | Required | Description                                                     |
|--------------|----------|----------|-----------------------------------------------------------------|
| `archive`    | string   | yes      | Name for the `.sfpak` archive produced from this directory      |
| `exclude`    | string[] | no       | Comma-separated list of file extensions to skip (e.g. `.png`)   |
| `store-names` | bool    | no       | Store original filenames in the archive for debug inspection    |
| `compress`   | bool     | no       | Compresses files larger than 4096 bytes before storing them in the archive.    |

Example with all parameters:

```
archive = main;
exclude = .png, .jpg, .vert, .help;
store-names = true;
compress = false;
```

## How it works

When the tool scans your source directories it looks for `.sfpak.meta` files. Every asset file in the same directory (or any subdirectory) is then packed into the archive named in that meta file. Excluded extensions are skipped entirely.

```
assets/
  textures/
    textures.sfpak.meta   <- archive = textures; exclude = .psd;
    background.png
    ui.png
  audio/
    audio.sfpak.meta      <- archive = audio;
    music.ogg
    sfx.wav
  scripts/
    game.lua              <- no meta file → goes into the default archive
```

Running the tool on `assets/` produces:
- `textures.sfpak`
- `audio.sfpak`
- `main.sfpak` (for everything without a meta file)

## Fallback

Files that are not covered by any meta file are packed into the archive name set with `--name` (default: `main`).

## storenames override

If `--store-names` is passed on the command line, name storage is enabled globally for **all** archives — even those whose meta file sets `store-names = false`. The meta value only takes effect when the CLI flag is not set.