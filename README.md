# Static Forge

Static Forge is a simple asset archive system for bundling and loading game files.  
Its goal is to combine multiple files into a compact, structured container format for efficient storage and fast runtime access.

The system is split into three main components:

---

## StaticForgeTool (Executable)

The `StaticForgeTool` is a command-line application used to create and manage archive files.

Responsibilities:
- Creating archive files
- Adding files to an archive
- Packing and compressing assets
- Generating index and metadata
- Inspecting and debugging archives

This tool is typically used during the build process or in an asset pipeline.

---

## StaticForgeCore (Static Library)

The `StaticForgeCore` is the central core library of the system.

Responsibilities:
- Definition and handling of the archive format
- Reading and writing container files
- Managing the file index (table of contents)
- Handling offsets, sizes, and metadata
- Optional compression and encryption support
- Providing a shared API for both tool and runtime

This component contains all format logic and is used by both the tool and the runtime system.

---

## StaticForgeRuntime (Static Library)

The `StaticForgeRuntime` is the runtime library used in games or applications.

Responsibilities:
- Read-only access to archive files
- Loading files from containers
- Streaming assets on demand
- Fast lookup using prebuilt index data
- Integration into game engines or applications

This component does not contain any packing or writing logic and is strictly used for reading and consuming prebuilt archives.

---

## Architecture Overview
