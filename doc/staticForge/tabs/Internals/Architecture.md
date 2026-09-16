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
- Meta file parsing (`StaticForgeMeta`)
- Internal helpers: hashing, alignment, endian handling, error support

## StaticForgeTool

The CLI executable. Wraps `StaticForgeCore` with argument parsing and three modes: pack, info, help. Does not contain any format logic itself.

## Dependency graph

```
StaticForgeTool
    └── StaticForgeCore
            ├── StaticForgeBuilder
            ├── StaticForgeReader
            ├── StaticForgeArchive
            └── Internal (helpers, meta, hashing)

Your Game / Application
    └── StaticForgeCore
            ├── StaticForgeReader
            └── StaticForgeArchive
```

The tool-side classes (`StaticForgeBuilder`, meta parsing) are compiled into Core but an application that only reads archives still links the same library - there is currently no separate runtime-only build target.
