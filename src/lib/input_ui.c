#define _GNU_SOURCE

#include "tuiaction.h"
#include <glob.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

static void extract_completion_name(const char *path, char *out,
                                    size_t out_len) {
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

void tui_show_status_message(const char *message) {
  mvprintw(LINES - 2, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 2, 2, "%s", message != NULL ? message : "");
  refresh();
}

int tui_confirm_prompt(const char *prompt) {
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 1, 2, "%s", prompt != NULL ? prompt : "Confirm? (y/N): ");
  refresh();
  int ch = getch();
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  return (ch == 'y' || ch == 'Y');
}

void tui_input(const char *prompt, char *buffer, int max_len) {
  int pos = (int)strlen(buffer);
  int ch;
  curs_set(1);
  noecho();
  keypad(stdscr, TRUE);

  while (1) {
    mvprintw(LINES - 1, 0,
             "                                                                 "
             "               ");
    mvprintw(LINES - 1, 2, "%s%s", prompt, buffer);
    move(LINES - 1, 2 + (int)strlen(prompt) + pos);
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
  int pos = (int)strlen(buffer);
  int ch;
  curs_set(1);
  noecho();
  keypad(stdscr, TRUE);

  while (1) {
    mvprintw(LINES - 1, 0,
             "                                                                 "
             "               ");
    mvprintw(LINES - 1, 2, "%s%s", prompt, buffer);
    move(LINES - 1, 2 + (int)strlen(prompt) + pos);
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
        }
        globfree(&g);
      }
    } else if (ch >= 32 && ch <= 126 && pos < max_len - 1) {
      buffer[pos++] = (char)ch;
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

void open_search(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  tui_input("Search (Title/Project): ", action->state->search_query, MAX_TITLE);
}
