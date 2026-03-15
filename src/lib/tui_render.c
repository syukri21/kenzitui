#define _GNU_SOURCE
#include "app_config.h"
#include "tui_render.h"
#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct ColumnBucket {
  const char *name;
  Task **items;
  int count;
} ColumnBucket;

typedef struct PhaseSummary {
  int backlog;
  int doing;
  int need_cr;
  int done;
  int others;
  int total;
} PhaseSummary;

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
  if (strcasestr(phase, "done") != NULL) {
    return 3;
  }
  return 0;
}

static const char *display_owner_name(void) {
  const char *owner = getenv("KENZITUI_PHAB_USER");
  if (owner != NULL && owner[0] != '\0') {
    return owner;
  }
  owner = getenv("PHAB_USER");
  if (owner != NULL && owner[0] != '\0') {
    return owner;
  }
  owner = getenv("USER");
  if (owner != NULL && owner[0] != '\0') {
    return owner;
  }
  return "owner";
}

static void accumulate_phase_summary(const Task *task, PhaseSummary *summary) {
  if (task == NULL || summary == NULL) {
    return;
  }

  summary->total++;
  if (task->phase[0] == '\0') {
    summary->others++;
    return;
  }

  if (strcasestr(task->phase, "backlog") != NULL) {
    summary->backlog++;
  } else if (strcasestr(task->phase, "doing") != NULL) {
    summary->doing++;
  } else if (strcasestr(task->phase, "need") != NULL &&
             strcasestr(task->phase, "cr") != NULL) {
    summary->need_cr++;
  } else if (strcasestr(task->phase, "done") != NULL) {
    summary->done++;
  } else {
    summary->others++;
  }
}

static int percent_of(int part, int total) {
  if (total <= 0) {
    return 0;
  }
  return (part * 100 + (total / 2)) / total;
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

static int draw_segment(int y, int x, int width, const char *text, int attrs,
                        int color) {
  if (width <= 0 || text == NULL || text[0] == '\0') {
    return 0;
  }
  int n = (int)strlen(text);
  if (n > width) {
    n = width;
  }
  attron(attrs | COLOR_PAIR(color));
  mvaddnstr(y, x, text, n);
  attroff(attrs | COLOR_PAIR(color));
  return n;
}

static int draw_name_with_bracket_tone(int y, int x, int width,
                                       const char *name, int attrs) {
  if (width <= 0 || name == NULL || name[0] == '\0') {
    return 0;
  }

  int written = 0;
  int in_bracket = 0;
  const char *run_start = name;

  for (const char *p = name;; p++) {
    int at_end = (*p == '\0');
    int toggle = (*p == '[' || *p == ']');
    if (at_end || toggle) {
      if (p > run_start && written < width) {
        size_t run_len = (size_t)(p - run_start);
        int n = (int)run_len;
        if (n > width - written) {
          n = width - written;
        }
        attron(attrs | COLOR_PAIR(in_bracket ? 6 : 5));
        mvaddnstr(y, x + written, run_start, n);
        attroff(attrs | COLOR_PAIR(in_bracket ? 6 : 5));
        written += n;
      }

      if (at_end || written >= width) {
        break;
      }

      char bracket[2] = {*p, '\0'};
      written += draw_segment(y, x + written, width - written, bracket, attrs,
                              6);
      if (*p == '[') {
        in_bracket = 1;
      } else if (*p == ']') {
        in_bracket = 0;
      }
      run_start = p + 1;
    }
  }

  return written;
}

static void draw_ticket_point_name(const Task *task, int y, int x, int width,
                                   int attrs) {
  if (width <= 0) {
    return;
  }

  int written = 0;
  written += draw_segment(y, x + written, width - written, task->ticket, attrs,
                          8);
  written += draw_segment(y, x + written, width - written, " ", attrs, 5);

  char points[32];
  snprintf(points, sizeof(points), "P:%d", task->points);
  written += draw_segment(y, x + written, width - written, points, attrs, 9);
  written += draw_segment(y, x + written, width - written, " ", attrs, 5);

  draw_name_with_bracket_tone(y, x + written, width - written, task->name,
                              attrs);
}

static void draw_compact_tags(const Task *task, int y, int x, int width,
                              int is_selected) {
  if (width <= 0) {
    return;
  }

  if (task->tags[0] == '\0') {
    print_trim(y, x, width, "no-tag", A_DIM, 10);
    return;
  }

  char tags_buf[256];
  strncpy(tags_buf, task->tags, sizeof(tags_buf) - 1);
  tags_buf[sizeof(tags_buf) - 1] = '\0';

  int written = 0;
  char *saveptr = NULL;
  char *token = strtok_r(tags_buf, "|", &saveptr);
  while (token != NULL && written < width) {
    while (*token == ' ') {
      token++;
    }
    if (*token == '\0') {
      token = strtok_r(NULL, "|", &saveptr);
      continue;
    }

    char chip[96];
    snprintf(chip, sizeof(chip), "[%s]", token);
    written += draw_segment(y, x + written, width - written, chip,
                            is_selected ? A_DIM | A_BOLD : A_DIM, 10);
    if (written < width) {
      written += draw_segment(y, x + written, width - written, " ",
                              is_selected ? A_DIM | A_BOLD : A_DIM, 10);
    }
    token = strtok_r(NULL, "|", &saveptr);
  }
}

static int render_compact_card(const Task *task, int y, int x, int w,
                               int is_selected) {
  const AppConfig *cfg = app_config_get();
  int h = 5;
  draw_box_with_title(y, x, h, w, "", is_selected ? 2 : 7);

  if (is_selected) {
    attron(A_BOLD | COLOR_PAIR(2));
    mvaddch(y + 1, x + w - 2, '*');
    attroff(A_BOLD | COLOR_PAIR(2));
  }

  int text_attrs = is_selected ? A_BOLD : 0;
  draw_ticket_point_name(task, y + 1, x + 1, w - 2, text_attrs);

  if (cfg->compact.show_tags) {
    draw_compact_tags(task, y + 2, x + 1, w - 2, is_selected);
  } else {
    print_trim(y + 2, x + 1, w - 2, "", 0, 5);
  }
  if (cfg->compact.show_path) {
    print_trim(y + 3, x + 1, w - 2, task->path[0] != '\0' ? task->path : "-",
               is_selected ? A_BOLD : A_DIM, 1);
  } else if (cfg->compact.show_context) {
    print_trim(y + 3, x + 1, w - 2,
               task->context_path[0] != '\0' ? task->context_path : "-",
               is_selected ? A_BOLD : A_DIM, 1);
  } else {
    print_trim(y + 3, x + 1, w - 2, "", 0, 5);
  }

  return h;
}

static void render_board(Task *head, int selected_id, const char *search_query,
                         int top, int bottom) {
  static int col_scroll[4] = {0, 0, 0, 0};
  const int card_h = 5;

  Task *visible[512];
  int total_visible = 0;
  int visible_count = collect_visible_tasks(head, search_query, visible, 512,
                                            &total_visible);

  int col_gap = 1;
  int col_w = (COLS - 4 - (3 * col_gap)) / 4;
  int col_x[4] = {1,
                  1 + col_w + col_gap,
                  1 + (col_w + col_gap) * 2,
                  1 + (col_w + col_gap) * 3};
  int col_h = bottom - top + 1;

  Task *mem0[256], *mem1[256], *mem2[256], *mem3[256];
  ColumnBucket cols[4] = {{"Backlog", mem0, 0},
                          {"Doing", mem1, 0},
                          {"Need CR", mem2, 0},
                          {"Done", mem3, 0}};
  int col_points[4] = {0, 0, 0, 0};

  for (int i = 0; i < visible_count; i++) {
    int c = phase_to_column(visible[i]->phase);
    if (cols[c].count < 256) {
      cols[c].items[cols[c].count++] = visible[i];
      col_points[c] += visible[i]->points;
    }
  }

  for (int c = 0; c < 4; c++) {
    char title[64];
    snprintf(title, sizeof(title), "%s (%d|%d)", cols[c].name, cols[c].count,
             total_visible);
    draw_box_with_title(top, col_x[c], col_h, col_w, title, 7);

    char ptitle[32];
    snprintf(ptitle, sizeof(ptitle), "P:%d", col_points[c]);
    int px = col_x[c] + col_w - 2 - (int)strlen(ptitle);
    if (px > col_x[c] + 2) {
      print_trim(top, px, col_w - 3, ptitle, A_BOLD, 9);
    }

    print_trim(top + 1, col_x[c] + 2, col_w - 4, "Not Assigned", A_DIM, 5);
    char owner_line[96];
    snprintf(owner_line, sizeof(owner_line), "S %s", display_owner_name());
    print_trim(top + 2, col_x[c] + 2, col_w - 4, owner_line, 0, 1);

    int cards_top = top + 4;
    int cards_bottom = top + col_h - 2;
    int max_slots = (cards_bottom - cards_top + 1) / card_h;
    if (max_slots < 1) {
      max_slots = 1;
    }

    int max_scroll = cols[c].count - max_slots;
    if (max_scroll < 0) {
      max_scroll = 0;
    }
    if (col_scroll[c] > max_scroll) {
      col_scroll[c] = max_scroll;
    }
    if (col_scroll[c] < 0) {
      col_scroll[c] = 0;
    }

    int selected_row = -1;
    for (int i = 0; i < cols[c].count; i++) {
      if (cols[c].items[i]->id == selected_id) {
        selected_row = i;
        break;
      }
    }
    if (selected_row >= 0) {
      if (selected_row < col_scroll[c]) {
        col_scroll[c] = selected_row;
      } else if (selected_row >= col_scroll[c] + max_slots) {
        col_scroll[c] = selected_row - max_slots + 1;
      }
      if (col_scroll[c] > max_scroll) {
        col_scroll[c] = max_scroll;
      }
    }

    int start = col_scroll[c];
    int end = start + max_slots;
    if (end > cols[c].count) {
      end = cols[c].count;
    }

    if (start > 0) {
      char up_buf[16];
      snprintf(up_buf, sizeof(up_buf), "↑%d", start);
      int ux = col_x[c] + col_w - 2 - (int)strlen(up_buf);
      if (ux > col_x[c] + 1) {
        print_trim(top + 3, ux, col_w - 3, up_buf, A_DIM, 7);
      }
    }

    int y = cards_top;
    for (int i = start; i < end; i++) {
      y += render_compact_card(cols[c].items[i], y, col_x[c] + 1, col_w - 2,
                               cols[c].items[i]->id == selected_id);
    }

    int hidden_below = cols[c].count - end;
    if (hidden_below > 0) {
      char down_buf[16];
      snprintf(down_buf, sizeof(down_buf), "%d↓", hidden_below);
      int dx = col_x[c] + col_w - 2 - (int)strlen(down_buf);
      if (dx > col_x[c] + 1) {
        print_trim(top + col_h - 2, dx, col_w - 3, down_buf, A_DIM, 7);
      }
    }
  }
}

static void render_details(Task *selected, int top, int h) {
  draw_box_with_title(top, 1, h, COLS - 2, "Details", 7);
  if (selected == NULL) {
    return;
  }

  draw_segment(top + 1, 3, COLS - 6, selected->ticket, A_BOLD, 8);
  draw_segment(top + 1, 3 + (int)strlen(selected->ticket), COLS - 6, " | ",
               A_BOLD, 5);
  print_trim(top + 1, 6 + (int)strlen(selected->ticket), COLS - 10,
             selected->phase, A_BOLD, 1);
  char pbuf[32];
  snprintf(pbuf, sizeof(pbuf), " | P:%d", selected->points);
  draw_segment(top + 1, 7 + (int)strlen(selected->ticket) +
                            (int)strlen(selected->phase),
               COLS - 6, pbuf, A_BOLD, 9);

  char line2[512];
  snprintf(line2, sizeof(line2), "Project:%s  Next:%s", selected->project,
           selected->next_sprint_meeting[0] != '\0' ? selected->next_sprint_meeting
                                                    : "-");
  print_trim(top + 2, 3, COLS - 6, line2, 0, 5);

  char line3[512];
  snprintf(line3, sizeof(line3), "Tags:%s", selected->tags[0] != '\0' ? selected->tags : "-");
  print_trim(top + 3, 3, COLS - 6, line3, 0, 5);

  char line4[512];
  snprintf(line4, sizeof(line4), "Path:%s",
           selected->path[0] != '\0' ? selected->path : "-");
  print_trim(top + 4, 3, COLS - 6, line4, 0, 5);

  char line5[512];
  snprintf(line5, sizeof(line5), "Context:%s",
           selected->context_path[0] != '\0' ? selected->context_path : "-");
  print_trim(top + 5, 3, COLS - 6, line5, 0, 5);

  print_trim(top + 6, 3, COLS - 6, selected->description, 0, 5);
}

void init_tui_colors(void) {
  const AppConfig *cfg = app_config_get();
  if (!has_colors()) {
    return;
  }

  start_color();
  use_default_colors();
  init_pair(1, cfg->colors.path, -1);
  init_pair(2, cfg->colors.selected_border, -1);
  init_pair(6, cfg->colors.bracket, -1);
  init_pair(8, cfg->colors.ticket, -1);
  init_pair(9, cfg->colors.points, -1);
  init_pair(10, cfg->colors.tags, -1);
  init_pair(11, cfg->colors.title, -1);
  init_pair(5, cfg->colors.text, -1);
  init_pair(7, cfg->colors.border, -1);
}

void display_tasks(Task *head, int selected_id, const char *search_query) {
  const AppConfig *cfg = app_config_get();
  if (LINES < 24 || COLS < 90) {
    print_trim(1, 2, COLS - 4, "Terminal too small. Need at least 90x24.",
               A_BOLD, 2);
    return;
  }

  print_trim(0, 2, 40, "KENZITUI COMPACT BOARD", A_BOLD, 11);
  if (search_query != NULL && search_query[0] != '\0') {
    char filter[256];
    snprintf(filter, sizeof(filter), "Filter: %s", search_query);
    print_trim(0, COLS - 42, 40, filter, 0, 7);
  }

  PhaseSummary summary = {0, 0, 0, 0, 0, 0};
  for (Task *current = head; current != NULL; current = current->next) {
    if (!matches_search(current, search_query)) {
      continue;
    }
    accumulate_phase_summary(current, &summary);
  }

  char stat_line[256];
  snprintf(stat_line, sizeof(stat_line),
           "Backlog %d%%  Doing %d%%  Need CR %d%%  Done %d%%  Others %d%%  (n=%d)",
           percent_of(summary.backlog, summary.total),
           percent_of(summary.doing, summary.total),
           percent_of(summary.need_cr, summary.total),
           percent_of(summary.done, summary.total),
           percent_of(summary.others, summary.total), summary.total);
  print_trim(1, 2, COLS - 4, stat_line, A_DIM, 10);

  int detail_h = 9;
  int detail_top = LINES - detail_h - 1;
  render_board(head, selected_id, search_query, 2, detail_top - 1);

  Task *selected = find_task_by_id(head, selected_id);
  render_details(selected, detail_top, detail_h);

  char footer[256];
  snprintf(footer, sizeof(footer),
           "%c/%c phase  %c/%c move  SPACE done  %c fetch  %c add  %c edit  %c open  %c search  %c help  q quit",
           cfg->keys.nav_left, cfg->keys.nav_right, cfg->keys.nav_down,
           cfg->keys.nav_up, cfg->keys.fetch, cfg->keys.add, cfg->keys.edit,
           cfg->keys.open, cfg->keys.search, cfg->keys.help);
  print_trim(LINES - 1, 2, COLS - 4, footer, A_DIM, 5);
}
