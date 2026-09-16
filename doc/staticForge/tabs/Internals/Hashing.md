---
id: node_mu3td1kucsyh3x1ldzws1
name: Hashing
---

# Hashing

StaticForge uses **FNV-1a** (Fowler–Noll–Vo) for two purposes: filename lookup and data integrity.

## Filename hashing

When the builder packs a file, the relative path string (e.g. `textures/background.png`) is hashed to a `uint64_t` and stored in the index as `hashName`.

At runtime, `StaticForgeArchive::LoadAsset` hashes the key string you pass in and looks it up in the index map. The original string is never stored in the archive.

**Implications:**
- Lookups are O(1) via `std::unordered_map`.
- The archive cannot list filenames - only validate whether a known key exists.
- Hash collisions between two different paths in the same archive are detected at build time and cause an error.

## Data checksum

During packing, each file's raw bytes are fed through FNV-1a with the initial value `2166136261` to produce a `uint32_t` checksum. This is written back into the index entry after the data is flushed.

When loading, `StaticForgeArchive::LoadEntry` recomputes the checksum over the read bytes and compares it to the stored value. A mismatch causes `LoadAsset` to return `false` with an error message.

## Path format

Use a path **relative to the source root** used during packing.

```cpp
// Correct
archive.LoadAsset("textures/background.png", data);

// Also valid
archive.LoadAsset("textures\\background.png", data);

// Wrong - path is not relative to the source root
archive.LoadAsset("./assets/textures/background.png", data);
```