# Kenzitui: Project Context

Kenzitui is a C11 + ncurses terminal task manager with persistence in `tasks.dat`.
It now supports direct sprint import from Phabricator (`fetch --id <sprint_id>`).

## Current Data Model
Each task stores:
- `id`
- `name`
- `description`
- `project`
- `path`
- `is_done`
- `priority`
- `phase`
- `points`
- `tags`
- `ticket`
- `next_sprint_meeting`

Persistence format (`tasks.dat`):
`id,name,description,project,path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

Loader is backward-compatible with legacy 7-field rows.

## Current Architecture
- `src/main.c`: app entry; handles CLI mode (`fetch`) or starts ncurses UI.
- `src/lib/app_config.c`: loads `.kenzitui.conf` overrides for keys/colors/compact fields.
- `src/lib/task.c`: task CRUD + file load/save logic.
- `src/lib/tuiaction.c`: dispatcher only (maps key -> action handler).
- `src/lib/actions_nav.c`: board-aware movement (`h/j/k/l`).
- `src/lib/actions_task.c`: task mutation actions (add/edit/delete/done/priority/move/open).
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

## Known-Critical Parsing Behaviors
- CSV loader preserves empty fields (`...,,...`) so column order remains stable.
- `phase` and `points` are loaded correctly from `tasks.dat` even when `tags` or `next_sprint_meeting` are empty.
- Fetch parser maps assigned tasks into the correct phase (including `Doing`) and fills `points` from workcard data.
- Fetch merge behavior keeps user-local configuration fields on existing tasks (`path`, `is_done`, `priority`) and refreshes Phabricator fields (`name`, `description`, `project`, `phase`, `points`, `tags`, `ticket`, `next_sprint_meeting`).

## Development Commands
- Build: `make`
- Run UI: `./bin/kenzitui`
- Fetch sprint: `./bin/kenzitui fetch --id <sprint_id>`
- Restore backup: `./bin/kenzitui restore --from tasks.dat.bak`
- Logic test: `gcc -Wall -Wextra -Werror -Iinclude -std=c11 src/main_task_test.c src/lib/*.c -o bin/test_task -lncurses && ./bin/test_task`

## Current Interaction Model
- `h/l`: move selection across phase columns.
- `j/k`: move selection within the current phase column.
- `m`: move selected task to next phase.
- `M`: move selected task to previous phase.
- `d`: delete requires confirmation (`y/N`).
- `f`: shows fetch preview (`fetched/updated/added/kept`) and asks confirmation before apply.
- Mutating actions persist immediately to `tasks.dat` (autosave), not only on quit.
- Save flow also maintains `tasks.dat.bak` backup before overwriting.

## Runtime State Model
- `AppState` is the single source of truth for interactive mode:
  - `head`, `selected_id`, `next_id`, `search_query`, `task_file`
- `TuiAction` now carries only `ch` and `AppState*`, making action handlers simpler and easier to test.
