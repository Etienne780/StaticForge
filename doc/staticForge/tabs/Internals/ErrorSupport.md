---
id: node_mu3td1ku72xkz714tsyqd
name: ErrorSupport
---

# ErrorSupport

`ErrorSupport` is the base class for `StaticForgeBuilder`, `StaticForgeReader`, and `StaticForgeArchive`.  
It provides a simple error collection system with optional formatted prefixes.

## API

```cpp
bool IsValid() const;            // true if no errors are stored
std::string GetError() const;    // returns all stored errors as a single string
std::string TakeError();         // returns all stored errors and clears them
void ClearErrors();              // clears all stored errors
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
[Error]: Failed to load asset, entry with key 'missing1' not found
>        Failed to load asset, entry with key 'missing2' not found
```

`HAS_HEADER` is a constant with the value `true` and is used by:

- `StaticForgeBuilder`
- `StaticForgeReader`
- `StaticForgeArchive`

Internal helper classes such as `StaticForgeMeta` disable this mode and emit raw messages without formatting.

## Error accumulation

Errors are accumulated in insertion order.

Each call to `AddError()` appends a new entry to the internal error list.  
`GetError()` combines all stored messages into a single newline-separated string.

## When errors are cleared

Errors are only cleared when they no longer apply:

| Event                          | Cleared errors                              |
|--------------------------------|---------------------------------------------|
| `StaticForgeBuilder::Build()`  | errors of the builder (previous build)      |
| `StaticForgeReader::Load()`    | errors of the reader and of the target archive |
| `StaticForgeMeta::Load()`      | errors of the meta parser                   |
| `TakeError()` / `ClearErrors()`| all errors of the object                    |

Errors of `StaticForgeArchive` calls like `LoadAsset` stay stored until they are taken or cleared, so several failed loads can be reported together.
