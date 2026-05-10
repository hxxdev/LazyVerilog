<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-04-12 | Updated: 2026-04-12 -->

# src

## Purpose
Core LSP server implementation. Currently contains a single stub that establishes the LSP message loop — reading `Content-Length`-framed JSON-RPC messages from stdin and writing responses to stdout. This is the entry point for the language server that editors will launch as a subprocess.

## Key Files

| File | Description |
|------|-------------|
| `main.cpp` | LSP server entry point: `readLSPMessage()` parses headers and reads payload; `writeLSPMessage()` writes framed JSON; `main()` loops forever echoing a placeholder response |

## For AI Agents

### Working In This Directory
- The LSP stub currently ignores request content and always replies `{"jsonrpc":"2.0","id":1,"result":"Hello from LSP"}`. Real handlers need to dispatch on the `method` field of the parsed JSON.
- Next steps likely involve: integrating `LspCpp` for protocol parsing, hooking Slang for `textDocument/publishDiagnostics`, and implementing `initialize` / `shutdown` lifecycle.
- Use `slang::SourceManager` + `slang::CompilationUnit` to parse `.sv` files passed via LSP `textDocument/didOpen`.

### Testing Requirements
- Build: `cmake --build build --target main`
- Manual test: echo a valid LSP initialize message and pipe to `./build/main`
  ```bash
  printf 'Content-Length: 47\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize"}' | ./build/main
  ```

### Common Patterns
- LSP framing: `Content-Length: <N>\r\n\r\n<body>` — no other headers currently used.
- All I/O is synchronous on `std::cin` / `std::cout`; `std::cerr` for diagnostics.

## Dependencies

### Internal
- Links against `slang::slang` (via CMake target in root `CMakeLists.txt`)

### External
- `slang` — will be used for SV parsing and diagnostics
- `LspCpp` — intended for LSP protocol handling (not yet integrated in this file)

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
