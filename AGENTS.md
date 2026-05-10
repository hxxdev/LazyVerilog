<!-- Generated: 2026-04-12 | Updated: 2026-04-12 -->

# LazyVerilog

## Purpose
A C++ project building a Language Server Protocol (LSP) server for SystemVerilog. It uses the Slang compiler library for parsing and semantic analysis, and LspCpp for the LSP transport layer. The project is in early/prototype stage — `src/main.cpp` is a working stub LSP message loop, and `slang-playground/` contains experiments with Slang's internal APIs.

## Key Files

| File | Description |
|------|-------------|
| `CMakeLists.txt` | CMake build config; defines two executables: `main` (LSP server) and `common` (playground), both linking `slang::slang` |
| `README.md` | Project rationale; explains choice of Slang based on sv-tests benchmark results |
| `.clangd` | clangd config: C++20, clang++ compiler, `-Wall` |
| `.clang-format` | LLVM style, 4-space indent, 100-char line limit |
| `.gitmodules` | Declares three submodules: `external/slang`, `external/LspCpp`, `external/auto-verilog` |
| `tags` | ctags index for code navigation (generated, do not edit) |

## Subdirectories

| Directory | Purpose |
|-----------|---------|
| `src/` | LSP server source code (see `src/AGENTS.md`) |
| `slang-playground/` | Standalone Slang API experiments (see `slang-playground/AGENTS.md`) |
| `tests/` | SystemVerilog test fixtures (see `tests/AGENTS.md`) |
| `external/` | Git submodule dependencies (see `external/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- Build system is CMake; build directory is `build/` (gitignored). To configure: `cmake -B build`, to build: `cmake --build build`.
- The project uses **C++20** — use modern features freely (ranges, concepts, structured bindings, etc.).
- `slang` must be found via `find_package(slang)` — it is provided by the `external/slang` submodule. Ensure submodules are initialized before building.
- Do not edit files inside `external/` — those are vendored submodules.
- `tags` is a ctags artifact; regenerate with `ctags -R src/ slang-playground/` if needed but do not commit it.

### Testing Requirements
- No formal test runner yet. Manual testing: run `./build/main` and pipe LSP-formatted messages to stdin.
- Use `tests/test.sv` as a sample SystemVerilog input when exercising the Slang parser.

### Common Patterns
- LSP messages follow the format: `Content-Length: <N>\r\n\r\n<JSON>`. See `src/main.cpp` for the read/write helpers.
- Slang utilities live under `slang/util/` (BumpAllocator, SmallVector, not_null, etc.).

## Dependencies

### External
- `slang` — SystemVerilog compiler/analyzer; top-ranked on the sv-tests benchmark
- `LspCpp` — C++ LSP framework (JSON-RPC transport, protocol types)
- `auto-verilog` — (submodule present, purpose TBD)

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
