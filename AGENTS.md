# AGENTS.md

Guidance for coding agents working in this repository.

## Project Overview
- Name: `KENZITUI`
- Language: C (`-std=c11`)
- UI library: `ncurses`
- Purpose: terminal task manager with persistent storage in `tasks.dat`

## Repository Layout
- `src/main.c`: application entry point and ncurses event loop
- `src/lib/*.c`: task model, TUI actions, utility logic
- `include/*.h`: public headers and shared types
- `docs/`: usage documentation
- `bin/`: compiled executables
- `obj/`: object files

## Build and Run
- Build app: `make`
- Run app: `./bin/kenzitui`
- Clean artifacts: `make clean`

## Testing
- Existing test entry: `make test`
- Note: current `test` target runs `./bin/test_task` before compiling it. If it fails on a clean tree, compile first with:
  - `gcc -Wall -Wextra -Werror -Iinclude -std=c11 src/main_task_test.c src/main.c src/lib/*.c -o bin/test_task -lncurses`

## Coding Rules
- Keep compatibility with GCC + C11.
- Preserve warning-free builds under `-Wall -Wextra -Werror`.
- Follow existing style in nearby files (function naming, spacing, enum usage).
- Prefer small, focused edits over broad refactors.
- Do not add new dependencies unless explicitly requested.

## Behavior and Data Safety
- Keep task persistence format backward-compatible unless a migration is explicitly requested.
- Any change affecting keybindings, task file format, or TUI flows should update docs in `README.md` and/or `docs/USAGE.md`.
- Avoid breaking core actions: add/edit/delete/toggle/search/open path/quit-save.

## Agent Workflow
1. Inspect related headers and implementation files before editing.
2. Apply minimal patch.
3. Rebuild with `make`.
4. Run relevant test command(s).
5. Summarize changes and mention any remaining risks.

## Out of Scope by Default
- Rewriting architecture.
- Renaming public structs/functions across the whole codebase.
- Changing persistence format.
- Adding external libraries.
