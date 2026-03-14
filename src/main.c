#define _GNU_SOURCE
#include "app_config.h"
#include "kenzutls.h"
#include "phab_fetch.h"
#include "task.h"
#include "tui_render.h"
#include "tuiaction.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int parse_fetch_id_arg(int argc, char **argv, int *out_id) {
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--id") == 0) {
      if (i + 1 >= argc) {
        return 0;
      }
      *out_id = atoi(argv[i + 1]);
      return 1;
    }

    if (strncmp(argv[i], "--id=", 5) == 0) {
      *out_id = atoi(argv[i] + 5);
      return 1;
    }
  }
  return 0;
}

static int has_flag(int argc, char **argv, const char *flag) {
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], flag) == 0) {
      return 1;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  app_config_load(".kenzitui.conf");

  if (argc > 1 && strcmp(argv[1], "fetch") == 0) {
    int sprint_id = 0;
    if (!parse_fetch_id_arg(argc, argv, &sprint_id) || sprint_id <= 0) {
      fprintf(stderr, "Usage: %s fetch --id <sprint_id> [--preview]\n", argv[0]);
      return EXIT_FAILURE;
    }

    if (has_flag(argc, argv, "--preview")) {
      size_t fetched = 0, updated = 0, added = 0, kept = 0;
      int rc = preview_fetch_sprint_tasks(sprint_id, "tasks.dat", ".env",
                                          &fetched, &updated, &added, &kept);
      if (rc != 0) {
        fprintf(stderr, "Failed to preview fetch for sprint %d.\n", sprint_id);
        return EXIT_FAILURE;
      }
      printf("Preview sprint %d: fetched=%zu updated=%zu added=%zu kept=%zu\n",
             sprint_id, fetched, updated, added, kept);
      return EXIT_SUCCESS;
    }

    int rc = fetch_sprint_tasks_to_file(sprint_id, "tasks.dat", ".env");
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  if (argc > 1 && strcmp(argv[1], "restore") == 0) {
    const char *from = "tasks.dat.bak";
    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
        from = argv[i + 1];
        i++;
      } else if (strncmp(argv[i], "--from=", 7) == 0) {
        from = argv[i] + 7;
      }
    }
    if (!copy_file_binary(from, "tasks.dat")) {
      fprintf(stderr, "Failed to restore from %s to tasks.dat\n", from);
      return EXIT_FAILURE;
    }
    printf("Restored tasks.dat from %s\n", from);
    return EXIT_SUCCESS;
  }

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  init_tui_colors();

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
  action.task_file = "tasks.dat";

  int ch = 0;
  while (ch != 'q') {
    clear();
    display_tasks(head, selected_id, search_query);
    refresh();

    action.ch = getch();
    ch = action.ch;
    execute(&action);
  }

  save_tasks_to_file(head, "tasks.dat");
  free_all_tasks(head);
  endwin();

  return EXIT_SUCCESS;
}
