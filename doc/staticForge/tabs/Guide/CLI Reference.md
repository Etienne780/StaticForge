---
id: node_mu3td1kuw60i4y16ior5l
name: CLI Reference
---

# CLI Reference

Quick reference for all `StaticForgeTool` arguments.

{style=outline}
[toc]

## Arguments

{hover=row; striped; col 0: width=160; col 1: width=100}
| Argument        | Short   | Description                                                                  | Type     | Default | Multi |
|-----------------|---------|------------------------------------------------------------------------------|----------|---------|-------|
| `--source`      | `--src` | Source directory of the files to be packed (can be specified multiple times) | filepath | —       | yes   |
| `--output`      | `--o`   | Output directory for packed archives or extracted files                      | filepath | —       | no    |
| `--name`        | `--n`   | Default archive name when no `.sfpak.meta` file is found                     | string   | `main`  | no    |
| `--extension`   | `--ext` | File extension of all archives (`pak` or `.pak`), overrides the meta files   | string   | `.sfpak`| no    |
| `--mkdir`       | `--p`   | Create the output directory if it does not exist                             | bool     | false   | no    |
| `--info`        | `--i`   | Display header and index info of an archive file (can be repeated)           | filepath | —       | yes   |
| `--extract`     | `--x`   | Extract archive file(s) into `<output>/<archive name>/` (can be repeated)    | filepath | —       | yes   |
| `--verify`      | `--vf`  | Check the checksums of all files in archive file(s) (can be repeated)        | filepath | —       | yes   |
| `--verbose`     | `--v`   | Enable debug/verbose console output                                          | bool     | false   | no    |
| `--store-names` | `--sn`  | Store original filenames in all archives for debug inspection and extraction | bool     | false   | no    |
| `--compress`    | `--c`   | Compress files larger than 4096 bytes before storing them in the archive     | bool     | false   | no    |
| `--help`        | `--h`   | Show the help message                                                        | —        | —       | no    |

Short forms always use two dashes (`--v`, not `-v`).

Arguments that can be repeated also accept a comma-separated list: `--source "./textures, ./audio"`.

`--info`, `--extract` and `--verify` accept archives with any extension, the content of the file decides if it is an archive.

## Archive extension

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --ext pak
```

`--extension` sets the extension of every archive of the build, also of the archives whose meta file sets its own `extension`. The leading dot is optional. An invalid extension (empty, contains `/`, `\`, `< > : " | ? *` or ends with `.` or a space) is an error:

{label="Output"}
```
[Error] Build failed: Invalid archive extension '.pak.' (must not be empty, contain '/', '\', '<', '>', ':', '"', '|', '?', '*' or end with '.' or ' ')
```

See [[packing-assets]] for the details.

## Compression settings

Only used together with compression (`--compress` or `compress = true;` in a meta file). They override the values of `.sfpak.meta` files.

{hover=row; striped; col 0: width=250; col 1: width=100}
| Argument                | Short   | Description                                                       | Default | Range            |
|-------------------------|---------|-------------------------------------------------------------------|---------|------------------|
| `--compress-block-size` | `--cbs` | Size in bytes of the blocks a file is split into for compression  | `65536` | 1024 - 67108864  |
| `--compress-table-size` | `--cts` | Match table size as power of two (entries = 2^value)              | `16`    | 8 - 24           |
| `--compress-min-match`  | `--cmm` | Shortest repeated sequence that is stored as a reference          | `8`     | 4 - 65535        |
| `--compress-min-saving` | `--cms` | Percent a block or file must get smaller, otherwise it is stored uncompressed | `5` | 0 - 99 |

Larger blocks compress better, because a block can only reference data inside of itself. Above 64 KB the gain is very small.

`--compress-min-saving` avoids compression that isn't worth the time it takes to decompress: a block is only stored compressed if it gets at least this many percent smaller, otherwise it is stored raw. The same applies to the whole file, a file that doesn't get small enough is stored completely uncompressed. With `0` every saved byte counts.

## Incremental build

{striped hover=row; col 0: width=140; col 1: width=80}
| Argument      | Short  | Description                                                     | Type     | Default |
|---------------|--------|-----------------------------------------------------------------|----------|---------|
| `--cache-dir` | `--cd` | Directory of the build cache, enables incremental builds        | filepath | —       |
| `--rebuild`   | `--rb` | Builds all archives, also the ones that are up to date          | bool     | false   |

With `--cache-dir` only the archives whose files or settings changed are built, see [[packing-assets]]. Without it every archive is built.

## Values

- **bool** arguments can be used without a value: `--verbose` is the same as `--verbose true`. Accepted values are `true`/`false`, `1`/`0`, `yes`/`no` and `y`/`n` (case insensitive). Anything else is an error, so a forgotten value like `--mkdir ./assets` is reported instead of silently being `false`.
- **int** arguments must be plain decimal numbers (`65536`, not `64k`).
- All other arguments require a value.
- Arguments and paths can contain any Unicode characters.
- Paths with spaces must be quoted: `--source "C:\Program Files\Game\assets"`. On Windows a path in quotes must not end with `\` (`"C:\assets\"`), because `\"` is read as a quote character.

## Modes

Exactly one mode must be active per invocation. Modes are determined by which arguments are provided:

{hover=row; col 0: hl}
| Mode    | Required arguments            | Optional arguments                                                          |
|---------|-------------------------------|-----------------------------------------------------------------------------|
| pack    | `--source` and `--output`     | `--name`, `--extension`, `--mkdir`, `--verbose`, `--store-names`, `--compress`, `--compress-*`, `--cache-dir`, `--rebuild` |
| extract | `--extract` and `--output`    | `--mkdir`, `--verbose`                                                      |
| info    | `--info`                      | `--verbose`                                                                 |
| verify  | `--verify`                    | `--verbose`                                                                 |
| help    | `--help`                      | —                                                                           |

Combining arguments from multiple modes (e.g. `--source` together with `--info`) is an error. Pack arguments like `--extension` can't be used with the other modes either.

## Exit code

The tool returns `0` on success and `1` on any error, so it can be used in build scripts.

Errors are printed as `[Error] <message>`. Several errors at once, e.g. more than one invalid argument, are listed below a short description:

{label="Output" indent=2}
```
[Error] Invalid arguments:
- Unknown argument 'foo'
- Invalid value '10k' for argument 'cbs'
```

## Command line priority

When `--store-names` or `--compress` is set on the command line, the option is forced on for every archive regardless of what the `.sfpak.meta` file specifies. The meta-file value is only used when the CLI flag is absent.

`--extension` and the `--compress-*` settings replace the meta file values for all archives when they are set.
