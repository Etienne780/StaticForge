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
archive = main; # this is also a comment
```

## Tokenizer

`MetaTokenizer` reads the entire file into memory (skipping a UTF-8 BOM), then walks it character by character via `ProcessContent` → `ProcessChar`. It tracks the current line and column for error reporting and produces a flat `std::vector<MetaToken>`. Columns count characters, not bytes, so multi-byte UTF-8 characters count as one column.

**Character dispatch (`ProcessChar`):**

| Input                                        | Action                                       |
|----------------------------------------------|----------------------------------------------|
| Whitespace (`\r`, `\n`, ` `, `\t`)           | Skipped; newlines increment the line counter |
| `=`                                          | Emit `Assign`                                |
| `,`                                          | Emit `Separator`                             |
| `;`                                          | Emit `Delimiter`                             |
| `#`                                          | Call `SkipComment` — consume until `\n`      |
| Letter, digit, `_`, `.`, `-`, UTF-8 byte     | Call `ReadIdentifier`                        |
| Anything else                                | Add error, emit `UNKNOWN`                    |

**`ReadIdentifier`:** records the start pointer, then advances `m_cur` while the character is an identifier character (`IsIdentifierChar`: alphanumeric, `_`, `.`, `-` or a byte ≥ `0x80`, which is part of a UTF-8 character). The resulting `string_view` into the in-memory buffer is stored in the token without any heap allocation.

**Token types:**

| Token type   | Description                                                  |
|--------------|--------------------------------------------------------------|
| `Identifier` | An unquoted word — keys, values, extensions, booleans, numbers |
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

Any deviation — wrong token type, unexpected `EndOfFile`, missing `Delimiter` — is a parse error. Errors about the statement structure include the line and column of the offending token.

## Error suggestions

If a key is not recognised, the parser compares it with every known key. Separators (`-`, `_`, space) and case are ignored, and the names are compared character by character. If the lengths differ by at most 3 and at most 3 characters are different, a suggestion is appended to the error:

```
Unknown identifier 'archiv'. Did you mean 'archive'?
```

## Supported parameters

| Key                   | Type     | Required | Description                                                                 |
|-----------------------|----------|----------|-----------------------------------------------------------------------------|
| `archive`             | string   | yes      | Name for the `.sfpak` archive                                               |
| `exclude`             | string[] | no       | Comma-separated list of file extensions to skip                             |
| `store-names`         | bool     | no       | Store original filenames in the archive for debug inspection and extraction |
| `compress`            | bool     | no       | Compress files larger than 4096 bytes                                       |
| `compress-block-size` | uint     | no       | Compression block size in bytes                                             |
| `compress-table-size` | uint     | no       | Compressor match table size as power of two                                 |
| `compress-min-match`  | uint     | no       | Shortest match the compressor stores as reference                           |
| `compress-min-saving` | uint     | no       | Percent a block or file must get smaller to be stored compressed            |

## Value parsing

**`archive`** — the `Identifier` token value is used as-is. The builder rejects names containing `/` or `\` and the names `.` and `..`.

**`exclude`** — after the `=`, the parser collects `Identifier` tokens separated by `Separator` (`,`) tokens until it sees a `Delimiter` (`;`). Each value is lowercased and a missing leading dot is added, so `.PNG`, `.png` and `png` are treated identically.

**`store-names`, `compress`** — the single `Identifier` token after `=` is compared case-insensitively. Only `true` and `false` are valid, anything else is a parse error.

**`compress-block-size`, `compress-table-size`, `compress-min-match`, `compress-min-saving`** — parsed with `std::from_chars` as an unsigned decimal number. The whole value has to be a number (`64k` or `-5` are errors). The builder checks the value against the valid range.
