#define _GNU_SOURCE

#include "app_config.h"
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
      char suggest[1024];
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
        suggest[0] = '\0';
        if (g.gl_pathc > 0) {
          char first_name[MAX_PATH];
          extract_completion_name(g.gl_pathv[0], first_name,
                                  sizeof(first_name));

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
          // If common-prefix cannot advance, use first match so TAB still helps.
          if (prefix_len == min_prefix && g.gl_pathc > 0) {
            prefix_len = strlen(first_name);
          }

          if (dir_len + prefix_len < (size_t)max_len) {
            memcpy(buffer, dir_prefix, dir_len);
            strncpy(buffer + dir_len, first_name, prefix_len);
            buffer[dir_len + prefix_len] = '\0';
            pos = (int)(dir_len + prefix_len);
          }

          // Show candidate options on status line (compact single-line view).
          snprintf(suggest, sizeof(suggest), "Suggestions:");
          for (size_t i = 0; i < g.gl_pathc; i++) {
            char name[MAX_PATH];
            extract_completion_name(g.gl_pathv[i], name, sizeof(name));
            if (name[0] == '\0') {
              continue;
            }
            size_t used = strlen(suggest);
            size_t add = strlen(name) + 1;
            if (used + add + 4 >= sizeof(suggest)) {
              strncat(suggest, " ...", sizeof(suggest) - strlen(suggest) - 1);
              break;
            }
            strncat(suggest, " ", sizeof(suggest) - strlen(suggest) - 1);
            strncat(suggest, name, sizeof(suggest) - strlen(suggest) - 1);
          }
        } else {
          snprintf(suggest, sizeof(suggest), "No path match");
        }
        mvprintw(LINES - 2, 0,
                 "                                                                   "
                 "             ");
        mvprintw(LINES - 2, 2, "%s", suggest);
        refresh();
        globfree(&g);
      } else {
        mvprintw(LINES - 2, 0,
                 "                                                                   "
                 "             ");
        mvprintw(LINES - 2, 2, "No path match");
        refresh();
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

void tui_show_keybindings_help(void) {
  const AppConfig *cfg = app_config_get();
  clear();

  int w = 68;
  int h = 17;
  int y = (LINES - h) / 2;
  int x = (COLS - w) / 2;
  if (y < 0) {
    y = 0;
  }
  if (x < 0) {
    x = 0;
  }
  if (w > COLS) {
    w = COLS;
  }
  if (h > LINES) {
    h = LINES;
  }
  if (w < 10 || h < 6) {
    mvprintw(0, 0, "Terminal too small for help. Press any key.");
    refresh();
    (void)getch();
    return;
  }

  attron(COLOR_PAIR(4));
  mvhline(y, x + 1, ACS_HLINE, w - 2);
  mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
  mvvline(y + 1, x, ACS_VLINE, h - 2);
  mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
  mvaddch(y, x, ACS_ULCORNER);
  mvaddch(y, x + w - 1, ACS_URCORNER);
  mvaddch(y + h - 1, x, ACS_LLCORNER);
  mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(4));

  mvprintw(y + 1, x + 2, "KEYBINDINGS");
  mvprintw(y + 3, x + 2, "%c/%c move phase   %c/%c move task",
           cfg->keys.nav_left, cfg->keys.nav_right, cfg->keys.nav_down,
           cfg->keys.nav_up);
  mvprintw(y + 4, x + 2, "SPACE done         %c fetch          %c search",
           cfg->keys.fetch, cfg->keys.search);
  mvprintw(y + 5, x + 2,
           "%c add             %c edit           %c edit-path",
           cfg->keys.add, cfg->keys.edit, cfg->keys.edit_path);
  mvprintw(y + 6, x + 2, "%c delete          %c priority       %c next-phase",
           cfg->keys.del, cfg->keys.priority, cfg->keys.move_next_phase);
  mvprintw(y + 7, x + 2, "%c prev-phase      %c open project   %c open/gen context",
           cfg->keys.move_prev_phase, cfg->keys.open,
           cfg->keys.generate_context);
  mvprintw(y + 8, x + 2, "%c clear search    q quit/save",
           cfg->keys.clear_search);

  mvprintw(y + h - 2, x + 2, "Press any key to close");
  refresh();
  (void)getch();
}
