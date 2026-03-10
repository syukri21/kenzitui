#define _GNU_SOURCE
#include "task.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>

void display_tasks(Task *head, int selected_id, const char *search_query) {
  int row = 2;
  Task *current = head;

  attron(A_BOLD | COLOR_PAIR(1));
  mvprintw(0, 2, "--- KENZITUI TASK MANAGER ---");
  if (search_query != NULL && search_query[0] != '\0') {
    mvprintw(0, 35, "Search: %s", search_query);
  }
  attroff(A_BOLD | COLOR_PAIR(1));

  attron(A_DIM);
  mvprintw(row++, 2, "ID  | Status | Priority | Project    | Title");
  mvprintw(row++, 2, "----------------------------------------------------------");
  attroff(A_DIM);

  while (current != NULL) {
    if (search_query != NULL && search_query[0] != '\0' &&
        strcasestr(current->title, search_query) == NULL &&
        strcasestr(current->project, search_query) == NULL) {
      current = current->next;
      continue;
    }
    int color_pair = 5; // Default
    if (current->priority == HIGH)
      color_pair = 2;
    else if (current->priority == MEDIUM)
      color_pair = 3;
    else if (current->priority == LOW)
      color_pair = 4;

    if (current->id == selected_id) {
      attron(COLOR_PAIR(6) | A_BOLD);
      mvprintw(row, 0, ">");
      attroff(COLOR_PAIR(6) | A_BOLD);
    }

    if (current->is_done) {
      attron(A_DIM);
    }

    attron(COLOR_PAIR(color_pair));
    mvprintw(row++, 2, "%-3d | [%s]    | %-8s | %-10s | %s", current->id,
             current->is_done ? "x" : " ",
             current->priority == HIGH
                 ? "HIGH"
                 : (current->priority == MEDIUM ? "MEDIUM" : "LOW"),
             current->project, current->title);
    attroff(COLOR_PAIR(color_pair));

    if (current->is_done) {
      attroff(A_DIM);
    }

    current = current->next;
  }

  // Display description of selected task
  current = head;
  while (current != NULL && current->id != selected_id) {
    current = current->next;
  }
  if (current != NULL) {
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 7, 2, "Project: ");
    attroff(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 7, 11, "%s", current->project);

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 6, 2, "Path:    ");
    attroff(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 6, 11, "%s", current->path);

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 5, 2, "Description:");
    attroff(A_BOLD | COLOR_PAIR(1));
    mvprintw(LINES - 4, 2, "%s", current->description);
  }

  attron(A_DIM);
  mvprintw(LINES - 2, 2,
           "j/k: nav, SPACE: done, d: del, a: add, e: edit, p: priority, o: open nvim, /: search, c: clear, q: quit");
  attroff(A_DIM);
}

Priority get_priority_input() {
  mvprintw(LINES - 1, 0, "                                                                                ");
  mvprintw(LINES - 1, 2, "Select Priority (L: Low, M: Medium, H: High): ");
  int ch = getch();
  mvprintw(LINES - 1, 0, "                                                                                ");
  if (ch == 'h' || ch == 'H')
    return HIGH;
  if (ch == 'l' || ch == 'L')
    return LOW;
  return MEDIUM;
}

void get_input(const char *prompt, char *buffer, int max_len) {
  echo();
  curs_set(1);
  mvprintw(LINES - 1, 0, "                                                                                "); // Clear line
  mvprintw(LINES - 1, 2, "%s", prompt);
  getnstr(buffer, max_len - 1);
  mvprintw(LINES - 1, 0, "                                                                                "); // Clear line
  noecho();
  curs_set(0);
}

void get_path_input(const char *prompt, char *buffer, int max_len) {
  int pos = strlen(buffer);
  int ch;
  curs_set(1);
  noecho();
  keypad(stdscr, TRUE);

  while (1) {
    mvprintw(LINES - 1, 0, "                                                                                ");
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
            mvprintw(LINES - 2, 0, "                                                                                ");
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
            mvprintw(LINES - 2, 0, "                                                                                ");
          }
        }
        globfree(&g);
      }
    } else if (ch >= 32 && ch <= 126 && pos < max_len - 1) {
      buffer[pos++] = ch;
      buffer[pos] = '\0';
    }
  }

  mvprintw(LINES - 1, 0, "                                                                                ");
  mvprintw(LINES - 2, 0, "                                                                                ");
  curs_set(0);
}

int main() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);     // HIGH
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // MEDIUM
    init_pair(4, COLOR_GREEN, COLOR_BLACK);   // LOW
    init_pair(5, COLOR_WHITE, COLOR_BLACK);   // DEFAULT
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // SELECTION
  }

  Task *head = NULL;
  int next_id = 1;
  int last_id = 0;
  char search_query[MAX_TITLE] = "";
  head = load_tasks_from_file("tasks.dat", &last_id);
  if (last_id > 0)
    next_id = last_id + 1;

  if (head == NULL) {
    add_task(&head, create_task(next_id++, "Learn C", "Master the basics",
                                "General", ".", HIGH));
    add_task(&head, create_task(next_id++, "Build TUI", "Use ncurses", "Kenzitui",
                                ".", MEDIUM));
    add_task(&head, create_task(next_id++, "Add Persistence", "Save to file",
                                "Kenzitui", ".", LOW));
  }

  int ch = 0;
  int selected_id = (head != NULL) ? head->id : 0;
  while (ch != 'q') {
    clear();
    display_tasks(head, selected_id, search_query);
    refresh();
    ch = getch();

    switch (ch) {
    case '/': {
      get_input("Search (Title/Project): ", search_query, MAX_TITLE);
      break;
    }
    case 'c': {
      search_query[0] = '\0';
      break;
    }
    case 'o': {
      if (head == NULL || selected_id == 0)
        break;
      Task *current = head;
      while (current != NULL && current->id != selected_id) {
        current = current->next;
      }
      if (current != NULL && current->path[0] != '\0') {
        char command[MAX_PATH + 64];
        if (getenv("TMUX")) {
          snprintf(command, sizeof(command), "tmux new-window -n '%s' 'nvim %s'", 
                   current->project[0] != '\0' ? current->project : "Task", 
                   current->path);
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
    case 'a': {
      char title[MAX_TITLE];
      char desc[MAX_DESC];
      char project[MAX_PROJECT];
      char path[MAX_PATH] = "";
      get_input("Task Title: ", title, MAX_TITLE);
      get_input("Task Description: ", desc, MAX_DESC);
      get_input("Project Name: ", project, MAX_PROJECT);
      get_path_input("Project Path: ", path, MAX_PATH);
      Priority priority = get_priority_input();
      add_task(&head, create_task(next_id++, title, desc, project, path, priority));
      if (selected_id == 0 && head != NULL)
        selected_id = head->id;
      break;
    }
    case 'e': {
      if (head == NULL || selected_id == 0)
        break;
      Task *current = head;
      while (current != NULL && current->id != selected_id) {
        current = current->next;
      }
      if (current != NULL) {
        get_input("New Title: ", current->title, MAX_TITLE);
        get_input("New Description: ", current->description, MAX_DESC);
        get_input("New Project Name: ", current->project, MAX_PROJECT);
        get_path_input("New Project Path: ", current->path, MAX_PATH);
        current->priority = get_priority_input();
      }
      break;
    }
    case 'p': {
      if (head == NULL || selected_id == 0)
        break;
      Task *current = head;
      while (current != NULL && current->id != selected_id) {
        current = current->next;
      }
      if (current != NULL) {
        current->priority = (current->priority + 1) % 3;
      }
      break;
    }
    case 'j':
    case KEY_DOWN: {
      Task *current = head;
      while (current != NULL && current->id != selected_id) {
        current = current->next;
      }
      if (current != NULL && current->next != NULL) {
        selected_id = current->next->id;
      }
      break;
    }
    case 'k':
    case KEY_UP: {
      if (head == NULL)
        break;
      if (head->id == selected_id)
        break;
      Task *current = head;
      while (current->next != NULL && current->next->id != selected_id) {
        current = current->next;
      }
      if (current != NULL) {
        selected_id = current->id;
      }
      break;
    }
    case ' ':
      mark_task_done(head, selected_id);
      break;
    case 'd': {
      int old_id = selected_id;
      Task *current = head;
      Task *next_to_select = NULL;
      if (head != NULL && head->id == old_id) {
        next_to_select = head->next;
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
      delete_task(&head, old_id);
      if (next_to_select != NULL) {
        selected_id = next_to_select->id;
      } else if (head != NULL) {
        selected_id = head->id;
      } else {
        selected_id = 0;
      }
      break;
    }
    }
  }

  save_tasks_to_file(head, "tasks.dat");
  free_all_tasks(head);
  endwin();

  return EXIT_SUCCESS;
}
