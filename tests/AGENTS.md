<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-04-12 | Updated: 2026-04-12 -->

# tests

## Purpose
SystemVerilog source fixtures used for manual testing of the LSP server and Slang integration. These files serve as sample inputs when exercising the parser, diagnostics pipeline, or LSP handlers.

## Key Files

| File | Description |
|------|-------------|
| `test.sv` | 8-bit synchronous memory module: 256-entry register file with address/data_in/data_out ports and read/write control — a simple but complete SV module for parser smoke tests |

## For AI Agents

### Working In This Directory
- Add `.sv` files here as test cases for new LSP features (e.g., a file with intentional syntax errors to test diagnostics, a module hierarchy to test go-to-definition).
- Files here are **not** compiled by CMake — they are fed to the LSP server or Slang API directly.
- Use `test.sv` as the baseline smoke test when verifying Slang can parse a file without errors.

### Testing Requirements
- To parse with Slang CLI (if built): `./build/external/slang/driver/slang tests/test.sv`
- To test via LSP: open `tests/test.sv` in an editor configured to use `./build/main` as the SV language server.

### Common Patterns
- Prefer traditional Verilog port style (separate port list + direction declarations) to test older SV syntax compatibility.
- Include both well-formed and intentionally broken `.sv` files as the test suite grows.

## Dependencies

### External
- Parsed by `slang` (external submodule)

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
