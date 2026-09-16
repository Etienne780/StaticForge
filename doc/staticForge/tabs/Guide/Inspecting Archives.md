---
id: node_mu3td1ku1w2zp279b2ssx
name: Inspecting Archives
---

# Inspecting Archives

Use `--info` to display header and index information of one or more existing `.sfpak` files without extracting anything.

## Basic info

```bash
StaticForgeTool --info ./build/textures.sfpak
```

Output:

```
Archive: textures.sfpak
Version: 1
File count: 42
Index offset: 32
Index size: 1176
Data offset: 4096
```

## Inspecting multiple archives at once

`--info` can be repeated to inspect several archives in one invocation:

```bash
StaticForgeTool --info ./build/textures.sfpak --info ./build/audio.sfpak
```

Each archive is printed in sequence with a separator between them.

## Verbose index listing

Add `--verbose` to also print every index entry:

```bash
StaticForgeTool --info ./build/textures.sfpak --verbose
```

Output includes a line per file showing its hash, byte offset, size, and checksum:

```
Index entries:
  [0] hash: 12938471234  offset: 4096    size: 204800  checksum: 3421543
  [1] hash: 98712634521  offset: 208896  size: 81920   checksum: 9871234
  ...
```

## Name table

If the archive was built with `--store-names` (or `store-names = true` in the meta file), the inspector also prints the name table, showing the original relative path for each entry.

> **Note:** Without name storage the index only contains hashes. The original path is not recoverable from the archive alone — keep your source assets.
