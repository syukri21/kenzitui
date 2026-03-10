#define _GNU_SOURCE
#include "task.h"
#include "tuiaction.h"
#include <glob.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
  mvprintw(row++, 2,
           "----------------------------------------------------------");
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
           "j/k: nav, SPACE: done, d: del, a: add, e: edit, p: priority, o: "
           "open nvim, /: search, c: clear, q: quit");
  attroff(A_DIM);
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

  int selected_id = (head != NULL) ? head->id : 0;

  TuiAction action;
  action.head = &head;
  action.selected_id = &selected_id;
  action.next_id = &next_id;
  action.search_query = search_query;
  action.max_search_len = MAX_TITLE;

  int ch = 0;
  while (ch != 'q') {
    clear();
    display_tasks(head, selected_id, search_query);
    refresh();

    action.ch = getch();
    execute(&action);
  }

  save_tasks_to_file(head, "tasks.dat");
  free_all_tasks(head);
  endwin();

  return EXIT_SUCCESS;
}
