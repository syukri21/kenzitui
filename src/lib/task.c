#define _GNU_SOURCE

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kenzutls.h"
#include "task.h"

static void copy_str(char *dst, size_t dst_size, const char *src) {
  if (dst == NULL || dst_size == 0) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

static int load_obsidian_root(char *out, size_t out_size) {
  if (out == NULL || out_size == 0) {
    return 0;
  }
  out[0] = '\0';

  const char *root = getenv("OBSIDIAN_PATH");
  if (root != NULL && root[0] != '\0') {
    copy_str(out, out_size, root);
    return 1;
  }
  root = getenv("KENZITUI_OBSIDIAN_PATH");
  if (root != NULL && root[0] != '\0') {
    copy_str(out, out_size, root);
    return 1;
  }

  FILE *f = fopen(".env", "r");
  if (f == NULL) {
    return 0;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f) != NULL) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
      continue;
    }
    const char *value = NULL;
    if (strncmp(line, "OBSIDIAN_PATH=", 14) == 0) {
      value = line + 14;
    } else if (strncmp(line, "KENZITUI_OBSIDIAN_PATH=", 23) == 0) {
      value = line + 23;
    } else {
      continue;
    }

    while (*value == ' ' || *value == '\t') {
      value++;
    }

    copy_str(out, out_size, value);
    remove_trailing_newline(out);
    size_t len = strlen(out);
    if (len >= 2 &&
        ((out[0] == '"' && out[len - 1] == '"') ||
         (out[0] == '\'' && out[len - 1] == '\''))) {
      memmove(out, out + 1, len - 2);
      out[len - 2] = '\0';
      len -= 2;
    }

    if (len > 0) {
      fclose(f);
      return 1;
    }
    out[0] = '\0';
  }

  fclose(f);
  return 0;
}

static int append_char(char *out, size_t out_size, size_t *w, char ch) {
  if (out == NULL || w == NULL || *w + 1 >= out_size) {
    return 0;
  }
  out[(*w)++] = ch;
  out[*w] = '\0';
  return 1;
}

static void normalize_bracket_prefix_folder(const char *name, char *out,
                                            size_t out_size) {
  if (out == NULL || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (name == NULL || name[0] != '[') {
    snprintf(out, out_size, "General");
    return;
  }

  const char *end = strchr(name, ']');
  if (end == NULL || end <= name + 1) {
    snprintf(out, out_size, "General");
    return;
  }

  size_t w = 0;
  for (const char *p = name + 1; p < end; p++) {
    unsigned char ch = (unsigned char)*p;
    if (isalnum(ch)) {
      if (!append_char(out, out_size, &w, (char)ch)) {
        break;
      }
    }
  }

  if (w == 0) {
    snprintf(out, out_size, "General");
  }
}

static void build_task_name_slug(const char *name, char *out, size_t out_size) {
  if (out == NULL || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (name == NULL || name[0] == '\0') {
    snprintf(out, out_size, "Task");
    return;
  }

  const char *text = name;
  if (name[0] == '[') {
    const char *end = strchr(name, ']');
    if (end != NULL) {
      text = end + 1;
    }
  }
  while (*text != '\0' && isspace((unsigned char)*text)) {
    text++;
  }

  size_t w = 0;
  int prev_underscore = 0;
  for (const char *p = text; *p != '\0'; p++) {
    unsigned char ch = (unsigned char)*p;
    int is_invalid =
        (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' ||
         ch == '"' || ch == '<' || ch == '>' || ch == '|');

    if (isalnum(ch)) {
      if (!append_char(out, out_size, &w, (char)ch)) {
        break;
      }
      prev_underscore = 0;
      continue;
    }

    if (isspace(ch) || ch == '-' || ch == '_' || is_invalid) {
      if (!prev_underscore && w > 0) {
        if (!append_char(out, out_size, &w, '_')) {
          break;
        }
        prev_underscore = 1;
      }
    }
  }

  while (w > 0 && out[w - 1] == '_') {
    out[--w] = '\0';
  }

  if (w == 0) {
    snprintf(out, out_size, "Task");
  }
}

int task_build_context_path(char *out, size_t out_size, const char *ticket,
                            const char *task_name) {
  if (out == NULL || out_size == 0) {
    return 0;
  }
  out[0] = '\0';

  char root[MAX_PATH];
  if (!load_obsidian_root(root, sizeof(root))) {
    return 0;
  }

  char folder[64];
  char slug[128];
  const char *tid = (ticket != NULL && ticket[0] != '\0') ? ticket : "T0";
  normalize_bracket_prefix_folder(task_name, folder, sizeof(folder));
  build_task_name_slug(task_name, slug, sizeof(slug));

  return snprintf(out, out_size, "%s/%s/%s_%s.md", root, folder, tid, slug) <
         (int)out_size;
}

void task_auto_fill_context_path(Task *task) {
  if (task == NULL || task->context_path[0] != '\0') {
    return;
  }
  char generated[MAX_PATH];
  if (task_build_context_path(generated, sizeof(generated), task->ticket,
                              task->name)) {
    copy_str(task->context_path, sizeof(task->context_path), generated);
  }
}

Task *create_task(int id, const char *name, const char *desc,
                  const char *project, const char *path, Priority priority) {
  Task *new_task = (Task *)malloc(sizeof(Task));
  if (new_task == NULL) {
    return NULL;
  }

  new_task->id = id;
  copy_str(new_task->name, sizeof(new_task->name), name);
  copy_str(new_task->description, sizeof(new_task->description), desc);
  copy_str(new_task->project, sizeof(new_task->project), project);
  copy_str(new_task->path, sizeof(new_task->path), path);
  new_task->context_path[0] = '\0';

  copy_str(new_task->phase, sizeof(new_task->phase), "Backlog");
  new_task->points = 0;
  new_task->tags[0] = '\0';
  snprintf(new_task->ticket, sizeof(new_task->ticket), "T%d", id);
  new_task->next_sprint_meeting[0] = '\0';

  new_task->is_done = false;
  new_task->priority = priority;
  new_task->next = NULL;

  return new_task;
}

void task_set_phab_fields(Task *task, const char *phase, int points,
                          const char *tags, const char *ticket,
                          const char *next_sprint_meeting) {
  if (task == NULL) {
    return;
  }

  if (phase != NULL && phase[0] != '\0') {
    copy_str(task->phase, sizeof(task->phase), phase);
  }
  if (points >= 0) {
    task->points = points;
  }
  if (tags != NULL) {
    copy_str(task->tags, sizeof(task->tags), tags);
  }
  if (ticket != NULL && ticket[0] != '\0') {
    copy_str(task->ticket, sizeof(task->ticket), ticket);
  }
  if (next_sprint_meeting != NULL) {
    copy_str(task->next_sprint_meeting, sizeof(task->next_sprint_meeting),
             next_sprint_meeting);
  }
}

void add_task(Task **head, Task *new_task) {
  if (new_task == NULL) {
    return;
  }

  if (*head == NULL) {
    *head = new_task;
  } else {
    Task *current = *head;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = new_task;
  }
}

void mark_task_done(Task *head, int id) {
  Task *current = head;
  while (current != NULL) {
    if (current->id == id) {
      current->is_done = !current->is_done;
      return;
    }
    current = current->next;
  }
}

void delete_task(Task **head, int id) {
  if (*head == NULL) {
    return;
  }

  Task *current = *head;
  Task *prev = NULL;

  if (current->id == id) {
    *head = current->next;
    free(current);
    return;
  }

  while (current != NULL && current->id != id) {
    prev = current;
    current = current->next;
  }

  if (current == NULL) {
    return;
  }

  prev->next = current->next;
  free(current);
}

void print_all_tasks(Task *head) {
  Task *current = head;
  while (current != NULL) {
    printf("[%s] %s | %s | phase=%s points=%d tags=%s\n",
           current->is_done ? "x" : " ", current->ticket, current->name,
           current->phase, current->points,
           current->tags[0] != '\0' ? current->tags : "-");
    printf("Project: %s | Path: %s | Context: %s\n", current->project,
           current->path,
           current->context_path[0] != '\0' ? current->context_path : "-");
    printf("Next Sprint Meeting: %s\n",
           current->next_sprint_meeting[0] != '\0' ? current->next_sprint_meeting
                                                    : "-");
    printf("Description: %s\n\n", current->description);
    current = current->next;
  }
}

void free_all_tasks(Task *head) {
  Task *current = head;
  while (current != NULL) {
    Task *next_node = current->next;
    free(current);
    current = next_node;
  }
}

void save_tasks_to_file(Task *head, const char *filename) {
  if (filename != NULL && filename[0] != '\0') {
    char backup[512];
    snprintf(backup, sizeof(backup), "%s.bak", filename);
    (void)copy_file_binary(filename, backup);
  }

  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    return;
  }

  Task *current = head;
  while (current != NULL) {
    fprintf(file, "%d,", current->id);

    const char *string_fields[] = {current->name,         current->description,
                                   current->project,      current->path,
                                   current->context_path};
    for (size_t i = 0; i < 5; i++) {
      const char *value = string_fields[i] != NULL ? string_fields[i] : "";
      int needs_quotes = 0;
      for (const char *p = value; *p != '\0'; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
          needs_quotes = 1;
          break;
        }
      }

      if (!needs_quotes) {
        fprintf(file, "%s,", value);
      } else {
        fputc('"', file);
        for (const char *p = value; *p != '\0'; p++) {
          if (*p == '"') {
            fputc('"', file);
          }
          fputc(*p, file);
        }
        fputc('"', file);
        fputc(',', file);
      }
    }

    fprintf(file, "%d,%d,", current->is_done, current->priority);

    const char *phab_fields[] = {
        current->phase, current->tags, current->ticket, current->next_sprint_meeting};
    const int phab_points = current->points;

    // phase
    const char *phase = phab_fields[0] != NULL ? phab_fields[0] : "";
    int phase_quotes = 0;
    for (const char *p = phase; *p != '\0'; p++) {
      if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
        phase_quotes = 1;
        break;
      }
    }
    if (!phase_quotes) {
      fprintf(file, "%s,", phase);
    } else {
      fputc('"', file);
      for (const char *p = phase; *p != '\0'; p++) {
        if (*p == '"') {
          fputc('"', file);
        }
        fputc(*p, file);
      }
      fputc('"', file);
      fputc(',', file);
    }

    // points
    fprintf(file, "%d,", phab_points);

    // tags, ticket, next_sprint_meeting
    for (size_t i = 1; i < 4; i++) {
      const char *value = phab_fields[i] != NULL ? phab_fields[i] : "";
      int needs_quotes = 0;
      for (const char *p = value; *p != '\0'; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
          needs_quotes = 1;
          break;
        }
      }

      if (!needs_quotes) {
        fprintf(file, "%s", value);
      } else {
        fputc('"', file);
        for (const char *p = value; *p != '\0'; p++) {
          if (*p == '"') {
            fputc('"', file);
          }
          fputc(*p, file);
        }
        fputc('"', file);
      }

      if (i < 3) {
        fputc(',', file);
      }
    }

    fputc('\n', file);
    current = current->next;
  }

  fclose(file);
}

static int split_csv(char *line, char **fields, int max_fields) {
  if (line == NULL || fields == NULL || max_fields <= 0) {
    return 0;
  }

  int count = 0;
  char *src = line;
  char *dst = line;
  int in_quotes = 0;
  int at_field_start = 1;

  fields[count++] = dst;

  while (*src != '\0') {
    char ch = *src++;

    if (in_quotes) {
      if (ch == '"') {
        if (*src == '"') {
          *dst++ = '"';
          src++;
        } else {
          in_quotes = 0;
        }
      } else {
        *dst++ = ch;
      }
      at_field_start = 0;
      continue;
    }

    if (ch == ',' && !in_quotes) {
      *dst++ = '\0';
      if (count < max_fields) {
        fields[count++] = dst;
      }
      at_field_start = 1;
      continue;
    }

    if (ch == '"' && at_field_start) {
      in_quotes = 1;
      continue;
    }

    *dst++ = ch;
    at_field_start = 0;
  }

  *dst = '\0';
  return count;
}

Task *load_tasks_from_file(const char *filename, int *last_id) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    return NULL;
  }

  Task *head = NULL;
  *last_id = 0;

  char *line = NULL;
  size_t len = 0;

  while (getline(&line, &len, file) != -1) {
    if (line[0] == '\n' || line[0] == '\0') {
      continue;
    }

    remove_trailing_newline(line);

    char *fields[16];
    int field_count = split_csv(line, fields, 16);
    if (field_count < 7) {
      continue;
    }

    int id = atoi(fields[0]);
    int is_done_idx = 5;
    int priority_idx = 6;
    int phase_idx = 7;
    int points_idx = 8;
    int tags_idx = 9;
    int ticket_idx = 10;
    int next_idx = 11;
    int context_idx = -1;

    // New format:
    // id,name,description,project,path,context_path,is_done,priority,phase,points,tags,ticket,next
    if (field_count >= 13) {
      context_idx = 5;
      is_done_idx = 6;
      priority_idx = 7;
      phase_idx = 8;
      points_idx = 9;
      tags_idx = 10;
      ticket_idx = 11;
      next_idx = 12;
    }

    int is_done = atoi(fields[is_done_idx]);
    int priority = atoi(fields[priority_idx]);

    Task *new_task = create_task(id, fields[1], fields[2], fields[3], fields[4],
                                 (Priority)priority);
    if (new_task == NULL) {
      continue;
    }

    if (context_idx >= 0 && fields[context_idx] != NULL) {
      copy_str(new_task->context_path, sizeof(new_task->context_path),
               fields[context_idx]);
    }

    new_task->is_done = (bool)is_done;

    // Backward compatible: old format has 7 fields, then 12, now 13.
    if (field_count >= 12) {
      int points = atoi(fields[points_idx]);
      task_set_phab_fields(new_task, fields[phase_idx], points, fields[tags_idx],
                           fields[ticket_idx], fields[next_idx]);
    }

    add_task(&head, new_task);
    if (id > *last_id) {
      *last_id = id;
    }
  }

  free(line);
  fclose(file);
  return head;
}
