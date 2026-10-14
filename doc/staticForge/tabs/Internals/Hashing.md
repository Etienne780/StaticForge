---
id: node_mu3td1kucsyh3x1ldzws1
name: Hashing
---

# Hashing

StaticForge uses **FNV-1a** (Fowler–Noll–Vo) for two purposes: filename lookup (64 bit) and data integrity (32 bit).

## Filename hashing

When the builder packs a file, the relative path string (e.g. `textures/background.png`) is hashed with **64-bit FNV-1a** (offset basis `14695981039346656037`, prime `1099511628211`) and stored in the index as `hashName`.

At runtime, `StaticForgeArchive::LoadAsset` hashes the key string you pass in and looks it up in the index map. Unless the archive was built with `store-names`, the original string is not stored in the archive.

**Implications:**
- Lookups are O(1) via `std::unordered_map`.
- Without a name table the archive cannot list filenames - only validate whether a known key exists. `--info --verbose` and `--extract` then show the hash instead (16 hex digits).
- Hash collisions between two different paths of one build are detected at build time and cause an error. With 64 bit they are practically impossible (even with 100 000 files the probability is below one in a billion).

## Data checksum

During packing, each file's bytes as written to disk are fed through **32-bit FNV-1a** with the initial value `2166136261` to produce a `uint32_t` checksum. This is written into the index entry.

When loading, `StaticForgeArchive::LoadEntry` recomputes the checksum over the read bytes and compares it to the stored value. A mismatch causes `LoadAsset` to return `false` with an error message.

The checksum detects accidental damage. It is not a cryptographic signature and does not protect against intentional changes.

## Path format

Use a path **relative to the source root** used during packing. The path is UTF-8 and case sensitive, backslashes are converted to forward slashes before hashing.

```cpp
// Correct
archive.LoadAsset("textures/background.png", &data);

// Also valid
archive.LoadAsset("textures\\background.png", &data);

// Wrong - path is not relative to the source root
archive.LoadAsset("./assets/textures/background.png", &data);
```
