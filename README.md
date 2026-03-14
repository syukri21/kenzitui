<div align="center">

# 🏯 KENZITUI
### ⚡ Fast, Lightweight, and Persistent TUI Task Manager

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Library](https://img.shields.io/badge/library-ncurses-green.svg)](https://invisible-island.net/ncurses/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)

Kenzitui is a terminal-based project and task management tool built with C and ncurses. It features a clean TUI, persistent storage, and Vim-like navigation.

[Features](#-features) • [Keyboard Shortcuts](#%EF%B8%8F-keyboard-shortcuts) • [Architecture](#-architecture) • [Getting Started](#-getting-started)

---
</div>

## 🛠 Features

- **Task Management**: Add, edit, delete, and mark tasks as complete.
- **Prioritization**: Assign **High**, **Medium**, or **Low** priority to tasks.
- **Project Organization**: Group tasks by project names.
- **Phabricator-Oriented Fields**: Track `phase`, `points`, `tags`, `ticket`, and `next sprint meeting`.
- **Compact Board UI**: Border-first compact columns (`Backlog`, `Doing`, `Need CR`) with ticket + point visibility on each card.
- **Phase Point Totals**: Each phase header shows total points (`P:<sum>`) at top-right.
- **Board Navigation**: Vim-style board movement (`h/l` across phases, `j/k` within a phase column).
- **Phase Move Hotkeys**: `m` moves selected task to next phase, `M` moves back to previous phase.
- **In-TUI Fetch**: Press `f` to fetch a sprint and refresh the board without leaving TUI.
- **Search**: Real-time filtering of tasks by title or project.
- **Persistence**: Automatic saving/loading to `tasks.dat` with immediate autosave on mutating actions.
- **Integrated Editor**: Open task project paths directly in Neovim (with Tmux support).
- **Task Context Link**: Each task has `context_path` (Obsidian note path) with optional auto-generation from `OBSIDIAN_PATH`.
- **Smart Open Reuse**: `o` reuses existing tmux pane when same project path is already open; otherwise opens a new tmux window at that path.
- **Path Completion**: Tab-completion for file paths when adding/editing tasks.

## ⌨️ Keyboard Shortcuts

| Key       | Action               |
| --------- | -------------------- |
| `h` / `←` | Move To Left Phase   |
| `l` / `→` | Move To Right Phase  |
| `j` / `↓` | Move Down In Phase   |
| `k` / `↑` | Move Up In Phase     |
| `SPACE`   | Toggle Task Done     |
| `a`       | Add New Task         |
| `e`       | Edit Selected Task   |
| `d`       | Delete Selected Task |
| `p`       | Toggle Priority      |
| `m`       | Move To Next Phase   |
| `M`       | Move To Prev Phase   |
| `f`       | Fetch Sprint In TUI  |
| `/`       | Search Tasks         |
| `c`       | Clear Search         |
| `o`       | Open Path in Neovim  |
| `O`       | Open Context File    |
| `0`       | Generate/Open Context |
| `q`       | Quit and Save        |

Delete behavior:
- `d` requires confirmation (`y/N`) before the task is removed.

Open behavior (`o`):
- Expands `~` / `~/...` task paths to `$HOME` before opening.
- Opens Neovim with the task path as working directory.
- In tmux, switches to an existing pane if that same path is already open; otherwise creates a new window.

Open context behavior (`O`):
- Opens `context_path` as a file in Neovim.
- Shows status message if `context_path` is empty.

Generate/open context behavior (`0`):
- If `context_path` is empty or `-`, Kenzitui generates it and saves immediately.
- Then it creates missing parent folders/file and opens the note in Neovim.
- If `context_path` already exists, it opens directly.

## 🚀 Getting Started

### 📋 Prerequisites
Install ncurses development libraries on your system.
Examples:
- Ubuntu/Debian: `sudo apt install libncurses-dev`
- macOS (Homebrew): `brew install ncurses`

### 🛠 Installation
1.  **Build**: Run `make` to compile.
2.  **Run**: Execute `./bin/kenzitui`.
3.  **Clean**: Run `make clean` to remove build artifacts.

Restore from backup:
```bash
./bin/kenzitui restore --from tasks.dat.bak
```

### 🔄 Fetch Sprint Tasks (Phabricator)
Import sprint tasks directly into `tasks.dat`:
```bash
./bin/kenzitui fetch --id 3014
```

Preview merge impact (dry-run, no file changes):
```bash
./bin/kenzitui fetch --id 3014 --preview
```

In TUI (`f`):
- shows preview counts first (`fetched/updated/added/kept`)
- asks confirmation before applying.

Auth cookie source (in order):
- `KENZITUI_PHAB_COOKIE` environment variable
- `PHAB_COOKIE` environment variable
- `.env` file with either:
  - `KENZITUI_PHAB_COOKIE=<cookie>`
  - `PHAB_COOKIE=<cookie>`

Owner username source (used for fetch filtering + board label):
- `KENZITUI_PHAB_USER` environment variable
- `PHAB_USER` environment variable
- `.env` file with either:
  - `KENZITUI_PHAB_USER=<username>`
  - `PHAB_USER=<username>`
- fallback: current shell `USER`

Quick setup:
```bash
cp env.example .env
# then fill PHAB_COOKIE in .env
# optional: set OBSIDIAN_PATH for context note auto-generation/open with key 0
```

Security notes for `.env`:
- Never commit real cookies (`PHAB_COOKIE` / `KENZITUI_PHAB_COOKIE`) to git.
- Keep `.env` local only and rotate cookie immediately if it was exposed.
- If fetch starts failing with auth errors, refresh cookie from browser, update `.env`, and retry.

## ⚙️ UI Config
You can override keybinds/colors/compact card fields using `.kenzitui.conf`.
Start from:
```bash
cp kenzitui.conf.example .kenzitui.conf
```

## 💽 Backup Safety
- Every save writes a backup file first: `tasks.dat.bak`.
- Use restore command to roll back quickly.

## 🧰 Troubleshooting Fetch
- `Fetch failed. Check cookie in .env.`:
  - cookie is expired or invalid; refresh it from browser login and replace value in `.env`.
- Preview works but apply canceled:
  - in TUI, press `y` at preview prompt to continue, any other key cancels safely.
- TUI fetch spinner running too long:
  - press `ESC` to cancel current fetch process and keep current board unchanged.

## 💾 Task Data Format
`tasks.dat` rows use:
`id,name,description,project,path,context_path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

The loader remains backward-compatible with older 7-field task rows.
It also preserves empty CSV fields (`...,,...`) so `phase`, `points`, `tags`, and `ticket` stay mapped correctly.

Context auto-generation:
- Obsidian root source order:
  - `OBSIDIAN_PATH` environment variable
  - `KENZITUI_OBSIDIAN_PATH` environment variable
  - `.env` (`OBSIDIAN_PATH=` or `KENZITUI_OBSIDIAN_PATH=`)
- If task `context_path` is empty and root path exists, Kenzitui generates:
  - `<OBSIDIAN_PATH>/<Subfolder>/<ticket>_<task_name_slug>.md`
- Bracket prefix folder example:
  - `[Ledger Service] Create Credit API` -> `LedgerService/T148272_Create_Credit_API.md`

## 📌 Fetch Notes
- Sprint fetch maps tasks by assigned owner and preserves phase placement (e.g. `Doing` vs `Backlog`).
- Points are read from the Phabricator workcard points tag and persisted into `tasks.dat`.
- Fetch uses merge mode: it updates/adds fetched Phabricator tasks, while keeping non-Phabricator local tasks untouched.
- Local task path/context are never overwritten by fetch (CLI or TUI); `path` and `context_path` remain local configuration.

## 📂 Project Structure

- `src/`: Core application logic and `main.c`.
- `src/lib/`: Implementation of tasks, TUI actions, and utilities.
  - `tuiaction.c`: action dispatcher only
  - `actions_nav.c`: board navigation actions
  - `actions_task.c`: add/edit/delete/done/priority/move/open actions (`o` project path, `O` context path)
  - `actions_fetch.c`: preview + async/cancel fetch action
  - `input_ui.c`: prompt/status/input helpers
- `include/`: Header files defining the data models and action systems.
- `kenzitui-extension/`: Chrome extension helper to generate `.env` (`PHAB_COOKIE`, `PHAB_USER`) from selected host cookies.
- `bin/`: Compiled executables.
- `obj/`: Object files (.o).
- `Makefile`: Build system configuration.

## 🏗 Architecture (The TuiAction Pattern)

The project uses a centralized action system:

- **`Task`**: Doubly-linked list for task storage.
- **`AppState`**: Central runtime state (`head`, `selected_id`, `next_id`, `search_query`, `task_file`) passed through actions.
- **`TuiAction`**: Lightweight event wrapper (`ch` + `AppState*`) processed by `execute()` dispatcher.
