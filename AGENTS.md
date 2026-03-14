# AGENTS.md

Guidance for coding agents working in this repository.

## Project Overview
- Name: `KENZITUI`
- Language: C (`-std=c11`)
- UI library: `ncurses`
- Purpose: terminal task manager with persistent storage in `tasks.dat`

## Repository Layout
- `src/main.c`: CLI entry (`fetch`) + ncurses app loop
- `src/lib/task.c`: task model + persistence (`tasks.dat`)
- `src/lib/tui_render.c`: rendering layer for ncurses UI
- `src/lib/tuiaction.c`: keyboard actions and input flows
- `src/lib/phab_fetch.c`: Phabricator sprint fetch + parse
- `include/*.h`: public headers and shared types
- `docs/`: usage documentation
- `bin/`: compiled executables
- `obj/`: object files

## Build and Run
- Build app: `make`
- Run app: `./bin/kenzitui`
- Clean artifacts: `make clean`
- Fetch sprint tasks: `./bin/kenzitui fetch --id <sprint_id>`

## Testing
- Existing test entry: `make test`
- Note: current `test` target runs `./bin/test_task` before compiling it. If it fails on a clean tree, compile first with:
  - `gcc -Wall -Wextra -Werror -Iinclude -std=c11 src/main_task_test.c src/lib/*.c -o bin/test_task -lncurses`

## Coding Rules
- Keep compatibility with GCC + C11.
- Preserve warning-free builds under `-Wall -Wextra -Werror`.
- Follow existing style in nearby files (function naming, spacing, enum usage).
- Prefer small, focused edits over broad refactors.
- Do not add new dependencies unless explicitly requested.

## Behavior and Data Safety
- Keep task persistence loader backward-compatible with old rows.
- Current `tasks.dat` format is:
  - `id,name,description,project,path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`
- Preserve empty CSV fields while loading (`...,,...`) to avoid shifting `phase/points/tags/ticket`.
- Keep board navigation behavior stable:
  - `h/l` move across phase columns.
  - `j/k` move within current phase column.
  - `m` moves phase forward, `M` moves phase backward.
- Keep phase header totals accurate: top-right `P:<sum>` should match points of visible tasks in each column.
- Keep autosave behavior on mutating actions (add/edit/delete/done/priority/phase move).
- Avoid storing raw credentials in tracked files.
- Any change affecting keybindings, task file format, or TUI flows should update docs in `README.md` and/or `docs/USAGE.md`.
- Avoid breaking core actions: add/edit/delete/toggle/search/open path/quit-save.

## Fetch Auth
- Cookie source precedence:
  1. `KENZITUI_PHAB_COOKIE` env var
  2. `PHAB_COOKIE` env var
  3. `.env` (`KENZITUI_PHAB_COOKIE=` or `PHAB_COOKIE=`)
- `env.example` is the template. `.env` should remain local/untracked.

## Agent Workflow
1. Inspect related headers and implementation files before editing.
2. Apply minimal patch.
3. Rebuild with `make`.
4. Run relevant test command(s).
5. If changing fetch/parser behavior, verify with `./bin/kenzitui fetch --id <sprint_id>` and inspect `tasks.dat` for phase/points.
6. Summarize changes and mention any remaining risks.

## Out of Scope by Default
- Rewriting architecture.
- Renaming public structs/functions across the whole codebase.
- Breaking persistence backward compatibility.
- Adding external libraries.
