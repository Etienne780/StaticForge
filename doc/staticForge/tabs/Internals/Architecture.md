---
id: node_mu3td1ku1de5w2cswv1sn
name: Architecture
---

# Architecture

StaticForge is split into two main components.

## StaticForgeCore

The core static library. Contains all format logic and is linked by both the tool and any consuming application:

- Archive format definition and binary layout
- `StaticForgeBuilder` - writes `.sfpak` files
- `StaticForgeReader` - reads and validates `.sfpak` files
- `StaticForgeArchive` - runtime handle for an open archive
- `StaticForgeConcurrentArchive` - runtime handle that can be used from several threads at once
- `StaticForgeArchiveSet` / `StaticForgeConcurrentArchiveSet` - load from several archives as if they were one
- Meta file parsing (`StaticForgeMeta`)
- Archive core (`Internal/ArchiveCore`): the loaded archive data, shared between archive objects, and the asset loading logic
- Internal helpers: hashing, alignment, endian handling, compression, memory mapping, error support

Only the headers directly in `include/StaticForgeCore/` are part of the public API. The `Internal/` headers are not included in the SDK release packages.

## StaticForgeTool

The CLI executable. Wraps `StaticForgeCore` with argument parsing and five modes: pack, extract, info, verify, help. Does not contain any format logic itself.

`Utf8Console` makes the tool work with Unicode on Windows: it reads the command line arguments as UTF-8 and switches the console output to UTF-8 while the tool runs. On other platforms the arguments already are UTF-8.

## Dependency graph

```
StaticForgeTool
    └── StaticForgeCore
            ├── StaticForgeBuilder
            ├── StaticForgeReader
            ├── StaticForgeArchive
            ├── StaticForgeConcurrentArchive
            ├── StaticForgeArchiveSet / StaticForgeConcurrentArchiveSet
            └── Internal (archive core, set index, thread errors, helpers, meta, hashing, compression, mmap)

Your Game / Application
    └── StaticForgeCore
            ├── StaticForgeReader
            ├── StaticForgeArchive / StaticForgeConcurrentArchive
            └── StaticForgeArchiveSet / StaticForgeConcurrentArchiveSet (optional)
```

The tool-side classes (`StaticForgeBuilder`, meta parsing) are compiled into Core but an application that only reads archives still links the same library - there is currently no separate runtime-only build target.

## Platforms

Tested on Windows (MSVC) and Linux (GCC, Ubuntu). Archives are byte-identical across platforms: an archive packed on Windows can be loaded on Linux and the other way around.
