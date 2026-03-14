#define _GNU_SOURCE
#include "phab_fetch.h"
#include "tuiaction.h"
#include <glob.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_NAV_TASKS 512

typedef struct NavColumns {
  Task *items[3][MAX_NAV_TASKS];
  int counts[3];
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
  for (int c = 0; c < 3; c++) {
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
  for (int c = 0; c < 3; c++) {
    if (cols->counts[c] > 0 && cols->items[c][0] != NULL) {
      *selected_id = cols->items[c][0]->id;
      return;
    }
  }
  *selected_id = 0;
}

static void nav_move_vertical(TuiAction *action, int delta_row) {
  if (action == NULL || action->head == NULL || action->selected_id == NULL) {
    return;
  }
  NavColumns cols;
  nav_build_columns(*action->head, action->search_query, &cols);

  int col = 0;
  int row = 0;
  if (!nav_find_selected(&cols, *action->selected_id, &col, &row)) {
    nav_select_first_available(&cols, action->selected_id);
    return;
  }

  int next_row = row + delta_row;
  if (next_row < 0 || next_row >= cols.counts[col]) {
    return;
  }
  if (cols.items[col][next_row] != NULL) {
    *action->selected_id = cols.items[col][next_row]->id;
  }
}

static void nav_move_horizontal(TuiAction *action, int delta_col) {
  if (action == NULL || action->head == NULL || action->selected_id == NULL) {
    return;
  }
  NavColumns cols;
  nav_build_columns(*action->head, action->search_query, &cols);

  int col = 0;
  int row = 0;
  if (!nav_find_selected(&cols, *action->selected_id, &col, &row)) {
    nav_select_first_available(&cols, action->selected_id);
    return;
  }

  int target = col + delta_col;
  if (target < 0 || target > 2 || cols.counts[target] <= 0) {
    return;
  }

  if (row >= cols.counts[target]) {
    row = cols.counts[target] - 1;
  }
  if (row < 0) {
    row = 0;
  }

  if (cols.items[target][row] != NULL) {
    *action->selected_id = cols.items[target][row]->id;
  }
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

static void persist_if_configured(TuiAction *action) {
  if (action == NULL || action->task_file == NULL || action->head == NULL) {
    return;
  }
  save_tasks_to_file(*action->head, action->task_file);
}

static void show_status_message(const char *message) {
  mvprintw(LINES - 2, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 2, 2, "%s", message != NULL ? message : "");
  refresh();
}

static void fetch_into_tui(TuiAction *action) {
  if (action == NULL || action->head == NULL || action->selected_id == NULL ||
      action->next_id == NULL) {
    return;
  }

  char sprint_buf[32] = "";
  tui_input("Fetch Sprint ID: ", sprint_buf, (int)sizeof(sprint_buf));
  int sprint_id = atoi(sprint_buf);
  if (sprint_id <= 0) {
    show_status_message("Invalid sprint ID.");
    return;
  }

  const char *task_file =
      (action->task_file != NULL && action->task_file[0] != '\0')
          ? action->task_file
          : "tasks.dat";

  show_status_message("Fetching sprint data...");
  int rc = fetch_sprint_tasks_to_file(sprint_id, task_file, ".env");
  if (rc != 0) {
    show_status_message("Fetch failed. Check cookie in .env.");
    return;
  }

  int last_id = 0;
  Task *loaded = load_tasks_from_file(task_file, &last_id);
  if (loaded == NULL) {
    show_status_message("Fetch done, but failed to reload tasks.dat.");
    return;
  }

  free_all_tasks(*action->head);
  *action->head = loaded;
  *action->selected_id = loaded->id;
  *action->next_id = (last_id > 0) ? (last_id + 1) : 1;
  show_status_message("Fetch success. Board updated.");
}

static void extract_completion_name(const char *path, char *out, size_t out_len) {
  size_t len = strlen(path);
  int is_dir = 0;
  while (len > 1 && path[len - 1] == '/') {
    is_dir = 1;
    len--;
  }

  size_t start = len;
  while (start > 0 && path[start - 1] != '/') {
    start--;
  }

  size_t name_len = len - start;
  if (name_len >= out_len) {
    name_len = out_len - 1;
  }
  memcpy(out, path + start, name_len);
  out[name_len] = '\0';

  if (is_dir && name_len + 1 < out_len) {
    out[name_len] = '/';
    out[name_len + 1] = '\0';
  }
}

void tui_input(const char *prompt, char *buffer, int max_len) {
  int pos = strlen(buffer);
  int ch;
  curs_set(1);
  noecho();
  keypad(stdscr, TRUE);

  while (1) {
    mvprintw(LINES - 1, 0,
             "                                                                 "
             "               ");
    mvprintw(LINES - 1, 2, "%s%s", prompt, buffer);
    move(LINES - 1, 2 + strlen(prompt) + pos);
    refresh();

    ch = getch();
    if (ch == '\n' || ch == '\r') {
      break;
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (pos > 0) {
        buffer[--pos] = '\0';
      }
    } else if (ch >= 32 && ch <= 126 && pos < max_len - 1) {
      buffer[pos++] = (char)ch;
      buffer[pos] = '\0';
    }
  }

  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  noecho();
  curs_set(0);
}

Priority tui_get_priority_input() {
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 1, 2, "Select Priority (L: Low, M: Medium, H: High): ");
  int ch = getch();
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  if (ch == 'h' || ch == 'H')
    return HIGH;
  if (ch == 'l' || ch == 'L')
    return LOW;
  return MEDIUM;
}

void tui_get_path_input(const char *prompt, char *buffer, int max_len) {
  int pos = strlen(buffer);
  int ch;
  curs_set(1);
  noecho();
  keypad(stdscr, TRUE);

  while (1) {
    mvprintw(LINES - 1, 0,
             "                                                                 "
             "               ");
    mvprintw(LINES - 1, 2, "%s%s", prompt, buffer);
    move(LINES - 1, 2 + strlen(prompt) + pos);
    refresh();

    ch = getch();

    if (ch == '\n' || ch == '\r') {
      break;
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (pos > 0) {
        buffer[--pos] = '\0';
      }
    } else if (ch == '\t') {
      glob_t g;
      char pattern[MAX_PATH + 2];
      char dir_prefix[MAX_PATH] = "";
      const char *current_token = buffer;
      char *last_slash = strrchr(buffer, '/');
      size_t dir_len = 0;

      if (last_slash != NULL) {
        dir_len = (size_t)(last_slash - buffer + 1);
        if (dir_len >= sizeof(dir_prefix)) {
          dir_len = sizeof(dir_prefix) - 1;
        }
        memcpy(dir_prefix, buffer, dir_len);
        dir_prefix[dir_len] = '\0';
        current_token = last_slash + 1;
      }

      snprintf(pattern, sizeof(pattern), "%s*", buffer);

      if (glob(pattern, GLOB_TILDE | GLOB_MARK, NULL, &g) == 0) {
        if (g.gl_pathc > 0) {
          char first_name[MAX_PATH];
          extract_completion_name(g.gl_pathv[0], first_name, sizeof(first_name));

          // Complete only the current segment after the last slash.
          size_t prefix_len = strlen(first_name);
          for (size_t i = 1; i < g.gl_pathc; i++) {
            char candidate_name[MAX_PATH];
            extract_completion_name(g.gl_pathv[i], candidate_name,
                                    sizeof(candidate_name));
            size_t j = 0;
            while (j < prefix_len && candidate_name[j] == first_name[j]) {
              j++;
            }
            prefix_len = j;
          }

          size_t min_prefix = strlen(current_token);
          if (prefix_len < min_prefix) {
            prefix_len = min_prefix;
          }

          if (dir_len + prefix_len < (size_t)max_len) {
            memcpy(buffer, dir_prefix, dir_len);
            strncpy(buffer + dir_len, first_name, prefix_len);
            buffer[dir_len + prefix_len] = '\0';
            pos = (int)(dir_len + prefix_len);
          }

          if (g.gl_pathc > 1) {
            mvprintw(LINES - 2, 0,
                     "                                                         "
                     "                       ");
            int x = 2;
            for (size_t i = 0; i < g.gl_pathc && i < 6; i++) {
              char display_name[MAX_PATH];
              extract_completion_name(g.gl_pathv[i], display_name,
                                      sizeof(display_name));

              mvprintw(LINES - 2, x, "%s ", display_name);
              x += strlen(display_name) + 2;
              if (x > COLS - 15)
                break;
            }
            if (g.gl_pathc > 6)
              mvprintw(LINES - 2, x, "...");
            refresh();
          } else {
            mvprintw(LINES - 2, 0,
                     "                                                         "
                     "                       ");
          }
        }
        globfree(&g);
      }
    } else if (ch >= 32 && ch <= 126 && pos < max_len - 1) {
      buffer[pos++] = ch;
      buffer[pos] = '\0';
    }
  }

  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 2, 0,
           "                                                                   "
           "             ");
  curs_set(0);
}

void execute(TuiAction *action) {
  switch (action->ch) {
  case SEARCH_KEY:
    open_search(action);
    break;

  case CLEAR_SEARCH_KEY:
    if (action->search_query)
      action->search_query[0] = '\0';
    break;

  case FETCH_KEY:
    fetch_into_tui(action);
    break;

  case NAV_DOWN_KEY:
  case KEY_DOWN: {
    nav_move_vertical(action, +1);
    break;
  }

  case NAV_UP_KEY:
  case KEY_UP: {
    nav_move_vertical(action, -1);
    break;
  }

  case NAV_LEFT_KEY:
  case KEY_LEFT: {
    nav_move_horizontal(action, -1);
    break;
  }

  case NAV_RIGHT_KEY:
  case KEY_RIGHT: {
    nav_move_horizontal(action, +1);
    break;
  }

  case DONE_KEY:
    mark_task_done(*action->head, *action->selected_id);
    persist_if_configured(action);
    break;

  case DELETE_KEY: {
    int old_id = *action->selected_id;
    Task *current = *action->head;
    Task *next_to_select = NULL;
    if (*action->head != NULL && (*action->head)->id == old_id) {
      next_to_select = (*action->head)->next;
    } else {
      while (current != NULL && current->next != NULL &&
             current->next->id != old_id) {
        current = current->next;
      }
      if (current != NULL && current->next != NULL) {
        next_to_select = current->next->next;
        if (next_to_select == NULL)
          next_to_select = current;
      }
    }
    delete_task(action->head, old_id);
    if (next_to_select != NULL) {
      *action->selected_id = next_to_select->id;
    } else if (*action->head != NULL) {
      *action->selected_id = (*action->head)->id;
    } else {
      *action->selected_id = 0;
    }
    persist_if_configured(action);
    break;
  }

  case ADD_KEY: {
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
    Task *task = create_task((*action->next_id)++, name, desc, project, path,
                             priority);
    if (task != NULL) {
      task_set_phab_fields(task, phase, atoi(points_buf), tags, ticket,
                           next_meeting);
      add_task(action->head, task);
    }
    if (*action->selected_id == 0 && *action->head != NULL)
      *action->selected_id = (*action->head)->id;
    persist_if_configured(action);
    break;
  }

  case EDIT_KEY: {
    if (*action->head == NULL || *action->selected_id == 0)
      break;
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL) {
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
      persist_if_configured(action);
    }
    break;
  }

  case PRIORITY_KEY: {
    if (*action->head == NULL || *action->selected_id == 0)
      break;
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL) {
      current->priority = (current->priority + 1) % 3;
      persist_if_configured(action);
    }
    break;
  }

  case MOVE_KEY: {
    if (*action->head == NULL || *action->selected_id == 0)
      break;
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL) {
      cycle_task_phase(current);
      persist_if_configured(action);
    }
    break;
  }

  case MOVE_BACK_KEY: {
    if (*action->head == NULL || *action->selected_id == 0)
      break;
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL) {
      cycle_task_phase_back(current);
      persist_if_configured(action);
    }
    break;
  }

  case OPEN_KEY: {
    if (*action->head == NULL || *action->selected_id == 0)
      break;
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL && current->path[0] != '\0') {
      char command[MAX_PATH + 128];
      const char *win_name =
          current->project[0] != '\0' ? current->project : "Task";

      if (getenv("TMUX")) {
        snprintf(command, sizeof(command),
                 "tmux select-window -t '%s' 2>/dev/null || tmux new-window "
                 "-n '%s' 'nvim %s'",
                 win_name, win_name, current->path);
        system(command);
      } else {
        def_prog_mode();
        endwin();
        printf("Not in tmux session. Opening nvim normally...\n");
        snprintf(command, sizeof(command), "nvim %s", current->path);
        system(command);
        reset_prog_mode();
        refresh();
      }
    }
    break;
  }
  }
}

void open_search(TuiAction *action) {
  tui_input("Search (Title/Project): ", action->search_query,
            action->max_search_len);
}
