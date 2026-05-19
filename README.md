# StaticForge

**StaticForge** is a simple asset archive system for bundling and loading game files.  
Its goal is to combine multiple files into a compact, structured container format (`.sfpak`) for efficient storage and fast runtime access.

The system is split into three main components:

---

## StaticForgeTool (Executable)

The `StaticForgeTool` is a command-line application used to create and manage archive files.

**Responsibilities:**
- Creating `.sfpak` archive files from source folders
- Adding files to an archive
- Packing assets with automatic alignment
- Generating index and metadata
- Inspecting and debugging archives (view header and index info)

This tool is typically used during the build process or in an asset pipeline.

**Basic usage:**
```bash
StaticForgeTool --source ./assets --output ./build
```

---

## StaticForgeCore (Static Library)

The `StaticForgeCore` is the central core library of the system.

**Responsibilities:**
- Definition and handling of the archive format
- Reading and writing container files
- Managing the file index (table of contents) with FNV-1a hashing
- Handling offsets, sizes, and metadata
- Runtime asset loading with checksum verification
- Providing a shared API for both tool and runtime

This component contains all format logic and is used by both the tool and the runtime system.

**Runtime usage example:**
```cpp
#include <StaticForgeCore/StaticForge.h>

StaticForge::StaticForgeArchive archive;
StaticForge::StaticForgeReader reader;

if (!reader.Load("./build/textures.sfpak", &archive)) {
    std::cerr << reader.GetError() << std::endl;
}

std::vector<std::byte> data;
if (!archive.LoadAsset("textures/background.png", data)) {
    std::cerr << archive.GetError() << std::endl;
}
```

---

## Architecture Overview

```
StaticForgeTool
    └── StaticForgeCore
            ├── StaticForgeBuilder   (pack archives)
            ├── StaticForgeReader    (read archives)
            ├── StaticForgeArchive   (runtime handle)
            └── Internal helpers     (hashing, meta parsing)

Your Game / Application
    └── StaticForgeCore
            ├── StaticForgeReader
            └── StaticForgeArchive
```

### Key Features

- **Fast lookups** - Filenames are stored as FNV-1a hashes, enabling O(1) access via hash maps
- **Alignment-friendly** - Data blocks are padded to 4096 bytes for direct memory-mapped access
- **Checksum validation** - Each file's integrity is verified when loaded
- **Meta file support** - Split assets across multiple archives using `.sfpak.meta` files
- **Move-only archive handle** - Safe ownership of file streams
- **Cross-platform** - Written in C++17 with little-endian format

### File Format (`.sfpak`)

| Region  | Size              | Description                         |
|---------|-------------------|-------------------------------------|
| Header  | 32 bytes (min)    | Magic (`SFPK`), version, offsets    |
| Index   | N × 32 bytes      | Hash, offset, size, padding, checksum |
| Data    | Variable          | Raw file data, each padded to 4096 bytes |

---

## Getting Started

1. **Pack your assets:**
   ```bash
   StaticForgeTool --source ./assets --output ./build
   ```

2. **In your game code:**
   ```cpp
   #include <StaticForgeCore/StaticForge.h>

   StaticForge::StaticForgeArchive archive;
   StaticForge::StaticForgeReader reader;
   
   if (!reader.Load("./build/main.sfpak", &archive)) {
       std::cerr << reader.GetError() << std::endl;
       return -1;
   }
   
   std::vector<std::byte> textureData;
   if (!archive.LoadAsset("textures/hero.png", textureData)) {
       std::cerr << archive.GetError() << std::endl;
   }
   ```

3. **Optional - Use meta files to split archives:**
   ```
   assets/
     textures/
       textures.sfpak.meta   # archive = textures;
       hero.png
     audio/
       audio.sfpak.meta      # archive = audio;
       music.ogg
     scripts/                # goes into default archive (main.sfpak)
       game.lua
   ```

---

## Command Line Reference

| Argument      | Description                                      | Default |
|---------------|--------------------------------------------------|---------|
| `--source`    | Source path(s) of files to pack                  | —       |
| `--output`    | Output directory for `.sfpak` file               | —       |
| `--name`      | Default archive name (no meta file)              | `main`  |
| `--mkdir`     | Create output directory if missing               | `false` |
| `--info`      | Display header/index info of an archive          | —       |
| `--verbose`   | Enable detailed console output                   | `false` |
| `--help`      | Show help message                                | —       |

---

## Requirements

- C++17 compiler
- Standard library with `<filesystem>` support

---

## Contributing

Contributions are welcome! Please open an issue or pull request for any improvements or bug fixes.
