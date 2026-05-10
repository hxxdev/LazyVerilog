<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-04-12 | Updated: 2026-04-12 -->

# external

## Purpose
Git submodule dependencies. All directories here are vendored third-party libraries. **Do not modify files inside this directory** — changes will be lost on submodule updates and will not be tracked by the project.

## Subdirectories

| Directory | Purpose |
|-----------|---------|
| `slang/` | SystemVerilog compiler/analyzer library — used for SV parsing, AST, diagnostics |
| `LspCpp/` | C++ LSP framework — JSON-RPC transport and Language Server Protocol types |
| `auto-verilog/` | Submodule declared in `.gitmodules`; purpose TBD |

## For AI Agents

### Working In This Directory
- **Never edit files here.** If a submodule needs updating, use `git submodule update --remote external/<name>` and commit the updated submodule pointer.
- To initialize after a fresh clone: `git submodule update --init --recursive`
- If a build fails because a submodule directory is empty, run the init command above.
- Headers from these libraries are included in project source via angle brackets (e.g., `#include <slang/util/SmallVector.h>`, `#include <LibLsp/...>`).

### Key APIs (for reference when writing src/ code)

**slang** (`external/slang/include/slang/`):
- `syntax/` — `SyntaxTree`, `Parser`, `Lexer`
- `ast/` — `Compilation`, `Symbol`, `Type`
- `diagnostics/` — `DiagnosticEngine`, `Diagnostic`
- `util/` — `SmallVector`, `BumpAllocator`, `not_null`, `Span`
- `driver/` — `Driver` (high-level entry point for compiling SV files)

**LspCpp** (`external/LspCpp/include/LibLsp/`):
- `JsonRpc/` — `TcpServer`, `StreamMessageProducer`, JSON-RPC endpoint
- `lsp/general/` — `initialize`, `initialized`, `shutdown` request/response types
- `lsp/language/` — `textDocument/*` request types (completion, hover, definition, diagnostics)
- `lsp/workspace/` — workspace symbol and configuration types

### Testing Requirements
- These libraries are built as part of the CMake build — no separate test step needed.
- Verify submodule state: `git submodule status`

## Dependencies

### External
- All dependencies are self-contained within their respective submodule directories.

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
