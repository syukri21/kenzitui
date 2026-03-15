#define _GNU_SOURCE

#include "kenzutls.h"
#include "tuiaction_actions.h"
#include <ctype.h>
#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void persist(AppState *state) {
  if (state == NULL || state->task_file == NULL) {
    return;
  }
  save_tasks_to_file(state->head, state->task_file);
}

static Task *find_selected(AppState *state) {
  if (state == NULL || state->selected_id == 0) {
    return NULL;
  }
  for (Task *current = state->head; current != NULL; current = current->next) {
    if (current->id == state->selected_id) {
      return current;
    }
  }
  return NULL;
}

static int is_missing_context_path(const char *path) {
  if (path == NULL) {
    return 1;
  }
  const char *start = path;
  while (*start != '\0' && isspace((unsigned char)*start)) {
    start++;
  }
  const char *end = start + strlen(start);
  while (end > start && isspace((unsigned char)end[-1])) {
    end--;
  }
  size_t len = (size_t)(end - start);
  if (len == 0) {
    return 1;
  }
  if (len == 1 && start[0] == '-') {
    return 1;
  }
  return 0;
}

static int resolve_open_path(const char *raw, char *out, size_t out_size) {
  if (raw == NULL || out == NULL || out_size == 0) {
    return 0;
  }
  if (raw[0] != '~') {
    return snprintf(out, out_size, "%s", raw) < (int)out_size;
  }

  const char *home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return snprintf(out, out_size, "%s", raw) < (int)out_size;
  }

  if (raw[1] == '\0') {
    return snprintf(out, out_size, "%s", home) < (int)out_size;
  }
  if (raw[1] == '/') {
    return snprintf(out, out_size, "%s%s", home, raw + 1) < (int)out_size;
  }

  // Keep ~user style unchanged; we only expand current-user "~" forms.
  return snprintf(out, out_size, "%s", raw) < (int)out_size;
}

static void build_tmux_window_name(const Task *task, char *out,
                                   size_t out_size) {
  if (task == NULL || out == NULL || out_size == 0) {
    return;
  }

  char base[MAX_TICKET];
  if (task->ticket[0] != '\0') {
    snprintf(base, sizeof(base), "%s", task->ticket);
  } else {
    snprintf(base, sizeof(base), "TASK%d", task->id);
  }

  size_t w = 0;
  for (size_t i = 0; base[i] != '\0' && w + 1 < out_size; i++) {
    unsigned char ch = (unsigned char)base[i];
    if (isalnum(ch) || ch == '-' || ch == '_') {
      out[w++] = (char)ch;
    } else {
      out[w++] = '_';
    }
  }
  if (w == 0) {
    snprintf(out, out_size, "TASK%d", task->id);
    return;
  }
  out[w] = '\0';
}

static int find_tmux_target_for_path(const char *path, char *out_window,
                                     size_t out_window_size, char *out_pane,
                                     size_t out_pane_size) {
  if (path == NULL || out_window == NULL || out_window_size == 0 ||
      out_pane == NULL || out_pane_size == 0) {
    return 0;
  }

  FILE *pipe = popen(
      "tmux list-panes -a -F '#{pane_id} #{session_name}:#{window_index} "
      "#{pane_current_path}'",
      "r");
  if (pipe == NULL) {
    return 0;
  }

  char line[2048];
  while (fgets(line, sizeof(line), pipe) != NULL) {
    char pane[64];
    char window[128];
    char pane_path[MAX_PATH];
    if (sscanf(line, "%63s %127s %255s", pane, window, pane_path) != 3) {
      continue;
    }
    if (strcmp(pane_path, path) == 0) {
      snprintf(out_window, out_window_size, "%s", window);
      snprintf(out_pane, out_pane_size, "%s", pane);
      pclose(pipe);
      return 1;
    }
  }

  pclose(pipe);
  return 0;
}

static void open_target_path(const Task *current, const char *raw_path,
                             const char *empty_message,
                             const char *failed_message) {
  if (current == NULL || raw_path == NULL || raw_path[0] == '\0') {
    tui_show_status_message(
        empty_message != NULL ? empty_message : "Path is empty.");
    return;
  }

  char command[4096];
  char resolved_path[MAX_PATH];
  if (!resolve_open_path(raw_path, resolved_path, sizeof(resolved_path))) {
    tui_show_status_message("Path too long.");
    return;
  }
  char q_path[(MAX_PATH * 5) + 8];
  if (!shell_quote_single(resolved_path, q_path, sizeof(q_path))) {
    tui_show_status_message("Path too long for shell escaping.");
    return;
  }

  if (getenv("TMUX")) {
    char window_name[64];
    char target_window[128];
    char target_pane[64];
    char q_target_window[(128 * 5) + 8];
    char q_target_pane[(64 * 5) + 8];
    char q_window_name[(64 * 5) + 8];
    if (find_tmux_target_for_path(resolved_path, target_window,
                                  sizeof(target_window), target_pane,
                                  sizeof(target_pane))) {
      if (!shell_quote_single(target_window, q_target_window,
                              sizeof(q_target_window)) ||
          !shell_quote_single(target_pane, q_target_pane,
                              sizeof(q_target_pane))) {
        tui_show_status_message("tmux target too long for shell escaping.");
        return;
      }

      snprintf(command, sizeof(command),
               "tmux select-window -t %s && tmux select-pane -t %s",
               q_target_window, q_target_pane);
    } else {
      build_tmux_window_name(current, window_name, sizeof(window_name));
      if (!shell_quote_single(window_name, q_window_name,
                              sizeof(q_window_name))) {
        tui_show_status_message("Window name too long for shell escaping.");
        return;
      }

      snprintf(command, sizeof(command), "tmux new-window -c %s -n %s 'nvim .'",
               q_path, q_window_name);
    }
    if (system(command) != 0) {
      tui_show_status_message(
          failed_message != NULL ? failed_message
                                 : "Failed to open tmux window for path.");
    }
  } else {
    def_prog_mode();
    endwin();
    snprintf(command, sizeof(command), "cd -- %s && nvim .", q_path);
    if (system(command) != 0) {
      tui_show_status_message(failed_message != NULL
                                  ? failed_message
                                  : "Failed to open nvim for selected path.");
    }
    reset_prog_mode();
    refresh();
  }
}

static int ensure_dir_recursive(const char *dir_path) {
  if (dir_path == NULL || dir_path[0] == '\0') {
    return 0;
  }
  if (strcmp(dir_path, ".") == 0 || strcmp(dir_path, "/") == 0) {
    return 1;
  }

  char tmp[MAX_PATH];
  if (snprintf(tmp, sizeof(tmp), "%s", dir_path) >= (int)sizeof(tmp)) {
    return 0;
  }

  size_t len = strlen(tmp);
  if (len == 0) {
    return 0;
  }
  if (len > 1 && tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
  }

  for (char *p = tmp + 1; *p != '\0'; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 0;
      }
      *p = '/';
    }
  }

  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static int split_parent_and_name(const char *full_path, char *parent,
                                 size_t parent_size, char *name,
                                 size_t name_size) {
  if (full_path == NULL || parent == NULL || name == NULL || parent_size == 0 ||
      name_size == 0) {
    return 0;
  }

  const char *slash = strrchr(full_path, '/');
  if (slash == NULL) {
    if (snprintf(parent, parent_size, ".") >= (int)parent_size) {
      return 0;
    }
    if (snprintf(name, name_size, "%s", full_path) >= (int)name_size) {
      return 0;
    }
    return 1;
  }

  size_t parent_len = (size_t)(slash - full_path);
  if (parent_len == 0) {
    if (snprintf(parent, parent_size, "/") >= (int)parent_size) {
      return 0;
    }
  } else {
    if (parent_len + 1 > parent_size) {
      return 0;
    }
    memcpy(parent, full_path, parent_len);
    parent[parent_len] = '\0';
  }

  if (snprintf(name, name_size, "%s", slash + 1) >= (int)name_size) {
    return 0;
  }
  return 1;
}

static int read_file_to_buffer(const char *path, char *out, size_t out_size) {
  if (path == NULL || out == NULL || out_size == 0) {
    return 0;
  }
  out[0] = '\0';

  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return 0;
  }

  size_t n = fread(out, 1, out_size - 1, f);
  out[n] = '\0';
  fclose(f);
  return 1;
}

static int replace_all(char *buf, size_t buf_size, const char *needle,
                       const char *replacement) {
  if (buf == NULL || needle == NULL || replacement == NULL || needle[0] == '\0' ||
      buf_size == 0) {
    return 0;
  }

  char temp[16384];
  size_t w = 0;
  size_t nlen = strlen(needle);
  size_t rlen = strlen(replacement);
  const char *p = buf;

  while (*p != '\0') {
    if (strncmp(p, needle, nlen) == 0) {
      if (w + rlen >= sizeof(temp)) {
        return 0;
      }
      memcpy(temp + w, replacement, rlen);
      w += rlen;
      p += nlen;
      continue;
    }
    if (w + 1 >= sizeof(temp)) {
      return 0;
    }
    temp[w++] = *p++;
  }
  temp[w] = '\0';

  if (w >= buf_size) {
    return 0;
  }
  memcpy(buf, temp, w + 1);
  return 1;
}

static void extract_service_and_title(const char *name, char *service,
                                      size_t service_size, char *title,
                                      size_t title_size) {
  if (service == NULL || service_size == 0 || title == NULL || title_size == 0) {
    return;
  }
  service[0] = '\0';
  title[0] = '\0';
  if (name == NULL || name[0] == '\0') {
    snprintf(service, service_size, "General");
    snprintf(title, title_size, "Task");
    return;
  }

  if (name[0] == '[') {
    const char *end = strchr(name, ']');
    if (end != NULL && end > name + 1) {
      size_t len = (size_t)(end - (name + 1));
      if (len >= service_size) {
        len = service_size - 1;
      }
      memcpy(service, name + 1, len);
      service[len] = '\0';
      const char *rest = end + 1;
      while (*rest != '\0' && isspace((unsigned char)*rest)) {
        rest++;
      }
      snprintf(title, title_size, "%s", *rest != '\0' ? rest : name);
      return;
    }
  }

  snprintf(service, service_size, "General");
  snprintf(title, title_size, "%s", name);
}

static const char *priority_to_text(Priority p) {
  switch (p) {
  case HIGH:
    return "high";
  case MEDIUM:
    return "medium";
  case LOW:
  default:
    return "low";
  }
}

static void tags_pipe_to_csv(const char *tags, char *out, size_t out_size) {
  if (out == NULL || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (tags == NULL || tags[0] == '\0') {
    return;
  }

  size_t w = 0;
  for (size_t i = 0; tags[i] != '\0' && w + 1 < out_size; i++) {
    char ch = tags[i];
    if (ch == '|') {
      if (w + 2 >= out_size) {
        break;
      }
      out[w++] = ',';
      out[w++] = ' ';
      continue;
    }
    out[w++] = ch;
  }
  out[w] = '\0';
}

static int write_context_template_file(const Task *task, const char *file_path) {
  if (task == NULL || file_path == NULL || file_path[0] == '\0') {
    return 0;
  }

  struct stat st;
  if (stat(file_path, &st) == 0 && st.st_size > 0) {
    return 1;
  }

  char template_buf[16384];
  if (!read_file_to_buffer("ContextTemplate.md", template_buf,
                           sizeof(template_buf))) {
    snprintf(
        template_buf, sizeof(template_buf),
        "---\n"
        "title: \"[%s] %s\"\n"
        "status: %s\n"
        "priority: %s\n"
        "points: %d\n"
        "tags: [%s]\n"
        "ticket: %s\n"
        "project: %s\n"
        "created: %s\n"
        "---\n\n"
        "%s\n",
        "General", task->name, task->phase, priority_to_text(task->priority),
        task->points, task->tags, task->ticket, task->project, "", task->description);
  }

  char service[64];
  char title[128];
  char points[16];
  char date[16];
  char tags_csv[256];
  extract_service_and_title(task->name, service, sizeof(service), title,
                            sizeof(title));
  snprintf(points, sizeof(points), "%d", task->points);
  tags_pipe_to_csv(task->tags, tags_csv, sizeof(tags_csv));

  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);
  strftime(date, sizeof(date), "%Y-%m-%d", &tm_now);

  if (!replace_all(template_buf, sizeof(template_buf), "{{Service}}", service) ||
      !replace_all(template_buf, sizeof(template_buf), "{{Title}}", title) ||
      !replace_all(template_buf, sizeof(template_buf), "{{Titile}}", title) ||
      !replace_all(template_buf, sizeof(template_buf), "{{status}}",
                   task->phase[0] != '\0' ? task->phase : "Backlog") ||
      !replace_all(template_buf, sizeof(template_buf), "{ { status } }",
                   task->phase[0] != '\0' ? task->phase : "Backlog") ||
      !replace_all(template_buf, sizeof(template_buf), "{{priority}}",
                   priority_to_text(task->priority)) ||
      !replace_all(template_buf, sizeof(template_buf), "{ { priority } }",
                   priority_to_text(task->priority)) ||
      !replace_all(template_buf, sizeof(template_buf), "{{Point}}", points) ||
      !replace_all(template_buf, sizeof(template_buf), "{{points}}", points) ||
      !replace_all(template_buf, sizeof(template_buf), "{ { Point } }", points) ||
      !replace_all(template_buf, sizeof(template_buf), "{{date}}", date) ||
      !replace_all(template_buf, sizeof(template_buf), "{ { date } }", date) ||
      !replace_all(template_buf, sizeof(template_buf), "{{tags}}", tags_csv) ||
      !replace_all(template_buf, sizeof(template_buf), "{ { tags } }",
                   tags_csv) ||
      !replace_all(template_buf, sizeof(template_buf), "{{ticket}}",
                   task->ticket)) {
    return 0;
  }

  FILE *f = fopen(file_path, "w");
  if (f == NULL) {
    return 0;
  }
  size_t len = strlen(template_buf);
  int ok = fwrite(template_buf, 1, len, f) == len;
  fclose(f);
  return ok;
}

static int ensure_context_file_seeded(const Task *task, const char *raw_path) {
  if (task == NULL || is_missing_context_path(raw_path)) {
    return 0;
  }

  char resolved_file[MAX_PATH];
  if (!resolve_open_path(raw_path, resolved_file, sizeof(resolved_file))) {
    return 0;
  }

  char parent[MAX_PATH];
  char file_name[MAX_PATH];
  if (!split_parent_and_name(resolved_file, parent, sizeof(parent), file_name,
                             sizeof(file_name))) {
    return 0;
  }
  if (file_name[0] == '\0') {
    return 0;
  }

  if (!ensure_dir_recursive(parent)) {
    return 0;
  }

  return write_context_template_file(task, resolved_file);
}

static void open_context_file_target(const Task *current, const char *raw_path,
                                     const char *empty_message,
                                     const char *failed_message) {
  if (current == NULL || is_missing_context_path(raw_path)) {
    tui_show_status_message(
        empty_message != NULL ? empty_message : "Context path is empty.");
    return;
  }

  char resolved_file[MAX_PATH];
  if (!resolve_open_path(raw_path, resolved_file, sizeof(resolved_file))) {
    tui_show_status_message("Context path too long.");
    return;
  }

  char parent[MAX_PATH];
  char file_name[MAX_PATH];
  if (!split_parent_and_name(resolved_file, parent, sizeof(parent), file_name,
                             sizeof(file_name))) {
    tui_show_status_message("Invalid context path.");
    return;
  }
  if (file_name[0] == '\0') {
    tui_show_status_message("Context file name is empty.");
    return;
  }

  if (!ensure_dir_recursive(parent)) {
    tui_show_status_message("Failed creating context directory.");
    return;
  }

  FILE *touch = fopen(resolved_file, "a");
  if (touch == NULL) {
    tui_show_status_message("Failed creating context file.");
    return;
  }
  fclose(touch);

  char q_parent[(MAX_PATH * 5) + 8];
  char q_file[(MAX_PATH * 5) + 8];
  if (!shell_quote_single(parent, q_parent, sizeof(q_parent)) ||
      !shell_quote_single(resolved_file, q_file, sizeof(q_file))) {
    tui_show_status_message("Context path too long for shell escaping.");
    return;
  }

  char command[4096];
  if (getenv("TMUX")) {
    char window_name[64];
    char target_window[128];
    char target_pane[64];
    char q_target_window[(128 * 5) + 8];
    char q_target_pane[(64 * 5) + 8];
    char q_window_name[(64 * 5) + 8];
    if (find_tmux_target_for_path(parent, target_window, sizeof(target_window),
                                  target_pane, sizeof(target_pane))) {
      if (!shell_quote_single(target_window, q_target_window,
                              sizeof(q_target_window)) ||
          !shell_quote_single(target_pane, q_target_pane,
                              sizeof(q_target_pane))) {
        tui_show_status_message("tmux target too long for shell escaping.");
        return;
      }
      snprintf(command, sizeof(command),
               "tmux select-window -t %s && tmux select-pane -t %s",
               q_target_window, q_target_pane);
    } else {
      build_tmux_window_name(current, window_name, sizeof(window_name));
      if (!shell_quote_single(window_name, q_window_name,
                              sizeof(q_window_name))) {
        tui_show_status_message("Window name too long for shell escaping.");
        return;
      }
      snprintf(command, sizeof(command),
               "tmux new-window -c %s -n %s \"nvim %s\"", q_parent,
               q_window_name, q_file);
    }
    if (system(command) != 0) {
      tui_show_status_message(failed_message != NULL
                                  ? failed_message
                                  : "Failed to open context file.");
    }
  } else {
    def_prog_mode();
    endwin();
    snprintf(command, sizeof(command), "cd -- %s && nvim %s", q_parent, q_file);
    if (system(command) != 0) {
      tui_show_status_message(failed_message != NULL
                                  ? failed_message
                                  : "Failed to open context file.");
    }
    reset_prog_mode();
    refresh();
  }
}

static void cycle_task_phase(Task *task) {
  if (task == NULL) {
    return;
  }
  if (strcasestr(task->phase, "done") != NULL) {
    strncpy(task->phase, "Backlog", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "doing") != NULL) {
    strncpy(task->phase, "Need CR", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "need") != NULL &&
      strcasestr(task->phase, "cr") != NULL) {
    strncpy(task->phase, "Done", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  strncpy(task->phase, "Doing", sizeof(task->phase) - 1);
  task->phase[sizeof(task->phase) - 1] = '\0';
}

static void cycle_task_phase_back(Task *task) {
  if (task == NULL) {
    return;
  }
  if (strcasestr(task->phase, "done") != NULL) {
    strncpy(task->phase, "Need CR", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "need") != NULL &&
      strcasestr(task->phase, "cr") != NULL) {
    strncpy(task->phase, "Doing", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  if (strcasestr(task->phase, "doing") != NULL) {
    strncpy(task->phase, "Backlog", sizeof(task->phase) - 1);
    task->phase[sizeof(task->phase) - 1] = '\0';
    return;
  }
  strncpy(task->phase, "Need CR", sizeof(task->phase) - 1);
  task->phase[sizeof(task->phase) - 1] = '\0';
}

void tui_action_toggle_done(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  mark_task_done(action->state->head, action->state->selected_id);
  persist(action->state);
}

void tui_action_delete(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  if (!tui_confirm_prompt("Delete task? (y/N): ")) {
    tui_show_status_message("Delete canceled.");
    return;
  }

  int old_id = action->state->selected_id;
  Task *current = action->state->head;
  Task *next_to_select = NULL;
  if (action->state->head != NULL && action->state->head->id == old_id) {
    next_to_select = action->state->head->next;
  } else {
    while (current != NULL && current->next != NULL && current->next->id != old_id) {
      current = current->next;
    }
    if (current != NULL && current->next != NULL) {
      next_to_select = current->next->next;
      if (next_to_select == NULL) {
        next_to_select = current;
      }
    }
  }

  delete_task(&action->state->head, old_id);
  if (next_to_select != NULL) {
    action->state->selected_id = next_to_select->id;
  } else if (action->state->head != NULL) {
    action->state->selected_id = action->state->head->id;
  } else {
    action->state->selected_id = 0;
  }
  persist(action->state);
}

void tui_action_add(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }

  char name[MAX_NAME] = "";
  char desc[MAX_DESC] = "";
  char project[MAX_PROJECT] = "";
  char path[MAX_PATH] = "";
  char context_path[MAX_PATH] = "";
  char phase[MAX_PHASE] = "Backlog";
  char tags[MAX_TAGS] = "";
  char ticket[MAX_TICKET] = "";
  char next_meeting[MAX_NEXT_MEETING] = "";
  char points_buf[16] = "0";

  tui_input("Task Name: ", name, MAX_NAME);
  tui_input("Task Description: ", desc, MAX_DESC);
  tui_input("Project Name: ", project, MAX_PROJECT);
  tui_get_path_input("Project Path: ", path, MAX_PATH);
  tui_get_path_input("Context File Path: ", context_path, MAX_PATH);
  tui_input("Phase: ", phase, MAX_PHASE);
  tui_input("Points: ", points_buf, (int)sizeof(points_buf));
  tui_input("Tags (| separated): ", tags, MAX_TAGS);
  tui_input("Ticket (e.g. T148277): ", ticket, MAX_TICKET);
  tui_input("Next Sprint Meeting: ", next_meeting, MAX_NEXT_MEETING);
  Priority priority = tui_get_priority_input();

  Task *task = create_task(action->state->next_id++, name, desc, project, path,
                           priority);
  if (task != NULL) {
    snprintf(task->context_path, sizeof(task->context_path), "%s", context_path);
    task_set_phab_fields(task, phase, atoi(points_buf), tags, ticket,
                         next_meeting);
    task_auto_fill_context_path(task);
    add_task(&action->state->head, task);
  }
  if (action->state->selected_id == 0 && action->state->head != NULL) {
    action->state->selected_id = action->state->head->id;
  }
  persist(action->state);
}

void tui_action_edit(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }

  char points_buf[16];
  snprintf(points_buf, sizeof(points_buf), "%d", current->points);
  tui_input("New Name: ", current->name, MAX_NAME);
  tui_input("New Description: ", current->description, MAX_DESC);
  tui_input("New Project Name: ", current->project, MAX_PROJECT);
  tui_get_path_input("New Project Path: ", current->path, MAX_PATH);
  tui_get_path_input("New Context File Path: ", current->context_path, MAX_PATH);
  tui_input("New Phase: ", current->phase, MAX_PHASE);
  tui_input("New Points: ", points_buf, (int)sizeof(points_buf));
  current->points = atoi(points_buf);
  tui_input("New Tags (| separated): ", current->tags, MAX_TAGS);
  tui_input("New Ticket: ", current->ticket, MAX_TICKET);
  tui_input("New Next Sprint Meeting: ", current->next_sprint_meeting,
            MAX_NEXT_MEETING);
  current->priority = tui_get_priority_input();
  task_auto_fill_context_path(current);
  persist(action->state);
}

void tui_action_cycle_priority(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  current->priority = (current->priority + 1) % 3;
  persist(action->state);
}

void tui_action_move_phase_next(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  cycle_task_phase(current);
  persist(action->state);
}

void tui_action_move_phase_prev(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  cycle_task_phase_back(current);
  persist(action->state);
}

void tui_action_open_path(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  open_target_path(current, current->path, "Project path is empty.",
                   "Failed to open project path.");
}

void tui_action_open_context_path(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }
  open_context_file_target(current, current->context_path,
                           "Context path is empty.",
                           "Failed to open context path.");
}

void tui_action_generate_context_path(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }
  Task *current = find_selected(action->state);
  if (current == NULL) {
    return;
  }

  if (is_missing_context_path(current->context_path)) {
    current->context_path[0] = '\0';
    task_auto_fill_context_path(current);
    if (is_missing_context_path(current->context_path)) {
      tui_show_status_message(
          "Context path generation failed. Set OBSIDIAN_PATH first.");
      return;
    }
    persist(action->state);
  }

  if (!ensure_context_file_seeded(current, current->context_path)) {
    tui_show_status_message("Failed to apply ContextTemplate.md to context.");
    return;
  }

  open_context_file_target(current, current->context_path,
                           "Context path is empty.",
                           "Failed to open context path.");
}
