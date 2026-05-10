<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-04-12 | Updated: 2026-04-12 -->

# slang-playground

## Purpose
Standalone experiments for learning and prototyping with the Slang C++ API before integrating functionality into the main LSP server. Each file compiles as its own executable (defined in root `CMakeLists.txt`) and explores specific Slang utilities or subsystems.

## Key Files

| File | Description |
|------|-------------|
| `common.cpp` | Demonstrates `SmallVector` (append/iterate), `BumpAllocator` (arena allocation via `.copy()`), and `not_null<T*>` pointer wrapper from `slang/util/` |

## For AI Agents

### Working In This Directory
- Each `.cpp` file here should be its own self-contained experiment with a `main()` function.
- To add a new experiment, create the `.cpp` file here and add a corresponding `add_executable` + `target_link_libraries` block in the root `CMakeLists.txt`.
- These files are **not** part of the production LSP server — treat them as a scratchpad. Keep them compiling but do not worry about production-quality error handling.
- `not_null<T*>` requires manual `delete` when used with raw `new` — it is a pointer wrapper, not a smart pointer.

### Testing Requirements
- Build: `cmake --build build --target common`
- Run: `./build/common` — output is printed to stdout, verify manually.

### Common Patterns
- Include headers from `slang/util/` for utility types; `slang/syntax/` and `slang/ast/` for parsing/analysis.
- `using namespace slang;` is acceptable in playground files.
- `BumpAllocator` is an arena: allocate freely, everything is freed when the allocator goes out of scope (except for raw `new` outside the allocator).

## Dependencies

### Internal
- `slang::slang` CMake target (links the full Slang library)

### External
- `slang/util/SmallVector.h` — stack-optimized growable array
- `slang/util/BumpAllocator.h` — arena allocator
- `slang/util/Util.h` — `not_null<T*>` and other utilities

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->
