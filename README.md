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
- **Search**: Real-time filtering of tasks by title or project.
- **Persistence**: Automatic saving/loading to `tasks.dat`.
- **Integrated Editor**: Open task project paths directly in Neovim (with Tmux support).
- **Path Completion**: Tab-completion for file paths when adding/editing tasks.

## ⌨️ Keyboard Shortcuts

| Key       | Action               |
| --------- | -------------------- |
| `j` / `↓` | Navigate Down        |
| `k` / `↑` | Navigate Up          |
| `SPACE`   | Toggle Task Done     |
| `a`       | Add New Task         |
| `e`       | Edit Selected Task   |
| `d`       | Delete Selected Task |
| `p`       | Toggle Priority      |
| `/`       | Search Tasks         |
| `c`       | Clear Search         |
| `o`       | Open Path in Neovim  |
| `q`       | Quit and Save        |

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
