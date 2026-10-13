---
id: node_mu3td1kurklac1g47o2b
name: Building Archives in Code
---

# Building Archives in Code

Instead of using the command-line tool you can drive the build process directly from C++ using `StaticForgeBuilder`.

## Minimal example

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

## Multiple source paths

```cpp
builder.SetSourcePath({
    "./assets/textures",
    "./assets/audio"
});
```

or add them one at a time:

```cpp
builder.AddSourcePath("./assets/textures");
builder.AddSourcePath("./assets/audio");
```

Paths are `std::filesystem::path`. For UTF-8 strings use `std::filesystem::u8path(...)`, so non-ASCII names also work on Windows.

## Fluent API

All setters return `*this` so they can be chained:

```cpp
StaticForge::StaticForgeBuilder builder{ "main", { "./assets" }, "./build" };
builder
    .SetCreateOutputDir(true)
    .SetDebugMode(true)
    .Build();
```

## Options

| Setter                                | CLI equivalent           | Description                                                           |
|---------------------------------------|--------------------------|-----------------------------------------------------------------------|
| `SetArchiveName(name)`                | `--name`                 | Archive name for files without a meta file (default `main`)           |
| `SetSourcePath(paths)` / `AddSourcePath(path)` | `--source`      | Source directories                                                    |
| `SetOutputPath(path)`                 | `--output`               | Output directory                                                      |
| `SetCreateOutputDir(bool)`            | `--mkdir`                | Create the output directory if it is missing                          |
| `SetDebugMode(bool)`                  | `--verbose`              | Print detailed progress to `stdout`                                   |
| `SetStoreNames(bool)`                 | `--store-names`          | Store file names in all archives                                      |
| `SetCompress(bool)`                   | `--compress`             | Compress files larger than 4096 bytes in all archives                 |
| `SetCompressionBlockSize(size)`       | `--compress-block-size`  | Compression block size in bytes (default `DEFAULT_COMPRESSION_BLOCK_SIZE`) |
| `SetCompressionTableSizeLog2(size)`   | `--compress-table-size`  | Match table size as power of two (default `DEFAULT_COMPRESSION_TABLE_SIZE_LOG2`) |
| `SetCompressionMinMatchLength(length)`| `--compress-min-match`   | Shortest match stored as reference (default `DEFAULT_COMPRESSION_MIN_MATCH_LENGTH`) |

The valid ranges of the compression settings are the `MIN_COMPRESSION_*` and `MAX_COMPRESSION_*` constants in `StaticForgeTypes.h`. Invalid values make `Build()` fail with an error. Compression settings that are set on the builder override the values of meta files, settings that are not set fall back to the meta file value and then to the default.

## Building again

The same builder can run `Build()` several times, e.g. after changing the source paths. Each build starts fresh and clears the errors of the previous build.

## Debug mode

`SetDebugMode(true)` prints detailed progress to `stdout` - the same output as `--verbose` in the CLI tool.
