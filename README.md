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
| `q`       | Quit and Save        |

Delete behavior:
- `d` requires confirmation (`y/N`) before the task is removed.

## 🚀 Getting Started

### 📋 Prerequisites
Install `libncurses-dev` (on Ubuntu/Debian):
```bash
sudo apt install libncurses-dev
```

### 🛠 Installation
1.  **Build**: Run `make` to compile.
2.  **Run**: Execute `./bin/kenzitui`.
3.  **Clean**: Run `make clean` to remove build artifacts.

### 🔄 Fetch Sprint Tasks (Phabricator)
Import sprint tasks directly into `tasks.dat`:
```bash
./bin/kenzitui fetch --id 3014
```

Auth cookie source (in order):
- `KENZITUI_PHAB_COOKIE` environment variable
- `PHAB_COOKIE` environment variable
- `.env` file with either:
  - `KENZITUI_PHAB_COOKIE=<cookie>`
  - `PHAB_COOKIE=<cookie>`

Quick setup:
```bash
cp env.example .env
# then fill PHAB_COOKIE in .env
```

Security notes for `.env`:
- Never commit real cookies (`PHAB_COOKIE` / `KENZITUI_PHAB_COOKIE`) to git.
- Keep `.env` local only and rotate cookie immediately if it was exposed.
- If fetch starts failing with auth errors, refresh cookie from browser, update `.env`, and retry.

## 💾 Task Data Format
`tasks.dat` rows use:
`id,name,description,project,path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

The loader remains backward-compatible with older 7-field task rows.
It also preserves empty CSV fields (`...,,...`) so `phase`, `points`, `tags`, and `ticket` stay mapped correctly.

## 📌 Fetch Notes
- Sprint fetch maps tasks by assigned owner and preserves phase placement (e.g. `Doing` vs `Backlog`).
- Points are read from the Phabricator workcard points tag and persisted into `tasks.dat`.
- Fetch uses merge mode: it updates/adds fetched Phabricator tasks, while keeping non-Phabricator local tasks untouched.
- Local task path is never overwritten by fetch (CLI or TUI); `path` remains local configuration.

## 📂 Project Structure

- `src/`: Core application logic and `main.c`.
- `src/lib/`: Implementation of tasks, TUI actions, and utilities.
- `include/`: Header files defining the data models and action systems.
- `bin/`: Compiled executables.
- `obj/`: Object files (.o).
- `Makefile`: Build system configuration.

## 🏗 Architecture (The TuiAction Pattern)

The project uses a centralized action system:

- **`Task`**: Doubly-linked list for task storage.
- **`TuiAction`**: A unified structure that captures user input and application state, processed by a central `execute()` function. This keeps the `main` loop clean and allows for easy expansion of features.
