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

This produces `./build/textures.sfpak`.

## Multiple source folders

`--source` can be repeated to include multiple directories:

```bash
StaticForgeTool --source ./textures --source ./audio --output ./build
```
or
```bash
StaticForgeTool --source "./textures, ./audio" --output ./build
```

## Creating the output directory automatically

If the output directory doesn't exist yet, add `--mkdir`:

```bash
StaticForgeTool --source ./assets --output ./build/packed --mkdir true
```

## Verbose output

Add `--verbose` (or `-v`) to print detailed information about every file processed:

```bash
StaticForgeTool --source ./assets --output ./build --verbose
```
