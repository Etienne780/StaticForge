---
id: node_mu3td1kurklac1g47o2b
name: Building Archives in Code
---

# Building Archives in Code

Instead of using the command-line tool you can drive the build process directly from C++ using `StaticForgeBuilder`. The phases of a build are described in [[build-process]].

## Minimal example

{copy indent=4}
```cpp
#include <StaticForgeCore/StaticForge.h>

StaticForge::StaticForgeBuilder builder;
builder
    .SetArchiveName("textures")
    .AddSourcePath("./assets/textures")
    .SetOutputPath("./build")
    .SetCreateOutputDir(true);

if (!builder.Build()) {
    std::cerr << builder.TakeError() << std::endl;
}
```

At least one source path and an output path are required, `Build()` fails with an error otherwise.

## Multiple source paths

{copy indent=4}
```cpp
builder.SetSourcePath({
    "./assets/textures",
    "./assets/audio"
});
```

or add them one at a time:

{copy}
```cpp
builder.AddSourcePath("./assets/textures");
builder.AddSourcePath("./assets/audio");
```

Paths are `std::filesystem::path`. For UTF-8 strings use `std::filesystem::u8path(...)`, so non-ASCII names also work on Windows.

## Fluent API

All setters return `*this` so they can be chained:

{copy indent=4}
```cpp
StaticForge::StaticForgeBuilder builder{ "main", { "./assets" }, "./build" };
builder
    .SetCreateOutputDir(true)
    .SetDebugMode(true)
    .Build();
```

## Options

{hover=row; striped; col 0: width=300; col 1: width=225}
| Setter                                | CLI equivalent           | Description                                                           |
|---------------------------------------|--------------------------|-----------------------------------------------------------------------|
| `SetArchiveName(name)`                | `--name`                 | Archive name for files without a meta file (default `main`)           |
| `SetArchiveExtension(extension)`      | `--extension`            | File extension of all archives, overrides the meta files (default `PACK_FILE_EXTENSION`, `.sfpak`) |
| `SetSourcePath(paths)` / `AddSourcePath(path)` | `--source`      | Source directories                                                    |
| `SetOutputPath(path)`                 | `--output`               | Output directory                                                      |
| `SetCreateOutputDir(bool)`            | `--mkdir`                | Create the output directory if it is missing                          |
| `SetDebugMode(bool)`                  | `--verbose`              | Print detailed progress to `stdout`                                   |
| `SetStoreNames(bool)`                 | `--store-names`          | Store file names in all archives                                      |
| `SetCompress(bool)`                   | `--compress`             | Compress files larger than 4096 bytes in all archives                 |
| `SetCompressionBlockSize(size)`       | `--compress-block-size`  | Compression block size in bytes (default `DEFAULT_COMPRESSION_BLOCK_SIZE`) |
| `SetCompressionTableSizeLog2(size)`   | `--compress-table-size`  | Match table size as power of two (default `DEFAULT_COMPRESSION_TABLE_SIZE_LOG2`) |
| `SetCompressionMinMatchLength(length)`| `--compress-min-match`   | Shortest match stored as reference (default `DEFAULT_COMPRESSION_MIN_MATCH_LENGTH`) |
| `SetCompressionMinSaving(percent)`    | `--compress-min-saving`  | Minimum saving in percent, otherwise stored raw (default `DEFAULT_COMPRESSION_MIN_SAVING`) |
| `SetCacheDir(path)`                   | `--cache-dir`            | Directory of the build cache, enables incremental builds              |
| `SetRebuild(bool)`                    | `--rebuild`              | Builds all archives, also the ones that are up to date                |

The valid ranges of the compression settings are the `MIN_COMPRESSION_*` and `MAX_COMPRESSION_*` constants in `StaticForgeTypes.h`. Invalid values make `Build()` fail with an error. Compression settings that are set on the builder override the values of meta files, settings that are not set fall back to the meta file value and then to the default.

## Archive extension

`SetArchiveExtension` gives all archives another extension than `.sfpak`. The leading dot is optional:

{copy indent=4}
```cpp
StaticForge::StaticForgeBuilder builder{ "main", { "./assets" }, "./build" };
builder
    .SetCreateOutputDir(true)
    .SetArchiveExtension("pak");    // same as ".pak", produces ./build/main.pak

if (!builder.Build()) {
    // e.g. "Invalid archive extension '.pak.' (...)"
    std::cerr << builder.TakeError() << std::endl;
}
```

- The extension is checked when `Build()` runs. It must not be empty, contain `/`, `\` or `< > : " | ? *`, or end with `.` or a space.
- Like the compression settings it overrides the `extension` of the meta files. Without it the meta value is used, without both `PACK_FILE_EXTENSION` (`.sfpak`).
- `StaticForgeReader::Load` accepts archives with any extension, nothing has to be changed for loading.

See [[packing-assets]] and [[meta-files]] for the rules of the extension.

## Building again

The same builder can run `Build()` several times, e.g. after changing the source paths. Each build starts fresh and clears the errors of the previous build.

## Incremental builds

With `SetCacheDir(path)` only the archives whose files or settings changed are built (see [[packing-assets]]). After `Build()` the counts tell what happened:

{copy indent=4}
```cpp
builder.SetCacheDir("./build/intermediates/release/sfcache");
if (builder.Build()) {
    std::cout << builder.GetBuiltArchiveCount() << " built, "
              << builder.GetUpToDateArchiveCount() << " up to date, "
              << builder.GetRemovedArchiveCount() << " removed\n";
}
```

## Debug mode

`SetDebugMode(true)` prints detailed progress to `stdout` - the same output as `--verbose` in the CLI tool.
