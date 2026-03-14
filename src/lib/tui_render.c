#define _GNU_SOURCE
#include "tui_render.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

typedef struct ColumnBucket {
  const char *name;
  Task **items;
  int count;
} ColumnBucket;

static int matches_search(const Task *task, const char *search_query) {
  if (search_query == NULL || search_query[0] == '\0') {
    return 1;
  }

  return strcasestr(task->name, search_query) != NULL ||
         strcasestr(task->project, search_query) != NULL ||
         strcasestr(task->ticket, search_query) != NULL ||
         strcasestr(task->tags, search_query) != NULL ||
         strcasestr(task->phase, search_query) != NULL;
}

static Task *find_task_by_id(Task *head, int id) {
  Task *current = head;
  while (current != NULL) {
    if (current->id == id) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static int phase_to_column(const char *phase) {
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

static int collect_visible_tasks(Task *head, const char *search_query,
                                 Task **out_items, int max_items,
                                 int *total_visible) {
  int count = 0;
  int total = 0;
  Task *current = head;
  while (current != NULL) {
    if (matches_search(current, search_query)) {
      total++;
      if (count < max_items) {
        out_items[count++] = current;
      }
    }
    current = current->next;
  }
  *total_visible = total;
  return count;
}

static void print_trim(int y, int x, int width, const char *text, int attrs,
                       int color) {
  if (width <= 0) {
    return;
  }
  attron(attrs | COLOR_PAIR(color));
  mvprintw(y, x, "%.*s", width, text != NULL ? text : "");
  attroff(attrs | COLOR_PAIR(color));
}

static void draw_box_with_title(int y, int x, int h, int w, const char *title,
                                int color) {
  if (h < 3 || w < 6) {
    return;
  }

  attron(COLOR_PAIR(color));
  mvhline(y, x + 1, ACS_HLINE, w - 2);
  mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
  mvvline(y + 1, x, ACS_VLINE, h - 2);
  mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
  mvaddch(y, x, ACS_ULCORNER);
  mvaddch(y, x + w - 1, ACS_URCORNER);
  mvaddch(y + h - 1, x, ACS_LLCORNER);
  mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(color));

  if (title != NULL && title[0] != '\0') {
    print_trim(y, x + 2, w - 4, title, A_BOLD, color);
  }
}

static int render_compact_card(const Task *task, int y, int x, int w,
                               int is_selected) {
  int h = 4;
  draw_box_with_title(y, x, h, w, "", 7);

  if (is_selected) {
    attron(A_BOLD | A_REVERSE | COLOR_PAIR(5));
  } else {
    attron(COLOR_PAIR(5));
  }

  char line1[256];
  snprintf(line1, sizeof(line1), "%s P:%d %s", task->ticket, task->points,
           task->name);
  mvprintw(y + 1, x + 1, "%.*s", w - 2, line1);

  char line2[256];
  snprintf(line2, sizeof(line2), "%s", task->tags[0] != '\0' ? task->tags : "-");
  mvprintw(y + 2, x + 1, "%.*s", w - 2, line2);

  if (is_selected) {
    attroff(A_BOLD | A_REVERSE | COLOR_PAIR(5));
  } else {
    attroff(COLOR_PAIR(5));
  }

  return h;
}

static void render_board(Task *head, int selected_id, const char *search_query,
                         int top, int bottom) {
  Task *visible[512];
  int total_visible = 0;
  int visible_count = collect_visible_tasks(head, search_query, visible, 512,
                                            &total_visible);

  int col_gap = 1;
  int col_w = (COLS - 4 - (2 * col_gap)) / 3;
  int col_x[3] = {1, 1 + col_w + col_gap, 1 + (col_w + col_gap) * 2};
  int col_h = bottom - top + 1;

  Task *mem0[256], *mem1[256], *mem2[256];
  ColumnBucket cols[3] = {{"Backlog", mem0, 0},
                          {"Doing", mem1, 0},
                          {"Need CR", mem2, 0}};

  for (int i = 0; i < visible_count; i++) {
    int c = phase_to_column(visible[i]->phase);
    if (cols[c].count < 256) {
      cols[c].items[cols[c].count++] = visible[i];
    }
  }

  for (int c = 0; c < 3; c++) {
    char title[64];
    snprintf(title, sizeof(title), "%s (%d|%d)", cols[c].name, cols[c].count,
             total_visible);
    draw_box_with_title(top, col_x[c], col_h, col_w, title, 7);

    print_trim(top + 1, col_x[c] + 2, col_w - 4, "Not Assigned", A_DIM, 5);
    print_trim(top + 2, col_x[c] + 2, col_w - 4, "S syukri.khairi", 0, 1);

    int y = top + 4;
    for (int i = 0; i < cols[c].count; i++) {
      int h = 4;
      if (y + h > top + col_h - 1) {
        print_trim(top + col_h - 2, col_x[c] + 2, col_w - 4, "+more", A_DIM,
                   5);
        break;
      }

      y += render_compact_card(cols[c].items[i], y, col_x[c] + 1, col_w - 2,
                               cols[c].items[i]->id == selected_id);
    }
  }
}

static void render_details(Task *selected, int top, int h) {
  draw_box_with_title(top, 1, h, COLS - 2, "Details", 7);
  if (selected == NULL) {
    return;
  }

  char line1[256];
  snprintf(line1, sizeof(line1), "%s | %s | P:%d", selected->ticket,
           selected->phase, selected->points);
  print_trim(top + 1, 3, COLS - 6, line1, A_BOLD, 5);

  char line2[512];
  snprintf(line2, sizeof(line2), "Project:%s  Next:%s", selected->project,
           selected->next_sprint_meeting[0] != '\0' ? selected->next_sprint_meeting
                                                    : "-");
  print_trim(top + 2, 3, COLS - 6, line2, 0, 5);

  char line3[512];
  snprintf(line3, sizeof(line3), "Tags:%s", selected->tags[0] != '\0' ? selected->tags : "-");
  print_trim(top + 3, 3, COLS - 6, line3, 0, 5);

  print_trim(top + 4, 3, COLS - 6, selected->description, 0, 5);
}

void init_tui_colors(void) {
  if (!has_colors()) {
    return;
  }

  start_color();
  use_default_colors();
  init_pair(1, COLOR_CYAN, -1);
  init_pair(2, COLOR_RED, -1);
  init_pair(5, COLOR_WHITE, -1);
  init_pair(7, COLOR_BLUE, -1);
}

void display_tasks(Task *head, int selected_id, const char *search_query) {
  if (LINES < 24 || COLS < 90) {
    print_trim(1, 2, COLS - 4, "Terminal too small. Need at least 90x24.",
               A_BOLD, 2);
    return;
  }

  print_trim(0, 2, 40, "KENZITUI COMPACT BOARD", A_BOLD, 1);
  if (search_query != NULL && search_query[0] != '\0') {
    char filter[256];
    snprintf(filter, sizeof(filter), "Filter: %s", search_query);
    print_trim(0, COLS - 42, 40, filter, 0, 7);
  }

  int detail_h = 7;
  int detail_top = LINES - detail_h - 1;
  render_board(head, selected_id, search_query, 2, detail_top - 1);

  Task *selected = find_task_by_id(head, selected_id);
  render_details(selected, detail_top, detail_h);

  print_trim(LINES - 1, 2, COLS - 4,
             "j/k move  SPACE done  a add  e edit  d delete  p priority  / search  c clear  o open  q quit",
             A_DIM, 5);
}
