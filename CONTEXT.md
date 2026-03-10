# Kenzitui: Ncurses Task Manager (C Learning Project)

A terminal-based project and task management tool built with C and ncurses. This project serves as a hands-on journey to mastering C programming while building a functional, efficient TUI.

## 🎯 Project Goals
- **Functional**: Manage tasks with titles, descriptions, due dates, and priorities.
- **Persistent**: Save and load data from local files (JSON or custom binary format).
- **Aesthetic**: A clean, responsive ncurses interface with keyboard shortcuts.
- **Educational**: Deep dive into C memory management, pointers, and data structures.

## 🛠 Tech Stack
- **Language**: C (C11 standard)
- **UI Library**: `ncurses` (for the terminal interface)
- **Build System**: `Makefile`
- **LSP**: `clangd` (configured in Neovim)

## 📚 Learning Roadmap (C Milestones)
1. **The Basics**: Variables, types, control flow, and basic `stdio`.
2. **Memory Management**: Understanding the stack vs. heap, `malloc`, and `free`.
3. **Pointers & Arrays**: Mastering the "scary" parts of C.
4. **Structures (structs)**: Defining task and project data models.
5. **File I/O**: Persisting task data to the disk.
6. **Data Structures**: Implementing Linked Lists or Dynamic Arrays for task storage.
7. **Ncurses**: Handling windows, colors, and real-time user input.

## 🚀 Initial Feature Set
- [ ] Display a list of tasks.
- [ ] Add/Edit/Delete tasks.
- [ ] Mark tasks as complete.
- [ ] Basic categorization (Projects).
- [ ] Keyboard-driven navigation (Vim-like keys preferred).

## 🛠 Development Commands
- **Compile**: `make`
- **Run**: `./kenzitui`
- **Debug**: `gdb ./kenzitui`
