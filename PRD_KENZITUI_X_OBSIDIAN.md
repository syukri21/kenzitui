# PRD: KENZITUI x Obsidian Context File

## 1. Objective
Enable each Kenzitui task to be linked to an Obsidian note (“context file”) so users can jump from a task directly to its related working context.

## 2. Problem Statement
Current tasks only store `path` (project working directory). Users also need a persistent link to a knowledge/context note in Obsidian. Without this, task context is fragmented and requires manual navigation.

## 3. Goals
- Add a task-level `context_path` field.
- Auto-generate `context_path` from `OBSIDIAN_PATH` when empty.
- Make `context_path` editable with existing-value defaults.
- Open linked context quickly from TUI.
- Keep field local-only and stable across fetch operations.
- Preserve backward compatibility for existing `tasks.dat` files.

## 4. Non-Goals
- No direct Obsidian API integration.
- No sync with remote notes systems.
- No auto-generation of note content.
- No dependency on tmux/nvim for core correctness (they remain optional tooling).

## 5. User Stories
- As a user, I can set a context note path for a task.
- As a user, I can edit that context path later without retyping from scratch.
- As a user, I can open the context path with a dedicated keybinding.
- As a user, when I fetch from Phabricator, my local context links remain untouched.

## 6. Functional Requirements

### 6.1 Data Model
- Add `context_path` to `Task` struct.
- Extend persistence format with `context_path` as a new CSV column.
- Ensure loader supports both old rows (without column) and new rows.

### 6.2 Persistence Format
Current:
- `id,name,description,project,path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

New:
- `id,name,description,project,path,context_path,is_done,priority,phase,points,tags,ticket,next_sprint_meeting`

Rules:
- Keep CSV escaping behavior safe and consistent.
- Preserve empty fields and column alignment.

### 6.3 TUI Input & Edit
- Add prompt in add flow: `Context File Path: `.
- Add prompt in edit flow: `New Context File Path: `.
- Prompt should prefill existing value for edit.
- Empty value is allowed.
- If empty and `OBSIDIAN_PATH` is set, auto-generate value.

### 6.4 Auto-Generation Rules (OBSIDIAN_PATH)
- Source root from env: `OBSIDIAN_PATH`.
- Generate only when `context_path` is empty.
- Never overwrite a non-empty manual `context_path`.
- Path format:
  - `<OBSIDIAN_PATH>/<Subfolder>/<ticket>_<task_name_slug>.md`
- File name uses `_` (underscore), never spaces.
- Invalid file-name characters are removed/replaced (`/ \\ : * ? \" < > |`).
- Collapse repeated `_`, trim leading/trailing `_`, enforce length cap.
- Subfolder extraction:
  - If task name starts with bracket prefix like `[Ledger Service] ...`, use that prefix as folder.
  - Normalize folder by removing spaces/separators: `Ledger Service` -> `LedgerService`.
  - If no bracket prefix exists, fallback folder: `General`.
- Example:
  - Name: `[Ledger Service] Create Credit API`
  - Ticket: `T148272`
  - Output: `<OBSIDIAN_PATH>/LedgerService/T148272_Create_Credit_API.md`

### 6.5 Open Actions
- Keep existing `o` behavior for project `path`.
- Add new action (default key: `O`) to open `context_path`.
- Reuse existing path handling logic:
  - `~` expansion
  - shell-safe quoting
  - tmux behavior (reuse existing pane by same path, else create)
- If `context_path` is empty, show clear status message.

### 6.6 UI Rendering
- Show `context_path` in details pane.
- Optional compact card visibility via config (default: off or truncated view).

### 6.7 Fetch Merge Behavior
- `context_path` is local-only.
- Fetch must never overwrite existing `context_path`.
- New imported tasks:
  - if `OBSIDIAN_PATH` set: auto-generate by rules above
  - otherwise: keep empty

## 7. Configuration
- Add key config entry for context-open action (e.g. `key.open_context`).
- Document default keybind and override behavior in `.kenzitui.conf` docs.
- Add env configuration:
  - `OBSIDIAN_PATH=<absolute_or_tilde_path_to_vault_root>`

## 8. Error Handling
- Opening context path should provide explicit status when:
  - path empty
  - open command fails
  - tmux target resolution fails

## 9. Backward Compatibility
- Existing `tasks.dat` rows must still load correctly.
- Saving after load should write in the new format.
- No data loss for legacy fields.

## 10. Testing Requirements

### 10.1 Unit/Logic
- Load old format row -> `context_path` defaults to empty.
- Load/save new format row -> `context_path` round-trips.
- CSV escaping works with commas/quotes in `context_path`.
- Auto-generation uses `_` not spaces in file name.
- Bracket prefix maps to normalized subfolder (`[Ledger Service]` -> `LedgerService`).

### 10.2 Fetch Merge
- Existing task with local `context_path` keeps value after fetch update.
- Local-only tasks keep `context_path`.
- Imported tasks auto-generate `context_path` only when empty and `OBSIDIAN_PATH` is set.

### 10.3 Action Behavior
- `O` opens context path with same safety behavior as `o`.
- Empty context path shows user-facing message.

## 11. UX/Keybinding Proposal
- `o`: open project path (existing).
- `O`: open context path (new).

## 12. Rollout Plan
1. Implement data model + persistence changes.
2. Implement TUI prompts and rendering updates.
3. Implement context open action + keybinding.
4. Add/adjust tests.
5. Update docs: `README.md`, `CONTEXT.md`, `AGENTS.md`.

## 13. Acceptance Criteria
- User can add/edit/open `context_path` from TUI.
- `context_path` auto-generates with underscore naming and bracket-based subfolder when empty and `OBSIDIAN_PATH` exists.
- Fetch does not alter local `context_path`.
- Old `tasks.dat` remains loadable.
- Build/tests pass on Linux/macOS workflow.
