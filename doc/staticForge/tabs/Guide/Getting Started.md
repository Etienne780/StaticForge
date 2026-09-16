---
id: node_mu3td1kuufx8v3a1hzju
name: Getting Started
---

# Getting Started

StaticForge is an asset archive system for bundling and loading game files. It combines multiple files into a compact `.sfpak` container that can be efficiently read at runtime.

## What you get

- A command-line tool (`StaticForgeTool`) to pack your assets during your build process
- A C++ library (`StaticForgeCore`) to load assets from `.sfpak` files at runtime

## Typical workflow

1. Organize your asset folders
2. Optionally add `.sfpak.meta` files to control how folders are grouped into archives
3. Run `StaticForgeTool` to pack your assets into one or more `.sfpak` files
4. At runtime, use `StaticForgeReader` and `StaticForgeArchive` to load assets by path

---

> **Tip:** `.sfpak` files are binary and not meant to be edited manually. Always rebuild them from source assets using the tool.
