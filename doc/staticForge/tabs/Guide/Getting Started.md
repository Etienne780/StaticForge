---
id: node_mu3td1kuufx8v3a1hzju
name: Getting Started
---

# Getting Started

StaticForge is an asset archive system for bundling and loading game files. It combines multiple files into a compact archive file that can be efficiently read at runtime. Archives use the extension `.sfpak` by default, any other extension can be set when packing (see [[packing-assets]]).

## What you get

- A command-line tool (`StaticForgeTool`) to pack your assets during your build process, and to inspect or extract existing archives
- A C++ library (`StaticForgeCore`) to load assets from archives at runtime

## Typical workflow

1. Organize your asset folders
2. Optionally add `.sfpak.meta` files to control how folders are grouped into archives (see [[meta-files]])
3. Run `StaticForgeTool` to pack your assets into one or more archives (see [[packing-assets]])
4. At runtime, use `StaticForgeReader` and `StaticForgeArchive` to load assets by path (see [[loading-assets]])

{copy}
```bash
StaticForgeTool --source ./assets --output ./build --mkdir
```

{copy indent=4}
```cpp
#include <StaticForgeCore/StaticForge.h>

StaticForge::StaticForgeReader reader;
StaticForge::StaticForgeArchive archive;
if (reader.Load("./build/main.sfpak", &archive)) {
    std::vector<std::byte> data;
    if (!archive.LoadAsset("textures/background.png", &data)) {
        std::cerr << archive.TakeError() << std::endl;
    }
}
```

---

> **Tip:** Archives are binary and not meant to be edited manually. Always rebuild them from source assets using the tool.

> **Note:** The archive format is not meant for long-term storage. Archives are rebuilt together with your application, so the reader only accepts archives of its own format version (currently `3`).
