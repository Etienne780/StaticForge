---
id: node_mu3td1ku1hsv9km18cfhyf
name: Meta File Parser
---

# Meta File Parser

Meta files use a minimal custom format parsed by `StaticForgeMeta` inside `StaticForgeCore`.

## Syntax

Each statement is a key-value pair terminated by a semicolon:

```
key = value;
```

Multiple statements can appear in any order:

```
archive = main;
exclude = .png, .jpg, .vert, .help;
store-names = true;
```

Comments start with `#` and run to the end of the line:

```
# this is a comment
archive = main;storenames 
```

## Tokenizer

`MetaTokenizer` reads the entire file into memory, then walks it character by character via `ProcessContent` → `ProcessChar`. It tracks the current line and column for error reporting and produces a flat `std::vector<MetaToken>`.

**Character dispatch (`ProcessChar`):**

| Input                                | Action                                       |
|--------------------------------------|----------------------------------------------|
| Whitespace (`\r`, `\n`, ` `, `\t`)  | Skipped; newlines increment the line counter |
| `=`                                  | Emit `Assign`                                |
| `,`                                  | Emit `Separator`                             |
| `;`                                  | Emit `Delimiter`                             |
| `#`                                  | Call `SkipComment` — consume until `\n`      |
| Alpha, `_`, `.`, `-`                 | Call `ReadIdentifier`                        |
| Anything else                        | Add error, emit `UNKNOWN`                    |

**`ReadIdentifier`:** records the start pointer, then advances `m_cur` while the character is alphanumeric, `_`, `.`, or `-`. The resulting `string_view` into the in-memory buffer is stored in the token without any heap allocation.

**Token types:**

| Token type   | Description                                                  |
|--------------|--------------------------------------------------------------|
| `Identifier` | An unquoted word — keys, values, extensions, booleans        |
| `Assign`     | The `=` character                                            |
| `Separator`  | The `,` character (used between list values like extensions) |
| `Delimiter`  | The `;` character — terminates a statement                   |
| `EndOfFile`  | Sentinel appended after the last character                   |
| `UNKNOWN`    | Unrecognised character; always accompanied by an error       |

Every token stores its **line** and **column** at the point it was emitted.

## Parser

`StaticForgeMeta::ParseTokens` consumes the token stream produced by `MetaTokenizer`. Each statement follows this pattern:

```
Identifier(key)  Assign  Identifier(value)  Delimiter
```

For list values (e.g. `exclude`) the value section is instead:

```
Identifier  Separator  Identifier  Separator  ...  Identifier  Delimiter
```

Any deviation — wrong token type, unexpected `EndOfFile`, missing `Delimiter` — is a parse error that includes the line and column of the offending token.

## Error suggestions

If an `Identifier` key is not recognised, the parser computes the edit distance to every known key. If the closest match differs by at most 3 characters a suggestion is appended to the error:

```
Unknown key 'archiv' at line 1, column 1. Did you mean 'archive'?
```

## Supported parameters

| Key          | Type     | Required | Description                                                     |
|--------------|----------|----------|-----------------------------------------------------------------|
| `archive`    | string   | yes      | Name for the `.sfpak` archive                                   |
| `exclude`    | string[] | no       | Comma-separated list of file extensions to skip                 |
| `storen-ames` | bool     | no       | Store original filenames in the archive for debug inspection    |
| `compress`   | bool     | no       | Compresses files larger than 4096 bytes before storing them in the archive.    |

## Value parsing

**`archive`** — the `Identifier` token value is used as-is.

**`exclude`** — after the `=`, the parser collects `Identifier` tokens separated by `Separator` (`,`) tokens until it sees a `Delimiter` (`;`). Each token value is trimmed and lowercased, so `.PNG` and `.png` are treated identically.

**`storenames`** — the single `Identifier` token after `=` is compared case-insensitively. `true` and `1` are truthy; anything else is false.