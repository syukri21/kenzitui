#define _GNU_SOURCE

#include "phab_fetch.h"
#include "tuiaction_actions.h"
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void fetch_wait_for_ack(void) {
  nodelay(stdscr, FALSE);
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
  mvprintw(LINES - 1, 2, "Press any key to continue...");
  refresh();
  (void)getch();
  mvprintw(LINES - 1, 0,
           "                                                                   "
           "             ");
}

static int fetch_read_last_log_line(char *out, size_t out_size) {
  if (out == NULL || out_size == 0) {
    return 0;
  }
  out[0] = '\0';

  FILE *f = fopen("/tmp/kenzitui_fetch.log", "r");
  if (f == NULL) {
    return 0;
  }

  char line[512];
  while (fgets(line, sizeof(line), f) != NULL) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
    }
    if (line[0] == '\0') {
      continue;
    }
    snprintf(out, out_size, "%s", line);
  }
  fclose(f);
  return out[0] != '\0';
}

static void fetch_show_error_with_log(const char *prefix) {
  const char *detail = phab_fetch_last_error();
  if (detail != NULL && detail[0] != '\0') {
    char msg[512];
    snprintf(msg, sizeof(msg), "%s: %s", prefix != NULL ? prefix : "Fetch failed",
             detail);
    tui_show_status_message(msg);
    fetch_wait_for_ack();
    return;
  }

  char last[256];
  if (fetch_read_last_log_line(last, sizeof(last))) {
    char msg[512];
    snprintf(msg, sizeof(msg), "%s: %s", prefix != NULL ? prefix : "Fetch failed",
             last);
    tui_show_status_message(msg);
  } else {
    tui_show_status_message(prefix != NULL ? prefix : "Fetch failed.");
  }
  fetch_wait_for_ack();
}

void tui_action_handle_fetch(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }

  char sprint_buf[32] = "";
  tui_input("Fetch Sprint ID: ", sprint_buf, (int)sizeof(sprint_buf));
  int sprint_id = atoi(sprint_buf);
  if (sprint_id <= 0) {
    tui_show_status_message("Invalid sprint ID.");
    return;
  }

  const char *task_file =
      (action->state->task_file != NULL && action->state->task_file[0] != '\0')
          ? action->state->task_file
          : "tasks.dat";

  size_t fetched = 0, updated = 0, added = 0, kept = 0;
  if (preview_fetch_sprint_tasks(sprint_id, task_file, ".env", &fetched, &updated,
                                 &added, &kept) != 0) {
    fetch_show_error_with_log(
        "Preview failed. Check .env cookie/user or network");
    return;
  }

  char prompt[160];
  snprintf(prompt, sizeof(prompt),
           "Preview f:%zu u:%zu a:%zu keep:%zu. Apply? (y/N): ", fetched,
           updated, added, kept);
  if (!tui_confirm_prompt(prompt)) {
    tui_show_status_message("Fetch canceled.");
    return;
  }

  FILE *log_reset = fopen("/tmp/kenzitui_fetch.log", "w");
  if (log_reset != NULL) {
    fclose(log_reset);
  }

  pid_t pid = fork();
  if (pid < 0) {
    tui_show_status_message("Failed to start fetch process.");
    return;
  }

  if (pid == 0) {
    FILE *sink = fopen("/tmp/kenzitui_fetch.log", "a");
    if (sink != NULL) {
      dup2(fileno(sink), STDOUT_FILENO);
      dup2(fileno(sink), STDERR_FILENO);
      fclose(sink);
    }
    int rc = fetch_sprint_tasks_to_file(sprint_id, task_file, ".env");
    _exit(rc == 0 ? 0 : 1);
  }

  nodelay(stdscr, TRUE);
  const char spinner[] = "|/-\\";
  int spin_idx = 0;
  int canceled = 0;
  int status = 0;

  for (;;) {
    int done = waitpid(pid, &status, WNOHANG);
    if (done == pid) {
      break;
    }

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Fetching sprint %d... %c  (press ESC to cancel)", sprint_id,
             spinner[spin_idx % 4]);
    tui_show_status_message(msg);
    spin_idx++;

    int ch = getch();
    if (ch == 27) {
      canceled = 1;
      kill(pid, SIGTERM);
      waitpid(pid, &status, 0);
      break;
    }
    napms(100);
  }
  nodelay(stdscr, FALSE);

  if (canceled) {
    tui_show_status_message("Fetch canceled.");
    return;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (WIFSIGNALED(status)) {
      char msg[128];
      snprintf(msg, sizeof(msg), "Fetch failed (signal %d)", WTERMSIG(status));
      fetch_show_error_with_log(msg);
    } else {
      fetch_show_error_with_log(
          "Fetch failed. Check .env cookie/user or network");
    }
    return;
  }

  int last_id = 0;
  Task *loaded = load_tasks_from_file(task_file, &last_id);
  if (loaded == NULL) {
    fetch_show_error_with_log("Fetch done, but failed to reload tasks.dat");
    return;
  }

  free_all_tasks(action->state->head);
  action->state->head = loaded;
  action->state->selected_id = loaded->id;
  action->state->next_id = (last_id > 0) ? (last_id + 1) : 1;
  tui_show_status_message("Fetch success. Board updated.");
}
