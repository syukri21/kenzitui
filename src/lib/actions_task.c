#define _GNU_SOURCE

#include "kenzutls.h"
#include "tuiaction_actions.h"
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

  char command[2048];
  const char *win_name =
      current->project[0] != '\0' ? current->project : "Task";
  char q_path[(MAX_PATH * 5) + 8];
  char q_win[(MAX_PROJECT * 5) + 8];
  if (!shell_quote_single(current->path, q_path, sizeof(q_path)) ||
      !shell_quote_single(win_name, q_win, sizeof(q_win))) {
    tui_show_status_message("Path/window name too long for shell escaping.");
    return;
  }

  if (getenv("TMUX")) {
    snprintf(command, sizeof(command),
             "tmux select-window -t %s 2>/dev/null || tmux new-window -n %s "
             "nvim -- %s",
             q_win, q_win, q_path);
    system(command);
  } else {
    def_prog_mode();
    endwin();
    printf("Not in tmux session. Opening nvim normally...\n");
    snprintf(command, sizeof(command), "nvim -- %s", q_path);
    system(command);
    reset_prog_mode();
    refresh();
  }
}
