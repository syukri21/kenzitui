#define _GNU_SOURCE

#include "kenzutls.h"
#include "tuiaction_actions.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void persist(AppState *state) {
  if (state == NULL || state->task_file == NULL) {
    return;
  }
  save_tasks_to_file(state->head, state->task_file);
}

static Task *find_selected(AppState *state) {
  if (state == NULL || state->selected_id == 0) {
    return NULL;
  }
  for (Task *current = state->head; current != NULL; current = current->next) {
    if (current->id == state->selected_id) {
      return current;
    }
  }
  return NULL;
}

static int resolve_open_path(const char *raw, char *out, size_t out_size) {
  if (raw == NULL || out == NULL || out_size == 0) {
    return 0;
  }
  if (raw[0] != '~') {
    return snprintf(out, out_size, "%s", raw) < (int)out_size;
  }

  const char *home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return snprintf(out, out_size, "%s", raw) < (int)out_size;
  }

  if (raw[1] == '\0') {
    return snprintf(out, out_size, "%s", home) < (int)out_size;
  }
  if (raw[1] == '/') {
    return snprintf(out, out_size, "%s%s", home, raw + 1) < (int)out_size;
  }

  // Keep ~user style unchanged; we only expand current-user "~" forms.
  return snprintf(out, out_size, "%s", raw) < (int)out_size;
}

static void build_tmux_window_name(const Task *task, char *out,
                                   size_t out_size) {
  if (task == NULL || out == NULL || out_size == 0) {
    return;
  }

  char base[MAX_TICKET];
  if (task->ticket[0] != '\0') {
    snprintf(base, sizeof(base), "%s", task->ticket);
  } else {
    snprintf(base, sizeof(base), "TASK%d", task->id);
  }

  size_t w = 0;
  for (size_t i = 0; base[i] != '\0' && w + 1 < out_size; i++) {
    unsigned char ch = (unsigned char)base[i];
    if (isalnum(ch) || ch == '-' || ch == '_') {
      out[w++] = (char)ch;
    } else {
      out[w++] = '_';
    }
  }
  if (w == 0) {
    snprintf(out, out_size, "TASK%d", task->id);
    return;
  }
  out[w] = '\0';
}

static int find_tmux_target_for_path(const char *path, char *out_window,
                                     size_t out_window_size, char *out_pane,
                                     size_t out_pane_size) {
  if (path == NULL || out_window == NULL || out_window_size == 0 ||
      out_pane == NULL || out_pane_size == 0) {
    return 0;
  }

  FILE *pipe = popen(
      "tmux list-panes -a -F '#{pane_id} #{session_name}:#{window_index} "
      "#{pane_current_path}'",
      "r");
  if (pipe == NULL) {
    return 0;
  }

  char line[2048];
  while (fgets(line, sizeof(line), pipe) != NULL) {
    char pane[64];
    char window[128];
    char pane_path[MAX_PATH];
    if (sscanf(line, "%63s %127s %255s", pane, window, pane_path) != 3) {
      continue;
    }
    if (strcmp(pane_path, path) == 0) {
      snprintf(out_window, out_window_size, "%s", window);
      snprintf(out_pane, out_pane_size, "%s", pane);
      pclose(pipe);
      return 1;
    }
  }

  pclose(pipe);
  return 0;
}

static void cycle_task_phase(Task *task) {
  if (task == NULL) {
    return;
  }
  if (strcasestr(task->phase, "doing") != NULL) {
    strncpy(task->phase, "Need CR", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "need") != NULL &&
      strcasestr(task->phase, "cr") != NULL) {
    strncpy(task->phase, "Backlog", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  strncpy(task->phase, "Doing", sizeof(task->phase) - 1);
  task->phase[sizeof(task->phase) - 1] = '\0';
}

static void cycle_task_phase_back(Task *task) {
  if (task == NULL) {
    return;
  }
  if (strcasestr(task->phase, "need") != NULL &&
      strcasestr(task->phase, "cr") != NULL) {
    strncpy(task->phase, "Doing", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "doing") != NULL) {
    strncpy(task->phase, "Backlog", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  strncpy(task->phase, "Need CR", sizeof(task->phase) - 1);
  task->phase[sizeof(task->phase) - 1] = '\0';
}

void tui_action_toggle_done(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  mark_task_done(action->state->head, action->state->selected_id);
  persist(action->state);
}

void tui_action_delete(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  if (!tui_confirm_prompt("Delete task? (y/N): ")) {
    tui_show_status_message("Delete canceled.");
    return;
  }

  int old_id = action->state->selected_id;
  Task *current = action->state->head;
  Task *next_to_select = NULL;
  if (action->state->head != NULL && action->state->head->id == old_id) {
    next_to_select = action->state->head->next;
  } else {
    while (current != NULL && current->next != NULL && current->next->id != old_id) {
      current = current->next;
    }
    if (current != NULL && current->next != NULL) {
      next_to_select = current->next->next;
      if (next_to_select == NULL) {
        next_to_select = current;
      }
    }
  }

  delete_task(&action->state->head, old_id);
  if (next_to_select != NULL) {
    action->state->selected_id = next_to_select->id;
  } else if (action->state->head != NULL) {
    action->state->selected_id = action->state->head->id;
  } else {
    action->state->selected_id = 0;
  }
  persist(action->state);
}

void tui_action_add(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }

  char name[MAX_NAME] = "";
  char desc[MAX_DESC] = "";
  char project[MAX_PROJECT] = "";
  char path[MAX_PATH] = "";
  char phase[MAX_PHASE] = "Backlog";
  char tags[MAX_TAGS] = "";
  char ticket[MAX_TICKET] = "";
  char next_meeting[MAX_NEXT_MEETING] = "";
  char points_buf[16] = "0";

  tui_input("Task Name: ", name, MAX_NAME);
  tui_input("Task Description: ", desc, MAX_DESC);
  tui_input("Project Name: ", project, MAX_PROJECT);
  tui_get_path_input("Project Path: ", path, MAX_PATH);
  tui_input("Phase: ", phase, MAX_PHASE);
  tui_input("Points: ", points_buf, (int)sizeof(points_buf));
  tui_input("Tags (| separated): ", tags, MAX_TAGS);
  tui_input("Ticket (e.g. T148277): ", ticket, MAX_TICKET);
  tui_input("Next Sprint Meeting: ", next_meeting, MAX_NEXT_MEETING);
  Priority priority = tui_get_priority_input();

  Task *task = create_task(action->state->next_id++, name, desc, project, path,
                           priority);
  if (task != NULL) {
    task_set_phab_fields(task, phase, atoi(points_buf), tags, ticket,
                         next_meeting);
    add_task(&action->state->head, task);
  }
  if (action->state->selected_id == 0 && action->state->head != NULL) {
    action->state->selected_id = action->state->head->id;
  }
  persist(action->state);
}

void tui_action_edit(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }

  char points_buf[16];
  snprintf(points_buf, sizeof(points_buf), "%d", current->points);
  tui_input("New Name: ", current->name, MAX_NAME);
  tui_input("New Description: ", current->description, MAX_DESC);
  tui_input("New Project Name: ", current->project, MAX_PROJECT);
  tui_get_path_input("New Project Path: ", current->path, MAX_PATH);
  tui_input("New Phase: ", current->phase, MAX_PHASE);
  tui_input("New Points: ", points_buf, (int)sizeof(points_buf));
  current->points = atoi(points_buf);
  tui_input("New Tags (| separated): ", current->tags, MAX_TAGS);
  tui_input("New Ticket: ", current->ticket, MAX_TICKET);
  tui_input("New Next Sprint Meeting: ", current->next_sprint_meeting,
            MAX_NEXT_MEETING);
  current->priority = tui_get_priority_input();
  persist(action->state);
}

void tui_action_cycle_priority(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  current->priority = (current->priority + 1) % 3;
  persist(action->state);
}

void tui_action_move_phase_next(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  cycle_task_phase(current);
  persist(action->state);
}

void tui_action_move_phase_prev(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  cycle_task_phase_back(current);
  persist(action->state);
}

void tui_action_open_path(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL || current->path[0] == '\0') {
    return;
  }

  char command[4096];
  char resolved_path[MAX_PATH];
  if (!resolve_open_path(current->path, resolved_path, sizeof(resolved_path))) {
    tui_show_status_message("Path too long.");
    return;
  }
  char q_path[(MAX_PATH * 5) + 8];
  if (!shell_quote_single(resolved_path, q_path, sizeof(q_path))) {
    tui_show_status_message("Path too long for shell escaping.");
    return;
  }

  if (getenv("TMUX")) {
    char window_name[64];
    char target_window[128];
    char target_pane[64];
    char q_target_window[(128 * 5) + 8];
    char q_target_pane[(64 * 5) + 8];
    char q_window_name[(64 * 5) + 8];
    if (find_tmux_target_for_path(resolved_path, target_window,
                                  sizeof(target_window), target_pane,
                                  sizeof(target_pane))) {
      if (!shell_quote_single(target_window, q_target_window,
                              sizeof(q_target_window)) ||
          !shell_quote_single(target_pane, q_target_pane,
                              sizeof(q_target_pane))) {
        tui_show_status_message("tmux target too long for shell escaping.");
        return;
      }

      snprintf(command, sizeof(command),
               "tmux select-window -t %s && tmux select-pane -t %s", q_target_window,
               q_target_pane);
    } else {
      build_tmux_window_name(current, window_name, sizeof(window_name));
      if (!shell_quote_single(window_name, q_window_name,
                              sizeof(q_window_name))) {
        tui_show_status_message("Window name too long for shell escaping.");
        return;
      }

      snprintf(command, sizeof(command), "tmux new-window -c %s -n %s 'nvim .'",
               q_path, q_window_name);
    }
    if (system(command) != 0) {
      tui_show_status_message("Failed to open tmux window for task path.");
    }
  } else {
    def_prog_mode();
    endwin();
    printf("Not in tmux session. Opening nvim normally...\n");
    snprintf(command, sizeof(command), "cd -- %s && nvim .", q_path);
    if (system(command) != 0) {
      tui_show_status_message("Failed to open nvim for task path.");
    }
    reset_prog_mode();
    refresh();
  }
}
