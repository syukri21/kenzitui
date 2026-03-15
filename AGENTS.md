# AGENTS.md

Guidance for coding agents working in this repository.

## Project Overview
- Name: `KENZITUI`
- Language: C (`-std=c11`)
- UI library: `ncurses`
- Purpose: terminal task manager with persistent storage in `tasks.dat`

## Repository Layout
- `src/main.c`: CLI entry (`fetch`) + ncurses app loop
- `src/lib/app_config.c`: runtime config loader (`.kenzitui.conf`)
- `src/lib/task.c`: task model + persistence (`tasks.dat`)
- `src/lib/tui_render.c`: rendering layer for ncurses UI
- `src/lib/tuiaction.c`: keyboard dispatcher only
- `src/lib/actions_nav.c`: navigation actions
- `src/lib/actions_task.c`: task actions (add/edit/delete/done/priority/move/open)
- `src/lib/actions_fetch.c`: fetch action
- `src/lib/input_ui.c`: user input/status helper functions
- `src/lib/phab_fetch.c`: Phabricator sprint fetch + parse
- `include/app_state.h`: central runtime state type for TUI mode
- `include/*.h`: public headers and shared types
- `docs/`: usage documentation
- `kenzitui-extension/`: Chrome extension helper subproject
- `bin/`: compiled executables
- `obj/`: object files

## Build and Run
- Build app: `make`
- Run app: `./bin/kenzitui`
- Clean artifacts: `make clean`
- Fetch sprint tasks: `./bin/kenzitui fetch --id <sprint_id>`
- Build should remain compatible on Linux and macOS (Darwin); keep Makefile ncurses detection paths working for both.

## Testing
- Existing test entry: `make test`
- Note: current `test` target runs `./bin/test_task` before compiling it. If it fails on a clean tree, compile first with:
  - `gcc -Wall -Wextra -Werror -Iinclude -std=c11 src/main_task_test.c src/lib/*.c -o bin/test_task -lncurses`

## Coding Rules
- Keep compatibility with GCC + C11.
- Preserve warning-free builds under `-Wall -Wextra -Werror`.
- Follow existing style in nearby files (function naming, spacing, enum usage).
- Prefer small, focused edits over broad refactors.
- Keep action modules separated by responsibility; avoid re-growing a monolithic `tuiaction.c`.
- Do not add new dependencies unless explicitly requested.

## Behavior and Data Safety
- Keep task persistence loader backward-compatible with old rows.
- Current `tasks.dat` format is:
  - `id,name,description,project,path,context_path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`
- Preserve empty CSV fields while loading (`...,,...`) to avoid shifting `phase/points/tags/ticket`.
- Keep board navigation behavior stable:
  - `h/l` move across phase columns.
  - `j/k` move within current phase column.
  - `m` moves phase forward, `M` moves phase backward.
- Keep phase header totals accurate: top-right `P:<sum>` should match points of visible tasks in each column.
- Keep autosave behavior on mutating actions (add/edit/delete/done/priority/phase move).
- Keep delete safety behavior: `d` must ask confirmation (`y/N`) before removal.
- Keep fetch merge behavior stable:
  - update/add fetched Phabricator tasks
  - keep local non-Phabricator tasks untouched
  - never overwrite local `path` from fetch
  - never overwrite local `context_path` from fetch
  - preserve local `is_done` and `priority` on existing tasks
- Keep fetch preview behavior stable:
  - CLI supports `fetch --id <id> --preview` without writing files
  - TUI `f` shows preview counts and requires confirmation before apply
- Keep open-path command execution shell-safe (quote/escape user-controlled strings).
- Keep open-path behavior stable:
  - expand `~` task path to `$HOME`
  - open nvim with task path as working directory
  - in tmux, reuse existing pane for same path instead of opening duplicate windows
- Keep open-context behavior stable:
  - key `O` opens `context_path` as file in nvim
  - if `context_path` empty/`-`, show status message
  - key `0` generates `context_path` when empty/`-`, persists it, seeds new/empty note from `ContextTemplate.md`, then opens file; if already set, open directly
- Keep backup behavior intact: saving tasks should refresh `tasks.dat.bak`.
- Keep restore command working (`restore --from <backup>` copies backup to `tasks.dat`).
- Avoid storing raw credentials in tracked files.
- Any change affecting keybindings, task file format, or TUI flows should update docs in `README.md` and/or `docs/USAGE.md`.
- Avoid breaking core actions: add/edit/delete/toggle/search/open path/open context/quit-save.

## Fetch Auth
- Cookie source precedence:
  1. `KENZITUI_PHAB_COOKIE` env var
  2. `PHAB_COOKIE` env var
  3. `.env` (`KENZITUI_PHAB_COOKIE=` or `PHAB_COOKIE=`)
- `env.example` is the template. `.env` should remain local/untracked.
- If cookie expires/fetch fails, refresh cookie from browser and rotate `.env` value.
- Owner username source precedence (no hardcoded owner in code):
  1. `KENZITUI_PHAB_USER`
  2. `PHAB_USER`
  3. `.env` (`KENZITUI_PHAB_USER=` or `PHAB_USER=`)
  4. fallback `USER`

## Obsidian Path Source
- Context generation (`0`) source precedence:
  1. `OBSIDIAN_PATH`
  2. `KENZITUI_OBSIDIAN_PATH`
  3. `.env` (`OBSIDIAN_PATH=` or `KENZITUI_OBSIDIAN_PATH=`)

## Context Template
- Template file for generated notes is `ContextTemplate.md` at repo root.
- Keep placeholder replacement behavior working for task-driven values (service/title/status/priority/points/tags/date/ticket).

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
