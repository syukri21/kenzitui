#define _GNU_SOURCE
#include "tuiaction.h"
#include <glob.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void tui_input(const char *prompt, char *buffer, int max_len) {
  echo();
  curs_set(1);
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             "); // Clear line
  mvprintw(LINES - 1, 2, "%s", prompt);
  getnstr(buffer, max_len - 1);
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             "); // Clear line
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
      snprintf(pattern, sizeof(pattern), "%s*", buffer);

      if (glob(pattern, GLOB_TILDE | GLOB_MARK, NULL, &g) == 0) {
        if (g.gl_pathc > 0) {
          // Find longest common prefix
          size_t prefix_len = strlen(g.gl_pathv[0]);
          for (size_t i = 1; i < g.gl_pathc; i++) {
            size_t j = 0;
            while (j < prefix_len && g.gl_pathv[i][j] == g.gl_pathv[0][j]) {
              j++;
            }
            prefix_len = j;
          }

          if (prefix_len < (size_t)max_len) {
            strncpy(buffer, g.gl_pathv[0], prefix_len);
            buffer[prefix_len] = '\0';
            pos = prefix_len;
          }

          if (g.gl_pathc > 1) {
            mvprintw(LINES - 2, 0,
                     "                                                         "
                     "                       ");
            int x = 2;
            for (size_t i = 0; i < g.gl_pathc && i < 6; i++) {
              char *name = strrchr(g.gl_pathv[i], '/');
              if (name && *(name + 1) != '\0')
                name++;
              else
                name = g.gl_pathv[i];

              mvprintw(LINES - 2, x, "%s ", name);
              x += strlen(name) + 2;
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

  case NAV_DOWN_KEY:
  case KEY_DOWN: {
    Task *current = *action->head;
    while (current != NULL && current->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL && current->next != NULL) {
      *action->selected_id = current->next->id;
    }
    break;
  }

  case NAV_UP_KEY:
  case KEY_UP: {
    if (*action->head == NULL || (*action->head)->id == *action->selected_id)
      break;
    Task *current = *action->head;
    while (current->next != NULL && current->next->id != *action->selected_id) {
      current = current->next;
    }
    if (current != NULL) {
      *action->selected_id = current->id;
    }
    break;
  }

  case DONE_KEY:
    mark_task_done(*action->head, *action->selected_id);
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
    break;
  }

  case ADD_KEY: {
    char title[MAX_TITLE];
    char desc[MAX_DESC];
    char project[MAX_PROJECT];
    char path[MAX_PATH] = "";
    tui_input("Task Title: ", title, MAX_TITLE);
    tui_input("Task Description: ", desc, MAX_DESC);
    tui_input("Project Name: ", project, MAX_PROJECT);
    tui_get_path_input("Project Path: ", path, MAX_PATH);
    Priority priority = tui_get_priority_input();
    add_task(action->head, create_task((*action->next_id)++, title, desc,
                                       project, path, priority));
    if (*action->selected_id == 0 && *action->head != NULL)
      *action->selected_id = (*action->head)->id;
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
      tui_input("New Title: ", current->title, MAX_TITLE);
      tui_input("New Description: ", current->description, MAX_DESC);
      tui_input("New Project Name: ", current->project, MAX_PROJECT);
      tui_get_path_input("New Project Path: ", current->path, MAX_PATH);
      current->priority = tui_get_priority_input();
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
