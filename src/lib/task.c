#define _GNU_SOURCE

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
    printf("Project: %s | Path: %s\n", current->project, current->path);
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
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    return;
  }

  Task *current = head;
  while (current != NULL) {
    fprintf(file,
            "%d,%s,%s,%s,%s,%d,%d,%s,%d,%s,%s,%s\n",
            current->id, current->name, current->description, current->project,
            current->path, current->is_done, current->priority, current->phase,
            current->points, current->tags, current->ticket,
            current->next_sprint_meeting);
    current = current->next;
  }

  fclose(file);
}

static int split_csv(char *line, char **fields, int max_fields) {
  int count = 0;
  char *cursor = line;

  if (line == NULL || fields == NULL || max_fields <= 0) {
    return 0;
  }

  // Preserve empty fields (",,") and trailing empty fields ("...,").
  while (count < max_fields) {
    fields[count++] = cursor;
    char *comma = strchr(cursor, ',');
    if (comma == NULL) {
      break;
    }
    *comma = '\0';
    cursor = comma + 1;
  }

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
    int is_done = atoi(fields[5]);
    int priority = atoi(fields[6]);

    Task *new_task = create_task(id, fields[1], fields[2], fields[3], fields[4],
                                 (Priority)priority);
    if (new_task == NULL) {
      continue;
    }

    new_task->is_done = (bool)is_done;

    // Backward compatible: old format has 7 fields, new has 12.
    if (field_count >= 12) {
      int points = atoi(fields[8]);
      task_set_phab_fields(new_task, fields[7], points, fields[9], fields[10],
                           fields[11]);
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
