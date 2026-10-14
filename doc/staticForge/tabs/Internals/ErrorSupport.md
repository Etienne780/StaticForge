---
id: node_mu3td1ku72xkz714tsyqd
name: ErrorSupport
---

# ErrorSupport

`ErrorSupport` is the base class for `StaticForgeBuilder`, `StaticForgeReader`, `StaticForgeArchive` and `StaticForgeArchiveSet`.  
It provides a simple error collection system.

## API

```cpp
bool IsValid() const;            // true if no errors are stored
std::string GetError() const;    // returns all stored errors as a single string
std::string TakeError();         // returns all stored errors and clears them
void ClearErrors();              // clears all stored errors
```

## Format

`GetError()` returns one message per line:

```txt
Failed to load asset, entry with key 'missing1' not found
Failed to load asset, entry with key 'missing2' not found
```

Formatting is left to the application. The CLI tool, for example, prints a single error as `[Error] <message>` and several errors as a list. Paths in messages always use `/` as separator.

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
