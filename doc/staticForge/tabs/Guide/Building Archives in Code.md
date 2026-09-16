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
    std::cerr << builder.GetError() << std::endl;
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

## Fluent API

All setters return `*this` so they can be chained:

```cpp
StaticForge::StaticForgeBuilder builder{ "main", { "./assets" }, "./build" };
builder
    .SetCreateOutputDir(true)
    .SetDebugMode(true)
    .Build();
```

## Debug mode

`SetDebugMode(true)` prints detailed progress to `stdout` - the same output as `--verbose` in the CLI tool.
