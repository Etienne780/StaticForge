---
id: node_mu3td1kuw60i4y16ior5l
name: CLI Reference
---

# CLI Reference

Quick reference for all `StaticForgeTool` arguments.

## Arguments

| Argument        | Short   | Description                                                                | Type     | Default | Multi |
|-----------------|---------|----------------------------------------------------------------------------|----------|---------|-------|
| `--source`      | `--src` | Source path(s) of the files to be packed (can be specified multiple times) | filepath | —       | yes   |
| `--output`      | `--o`   | Output directory for the `.sfpak` file                                     | filepath | —       | no    |
| `--name`        | `--n`   | Default archive name when no `.sfpak.meta` file is found                   | string   | `main`  | no    |
| `--mkdir`       | `--p`   | Create the output directory if it does not exist                           | bool     | false   | no    |
| `--info`        | `--i`   | Display header and index info of a `.sfpak` file (can be repeated)         | filepath | —       | yes   |
| `--verbose`     | `--v`   | Enable debug/verbose console output                                        | bool     | false   | no    |
| `--store-names`  | `--sn`  | Store original filenames in all archives for debug inspection              | bool     | false   | no    |
| `--compress`  | `--c`  | Compresses files larger than 4096 bytes before storing them in the archive    | bool     | false   | no    |
| `--help`        | `--h`   | Show this help message                                                     | —        | —       | no    |

## Modes

Exactly one mode must be active per invocation. Modes are determined by which arguments are provided:

| Mode  | Required arguments              |
|-------|---------------------------------|
| pack  | `--source` and `--output`       |
| info  | `--info`                        |
| help  | `--help`                        |

Combining arguments from multiple modes (e.g. `--source` together with `--info`) is an error.

## storenames priority

When `--store-names` is set on the command line, 
name storage is forced on for every archive regardless of what the `.sfpak.meta` file specifies. 
The meta-file value of `store-names` is only used when the CLI flag is absent.
