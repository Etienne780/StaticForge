# StaticForge

**StaticForge** is a simple asset archive system for bundling and loading game files.  
Its goal is to combine multiple files into a compact, structured container format (`.sfpak`) for efficient storage and fast runtime access.

The system is split into two main components: the command-line tool `StaticForgeTool` and the library `StaticForgeCore`.

---

## Requirements

- C++17 or later
- Windows (tested with MSVC)
- Linux (tested with GCC on Ubuntu)
- Other platforms may work but are currently untested

---

## StaticForgeTool (Executable)

The `StaticForgeTool` is a command-line application used to create and manage archive files.

**Responsibilities:**
- Creating `.sfpak` archive files from source folders
- Packing assets with automatic alignment and optional compression
- Incremental builds: only archives whose files or settings changed are built again
- Generating index and metadata
- Inspecting and debugging archives (view header and index info)
- Extracting archives back into normal files

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
if (!archive.LoadAsset("textures/background.png", &data)) {
    std::cerr << archive.TakeError() << std::endl;
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
            ├── StaticForgeConcurrentArchive (runtime handle for several threads)
            ├── StaticForgeArchiveSet / StaticForgeConcurrentArchiveSet (several archives as one)
            └── Internal helpers     (hashing, meta parsing, compression)

Your Game / Application
    └── StaticForgeCore
            ├── StaticForgeReader
            ├── StaticForgeArchive / StaticForgeConcurrentArchive
            └── StaticForgeArchiveSet / StaticForgeConcurrentArchiveSet (optional)
```

### Key Features

- **Fast lookups** - Filenames are stored as 64-bit FNV-1a hashes, enabling O(1) access via hash maps
- **Alignment-friendly** - Data blocks are padded to 4096 bytes for direct memory-mapped access
- **Checksum validation** - Each file's integrity is verified when loaded, or for the whole archive at once with `VerifyAll()` / `--verify`
- **Optional compression** - Block-wise LZ77 compression, decompressed automatically when loading. Data that doesn't compress well (e.g. JPG, OGG) is stored uncompressed automatically
- **Flexible loading** - Load into a vector or an existing buffer, load only a part of an asset, stream-based or memory-mapped
- **Multi-threading** - `StaticForgeConcurrentArchive` loads from several threads at once with a pool of file streams
- **Meta file support** - Split assets across multiple archives using `.sfpak.meta` files, and load them together with an archive set
- **Robust loading** - Damaged or truncated archives are rejected with an error instead of crashing
- **Unicode** - File names, paths and archive names can contain any Unicode characters
- **Cross-platform** - Written in C++17 with a little-endian format; archives are identical on Windows and Linux

### File Format (`.sfpak`)

| Region     | Size              | Description                                    |
|------------|-------------------|------------------------------------------------|
| Header     | 64 bytes          | Magic (`SFPK`), version, offsets               |
| Index      | N × 40 bytes      | Hash, offset, size, compressed size, padding, checksum |
| Data       | Variable          | File data, each padded to 4096 bytes           |
| Name table | Variable (optional) | Original file paths, only with `--store-names` |

The format is not backward compatible: the reader only loads archives of its own format version (currently `3`). Archives are meant to be rebuilt together with the application that uses them.

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
   if (!archive.LoadAsset("textures/hero.png", &textureData)) {
       std::cerr << archive.TakeError() << std::endl;
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

| Argument        | Description                                          | Default |
|-----------------|------------------------------------------------------|---------|
| `--source`      | Source directory/directories of files to pack        | —       |
| `--output`      | Output directory for archives or extracted files     | —       |
| `--name`        | Default archive name (no meta file)                  | `main`  |
| `--mkdir`       | Create output directory if missing                   | `false` |
| `--info`        | Display header/index info of an archive              | —       |
| `--extract`     | Extract archive(s) into `<output>/<archive name>/`   | —       |
| `--verify`      | Check archive(s) for damaged files                   | —       |
| `--verbose`     | Enable detailed console output                       | `false` |
| `--store-names` | Store original file paths in the archives            | `false` |
| `--compress`    | Compress files larger than 4096 bytes                | `false` |
| `--cache-dir`   | Build cache, only changed archives are built         | —       |
| `--rebuild`     | Build all archives, also the up to date ones         | `false` |
| `--help`        | Show help message                                    | —       |

Bool arguments can be used without a value (`--mkdir` is the same as `--mkdir true`). The compression can be tuned with `--compress-block-size`, `--compress-table-size`, `--compress-min-match` and `--compress-min-saving`. See the documentation for all details.

---

## Installation

Download the latest release and add the include directory to your include paths.
Link against the prebuilt x64 library provided in the release package.

Note: The release currently only includes prebuilt x64 libraries. Other architectures are not provided precompiled.

## Building & Tests

The build system uses [Premake5](https://premake.github.io/) via a `build.bat` script on Windows.

### Generating project files

```bat
build.bat vs2022       # Visual Studio 2022 (also opens in newer versions)
build.bat vs2019       # Visual Studio 2019
build.bat gmake        # GNU Makefiles
```

Run `build.bat help` to see all available targets.

### Compiling directly (Windows)

```bat
build.bat compile
```

This generates a Visual Studio solution and immediately builds it in Debug/x64 using `msbuild`. It uses the active Visual Studio developer environment, or finds the newest installed Visual Studio with C++ tools automatically (via `vswhere`).

### Linux

```bash
chmod +x vendor/premake5/premake5
./vendor/premake5/premake5 gmake
make config=release
```

The binaries are written to `build/bin/` and the library to `build/lib/`.

### Tests

The `StaticForgeTests` project is a console application that tests the library (builder, reader, archives, compression, meta files, multi-threading and damaged archives). It is built together with the other projects and runs in the CI on Windows and Linux.

```bash
StaticForgeTests                  # all normal tests, a few seconds
StaticForgeTests Archive Reader   # only tests whose name contains one of the filters
StaticForgeTests --stress         # also the slow stress tests (large files, many files, fuzzing)
StaticForgeTests --list           # list the tests
StaticForgeTests --keep           # keep the temporary files
```

The test files are written to a new folder in the system temp folder, which is removed at the end.

---

## Contributing

Contributions are welcome! Please open an issue or pull request for any improvements or bug fixes.
