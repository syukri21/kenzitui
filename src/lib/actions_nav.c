#define _GNU_SOURCE

#include "tuiaction_actions.h"
#include <string.h>

#define MAX_NAV_TASKS 512

typedef struct NavColumns {
  Task *items[4][MAX_NAV_TASKS];
  int counts[4];
} NavColumns;

static int nav_phase_to_column(const char *phase) {
  if (phase == NULL) {
    return 0;
  }
  if (strcasestr(phase, "doing") != NULL) {
    return 1;
  }
  if (strcasestr(phase, "need") != NULL && strcasestr(phase, "cr") != NULL) {
    return 2;
  }
  if (strcasestr(phase, "done") != NULL) {
    return 3;
  }
  return 0;
}

static int nav_matches_search(const Task *task, const char *search_query) {
  if (task == NULL) {
    return 0;
  }
  if (search_query == NULL || search_query[0] == '\0') {
    return 1;
  }
  return strcasestr(task->name, search_query) != NULL ||
         strcasestr(task->project, search_query) != NULL ||
         strcasestr(task->ticket, search_query) != NULL ||
         strcasestr(task->tags, search_query) != NULL ||
         strcasestr(task->phase, search_query) != NULL;
}

static void nav_build_columns(Task *head, const char *search_query,
                              NavColumns *cols) {
  memset(cols, 0, sizeof(*cols));
  for (Task *current = head; current != NULL; current = current->next) {
    if (!nav_matches_search(current, search_query)) {
      continue;
    }
    int col = nav_phase_to_column(current->phase);
    int idx = cols->counts[col];
    if (idx < MAX_NAV_TASKS) {
      cols->items[col][idx] = current;
      cols->counts[col]++;
    }
  }
}

static int nav_find_selected(const NavColumns *cols, int selected_id, int *out_col,
                             int *out_row) {
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < cols->counts[c]; r++) {
      if (cols->items[c][r] != NULL && cols->items[c][r]->id == selected_id) {
        *out_col = c;
        *out_row = r;
        return 1;
      }
    }
  }
  return 0;
}

static void nav_select_first_available(const NavColumns *cols, int *selected_id) {
  for (int c = 0; c < 4; c++) {
    if (cols->counts[c] > 0 && cols->items[c][0] != NULL) {
      *selected_id = cols->items[c][0]->id;
      return;
    }
  }
  *selected_id = 0;
}

void tui_action_nav_vertical(TuiAction *action, int delta_row) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  NavColumns cols;
  nav_build_columns(action->state->head, action->state->search_query, &cols);

  int col = 0;
  int row = 0;
  if (!nav_find_selected(&cols, action->state->selected_id, &col, &row)) {
    nav_select_first_available(&cols, &action->state->selected_id);
    return;
  }

  int next_row = row + delta_row;
  if (next_row < 0 || next_row >= cols.counts[col]) {
    return;
  }
  if (cols.items[col][next_row] != NULL) {
    action->state->selected_id = cols.items[col][next_row]->id;
  }
}

void tui_action_nav_horizontal(TuiAction *action, int delta_col) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  NavColumns cols;
  nav_build_columns(action->state->head, action->state->search_query, &cols);

  int col = 0;
  int row = 0;
  if (!nav_find_selected(&cols, action->state->selected_id, &col, &row)) {
    nav_select_first_available(&cols, &action->state->selected_id);
    return;
  }

  int target = col + delta_col;
  if (target < 0 || target > 3 || cols.counts[target] <= 0) {
    return;
  }

  if (row >= cols.counts[target]) {
    row = cols.counts[target] - 1;
  }
  if (row < 0) {
    row = 0;
  }

  if (cols.items[target][row] != NULL) {
    action->state->selected_id = cols.items[target][row]->id;
  }
}
