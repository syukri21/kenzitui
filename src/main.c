#define _GNU_SOURCE
#include "task.h"
#include "tuiaction.h"
#include <glob.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *priority_label(Priority priority) {
  if (priority == HIGH)
    return "HIGH";
  if (priority == MEDIUM)
    return "MED";
  return "LOW";
}

static int priority_color(Priority priority) {
  if (priority == HIGH)
    return 2;
  if (priority == MEDIUM)
    return 3;
  return 4;
}

static Task *find_task_by_id(Task *head, int id) {
  Task *current = head;
  while (current != NULL) {
    if (current->id == id)
      return current;
    current = current->next;
  }
  return NULL;
}

static int count_visible_tasks(Task *head, const char *search_query) {
  int count = 0;
  Task *current = head;
  while (current != NULL) {
    if (!(search_query != NULL && search_query[0] != '\0' &&
          strcasestr(current->title, search_query) == NULL &&
          strcasestr(current->project, search_query) == NULL)) {
      count++;
    }
    current = current->next;
  }
  return count;
}

void display_tasks(Task *head, int selected_id, const char *search_query) {
  if (LINES < 16 || COLS < 72) {
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(1, 2, "Terminal too small for modern layout.");
    attroff(A_BOLD | COLOR_PAIR(2));
    attron(A_DIM | COLOR_PAIR(5));
    mvprintw(3, 2, "Resize to at least 72x16.");
    attroff(A_DIM | COLOR_PAIR(5));
    return;
  }

  const int total_tasks = count_visible_tasks(head, "");
  const int visible_tasks = count_visible_tasks(head, search_query);
  const int start_row = 4;
  const int footer_row = LINES - 1;
  const int detail_top = LINES - 6;
  const int list_bottom = detail_top - 2;

  int id_w = 4;
  int status_w = 6;
  int prio_w = 6;
  int project_w = (COLS - 26) / 3;
  if (project_w < 10)
    project_w = 10;
  if (project_w > 20)
    project_w = 20;
  int title_w = COLS - (id_w + status_w + prio_w + project_w + 12);
  if (title_w < 12)
    title_w = 12;

  attron(A_BOLD | COLOR_PAIR(1));
  mvprintw(0, 2, "KENZITUI");
  attroff(A_BOLD | COLOR_PAIR(1));
  attron(A_DIM | COLOR_PAIR(5));
  mvprintw(0, 12, "Task Manager");
  mvprintw(0, COLS - 27, "Visible: %-3d  Total: %-3d", visible_tasks, total_tasks);
  attroff(A_DIM | COLOR_PAIR(5));

  attron(A_BOLD | COLOR_PAIR(7));
  if (search_query != NULL && search_query[0] != '\0') {
    mvprintw(1, 2, "Filter: %.*s", COLS - 12, search_query);
  } else {
    mvprintw(1, 2, "Filter: (none)");
  }
  attroff(A_BOLD | COLOR_PAIR(7));

  attron(COLOR_PAIR(7));
  mvhline(2, 1, ACS_HLINE, COLS - 2);
  attroff(COLOR_PAIR(7));

  attron(A_BOLD | COLOR_PAIR(5));
  mvprintw(3, 2, "%-*s %-*s %-*s %-*s %-*s", id_w, "ID", status_w, "STATE",
           prio_w, "PRIO", project_w, "PROJECT", title_w, "TITLE");
  attroff(A_BOLD | COLOR_PAIR(5));

  Task *current = head;
  int row = start_row;
  int hidden = 0;
  while (current != NULL) {
    if (search_query != NULL && search_query[0] != '\0' &&
        strcasestr(current->title, search_query) == NULL &&
        strcasestr(current->project, search_query) == NULL) {
      current = current->next;
      continue;
    }

    if (row > list_bottom) {
      hidden++;
      current = current->next;
      continue;
    }

    if (current->id == selected_id) {
      attron(COLOR_PAIR(6));
      mvhline(row, 1, ' ', COLS - 2);
      mvprintw(row, 2, ">");
      attroff(COLOR_PAIR(6));
    } else {
      attron(COLOR_PAIR(7));
      mvhline(row, 1, ' ', COLS - 2);
      attroff(COLOR_PAIR(7));
    }

    int text_color = current->is_done ? 8 : 5;
    attron(COLOR_PAIR(text_color));
    if (current->id == selected_id) {
      attron(A_BOLD);
    }

    mvprintw(row, 4, "%-*d %-*s ", id_w, current->id, status_w,
             current->is_done ? "DONE" : "TODO");
    attron(COLOR_PAIR(priority_color(current->priority)));
    mvprintw(row, 4 + id_w + status_w + 2, "%-*s", prio_w,
             priority_label(current->priority));
    attroff(COLOR_PAIR(priority_color(current->priority)));

    attron(COLOR_PAIR(text_color));
    mvprintw(row, 4 + id_w + status_w + prio_w + 3, "%-*.*s %-*.*s", project_w,
             project_w, current->project, title_w, title_w, current->title);

    if (current->id == selected_id) {
      attroff(A_BOLD);
    }
    attroff(COLOR_PAIR(text_color));

    row++;
    current = current->next;
  }

  if (hidden > 0) {
    attron(A_DIM | COLOR_PAIR(5));
    mvprintw(list_bottom, COLS - 16, "+ %d more", hidden);
    attroff(A_DIM | COLOR_PAIR(5));
  }

  attron(COLOR_PAIR(7));
  mvhline(detail_top - 1, 1, ACS_HLINE, COLS - 2);
  attroff(COLOR_PAIR(7));

  Task *selected = find_task_by_id(head, selected_id);
  if (selected != NULL) {
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(detail_top, 2, "Project");
    attroff(A_BOLD | COLOR_PAIR(1));
    attron(COLOR_PAIR(5));
    mvprintw(detail_top, 12, "%.*s", COLS - 14, selected->project);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(detail_top + 1, 2, "Path");
    attroff(A_BOLD | COLOR_PAIR(1));
    attron(COLOR_PAIR(5));
    mvprintw(detail_top + 1, 12, "%.*s", COLS - 14, selected->path);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(detail_top + 2, 2, "Description");
    attroff(A_BOLD | COLOR_PAIR(1));
    attron(COLOR_PAIR(5));
    mvprintw(detail_top + 2, 14, "%.*s", COLS - 16, selected->description);
    attroff(COLOR_PAIR(5));
  }

  attron(A_DIM | COLOR_PAIR(5));
  mvprintw(footer_row, 2,
           "j/k move  SPACE done  a add  e edit  d delete  p priority  / search  c clear  o open  q quit");
  attroff(A_DIM | COLOR_PAIR(5));
}

int main() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);    // Accent
    init_pair(2, COLOR_RED, -1);     // HIGH
    init_pair(3, COLOR_YELLOW, -1);  // MEDIUM
    init_pair(4, COLOR_GREEN, -1);   // LOW
    init_pair(5, COLOR_WHITE, -1);   // Body text
    init_pair(6, COLOR_BLACK, COLOR_CYAN); // Selected row
    init_pair(7, COLOR_BLUE, -1);    // Lines/meta
    init_pair(8, COLOR_WHITE, -1);   // Done task text
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
