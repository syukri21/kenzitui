# Kenzitui: Project Context

Kenzitui is a C11 + ncurses terminal task manager with persistence in `tasks.dat`.
It now supports direct sprint import from Phabricator (`fetch --id <sprint_id>`).
Repository also includes `kenzitui-extension/` (Chrome extension helper for `.env` generation).

## Current Data Model
Each task stores:
- `id`
- `name`
- `description`
- `project`
- `path`
- `context_path`
- `is_done`
- `priority`
- `phase`
- `points`
- `tags`
- `ticket`
- `next_sprint_meeting`

Persistence format (`tasks.dat`):
`id,name,description,project,path,context_path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

Loader is backward-compatible with legacy 7-field rows.

## Current Architecture
- `src/main.c`: app entry; handles CLI mode (`fetch`) or starts ncurses UI.
- `src/lib/app_config.c`: loads `.kenzitui.conf` overrides for keys/colors/compact fields.
- `src/lib/task.c`: task CRUD + file load/save logic.
- `src/lib/tuiaction.c`: dispatcher only (maps key -> action handler).
- `src/lib/actions_nav.c`: board-aware movement (`h/j/k/l`).
- `src/lib/actions_task.c`: task mutation actions (add/edit/delete/done/priority/move/open project/open context).
- `src/lib/actions_fetch.c`: fetch preview + async/cancel apply + reload.
- `src/lib/input_ui.c`: shared prompt/status/input UI helpers.
- `src/lib/tui_render.c`: compact border-based board renderer (`Backlog`, `Doing`, `Need CR`) with per-column point totals in header (`P:<sum>`).
- `src/lib/phab_fetch.c`: fetches and parses sprint workboard, then merges updates into `tasks.dat` (does not replace local-only tasks).

## Fetch Flow (Phabricator)
Command:
- `./bin/kenzitui fetch --id 3014`
- Preview only (no write): `./bin/kenzitui fetch --id 3014 --preview`
- In TUI: press `f`, input sprint ID.

Cookie source order:
1. `KENZITUI_PHAB_COOKIE` env var
2. `PHAB_COOKIE` env var
3. `.env` (`KENZITUI_PHAB_COOKIE=` or `PHAB_COOKIE=`)

Use `env.example` as template for `.env`.
Keep real cookies local and rotate them when expired or exposed.

Owner username source order (for fetch filtering / board label):
1. `KENZITUI_PHAB_USER` env var
2. `PHAB_USER` env var
3. `.env` (`KENZITUI_PHAB_USER=` or `PHAB_USER=`)
4. fallback `USER`

## Known-Critical Parsing Behaviors
- CSV loader preserves empty fields (`...,,...`) so column order remains stable.
- `phase` and `points` are loaded correctly from `tasks.dat` even when `tags` or `next_sprint_meeting` are empty.
- Fetch parser maps assigned tasks into the correct phase (including `Doing`) and fills `points` from workcard data.
- Fetch merge behavior keeps user-local configuration fields on existing tasks (`path`, `context_path`, `is_done`, `priority`) and refreshes Phabricator fields (`name`, `description`, `project`, `phase`, `points`, `tags`, `ticket`, `next_sprint_meeting`).
- For newly imported tasks, `context_path` auto-generates when `OBSIDIAN_PATH` is set.

## Development Commands
- Build: `make`
- Run UI: `./bin/kenzitui`
- Fetch sprint: `./bin/kenzitui fetch --id <sprint_id>`
- Restore backup: `./bin/kenzitui restore --from tasks.dat.bak`
- Logic test: `gcc -Wall -Wextra -Werror -Iinclude -std=c11 src/main_task_test.c src/lib/*.c -o bin/test_task -lncurses && ./bin/test_task`

Build portability:
- Supported targets: Linux and macOS (Darwin).
- Build links against system ncurses (Linux/macOS).

## Current Interaction Model
- `h/l`: move selection across phase columns.
- `j/k`: move selection within the current phase column.
- `m`: move selected task to next phase.
- `M`: move selected task to previous phase.
- `d`: delete requires confirmation (`y/N`).
- `f`: shows fetch preview (`fetched/updated/added/kept`) and asks confirmation before apply.
- `o`: expands `~` path to `$HOME` and opens Neovim at task path as working directory.
- `P`: edits selected task `path` only (project path quick edit).
- `O`: opens task `context_path` as a file in Neovim.
- `0`: if `context_path` is empty/`-`, generate then persist and open note file; if already set, open directly.
- `0` also seeds new/empty note file from `ContextTemplate.md` before opening.
- Path input TAB autocomplete shows candidate suggestions on status line.
- In tmux, `o` reuses an existing pane if the same path is already open; otherwise it creates a new tmux window.
- Mutating actions persist immediately to `tasks.dat` (autosave), not only on quit.
- Save flow also maintains `tasks.dat.bak` backup before overwriting.

Context root source order for generation (`0`):
1. `OBSIDIAN_PATH` env var
2. `KENZITUI_OBSIDIAN_PATH` env var
3. `.env` (`OBSIDIAN_PATH=` or `KENZITUI_OBSIDIAN_PATH=`)

Context note template behavior:
- Template file path: project root `ContextTemplate.md`.
- On `0`, if target context file is empty/new, template placeholders are replaced from selected task data.
- `workdir`/`projectPath` template placeholder uses selected task `path`; fallback is context file parent directory.

## Runtime State Model
- `AppState` is the single source of truth for interactive mode:
  - `head`, `selected_id`, `next_id`, `search_query`, `task_file`
- `TuiAction` now carries only `ch` and `AppState*`, making action handlers simpler and easier to test.
