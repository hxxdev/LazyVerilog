<!-- Parent: ../AGENTS.md -->
# demo

Manual testing fixtures for live Neovim plugin development.

## Files

| File | Purpose |
|------|---------|
| `memory.sv` | Sub-module: single-port SRAM — used to test autoinst, autowire, hover |
| `memory_top.sv` | Top-level instantiating `memory` — primary demo file for interactive testing |
| `params.svh` | Shared parameter/define header included by demo modules |
| `vcode.f` | `.f` filelist pointing to demo SV files (loaded via `lazyverilog.toml` `[design].vcode`) |

## Rules
- These files exist for **manual** Neovim testing, not automated pytest runs
- Edit freely to reproduce bugs or test new features; commit intentional changes only
- `lazyverilog.toml` at repo root references `vcode.f` here for extra-file compilation
