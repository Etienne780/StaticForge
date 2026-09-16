---
id: node_mu3td1ku72xkz714tsyqd
name: ErrorSupport
---

# ErrorSupport

`ErrorSupport` is the base class for `StaticForgeBuilder`, `StaticForgeReader`, and `StaticForgeArchive`.  
It provides a simple error collection system with optional formatted prefixes.

## API

```cpp
bool IsValid() const;    // true if no errors were added
std::string GetError();  // returns all collected errors as a single string
```

## Header mode

The constructor takes a boolean `hasHeader`.

When enabled, the first error is prefixed with:

```txt
[Error]:
```

Additional errors are indented for readability.

Example:

```txt
[Error]: Failed to load archive
>        Failed to read header
>        Invalid magic value
```

`HAS_HEADER` is a constant with the value `true` and is used by:

- `StaticForgeBuilder`
- `StaticForgeReader`
- `StaticForgeArchive`

Internal helper classes such as `StaticForgeMeta` usually disable this mode and emit raw messages without formatting.

## Error accumulation

Errors are accumulated in insertion order.

Each call to `AddError()` appends a new entry to the internal error list.  
`GetError()` combines all stored messages into a single newline-separated string.