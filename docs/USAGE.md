# Kenzitui Usage Guide

Kenzitui is a terminal-based task manager built with ncurses.

## Keyboard Shortcuts

- `j` or `DOWN`: Navigate down in the task list.
- `k` or `UP`: Navigate up in the task list.
- `SPACE`: Mark the selected task as done/undone.
- `a`: Add a new task.
- `e`: Edit the selected task.
- `d`: Delete the selected task.
- `p`: Cycle the priority of the selected task (LOW -> MEDIUM -> HIGH).
- `o`: Open the associated project path with `nvim`.
- `/`: Search tasks by title or project name.
- `c`: Clear the current search filter.
- `q`: Quit the application (saves tasks to `tasks.dat`).

## Priority Levels

Tasks can have one of three priority levels:
- **HIGH**: Displayed in RED.
- **MEDIUM**: Displayed in YELLOW.
- **LOW**: Displayed in GREEN.

## Project Paths

Each task can have a project name and a project path. When adding or editing a task, you will be prompted for both. Pressing `o` on a selected task will temporarily exit Kenzitui and open `nvim` at the specified path. Closing `nvim` will return you to Kenzitui.

## Data Persistence

Tasks are saved automatically to a file named `tasks.dat` when you quit the application. If the file exists, it will be loaded when you start Kenzitui.
