
#define _GNU_SOURCE // Required for some environments like older glibc versions
                    // or Windows

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kenzutls.h"
#include "task.h"

Task *create_task(int id, const char *title, const char *desc,
                  const char *project, const char *path, Priority priority) {
  Task *new_task = (Task *)malloc(sizeof(Task));
  if (new_task == NULL)
    return NULL;

  new_task->id = id;
  strncpy(new_task->title, title, MAX_TITLE - 1);
  new_task->title[MAX_TITLE - 1] = '\0';
  strncpy(new_task->description, desc, MAX_DESC - 1);
  new_task->description[MAX_DESC - 1] = '\0';
  strncpy(new_task->project, project, MAX_PROJECT - 1);
  new_task->project[MAX_PROJECT - 1] = '\0';
  strncpy(new_task->path, path, MAX_PATH - 1);
  new_task->path[MAX_PATH - 1] = '\0';
  new_task->is_done = false;
  new_task->priority = priority;
  new_task->next = NULL; // Initially, it points to nothing

  return new_task;
}

// Add a new task to the end of the list
void add_task(Task **head, Task *new_task) {
  if (new_task == NULL)
    return;

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
      current->is_done = !current->is_done; // Toggle done status
      return;
    }
    current = current->next;
  }
}

void delete_task(Task **head, int id) {
  if (*head == NULL)
    return;

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

  if (current == NULL)
    return;

  prev->next = current->next;
  free(current);
}

void print_all_tasks(Task *head) {
  Task *current = head;
  while (current != NULL) {
    printf("[%s] Task ID: %d | Priority: %d | Project: %s | Path: %s\n",
           current->is_done ? "x" : " ", current->id, current->priority,
           current->project, current->path);
    printf("Title: %s\n", current->title);
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
  if (file == NULL)
    return;

  Task *current = head;
  while (current != NULL) {
    fprintf(file, "%d,%s,%s,%s,%s,%d,%d\n", current->id, current->title,
            current->description, current->project, current->path,
            current->is_done, current->priority);
    current = current->next;
  }

  fclose(file);
}

Task *load_tasks_from_file(const char *filename, int *last_id) {
  FILE *file = fopen(filename, "r");
  if (file == NULL)
    return NULL;

  Task *head = NULL;
  *last_id = 0;

  char *line = NULL;
  size_t len = 0;

  while (getline(&line, &len, file) != -1) {
    if (line[0] == '\n' || line[0] == '\0') {
      continue;
    }

    remove_trailing_newline(line);

    int id, is_done, priority;
    char title[MAX_TITLE];
    char desc[MAX_DESC];
    char project[MAX_PROJECT];
    char path[MAX_PATH];

    char *token = strtok(line, ",");
    if (!token)
      continue;
    id = atoi(token);

    token = strtok(NULL, ",");
    if (!token)
      continue;
    strncpy(title, token, MAX_TITLE - 1);
    title[MAX_TITLE - 1] = '\0';

    token = strtok(NULL, ",");
    if (!token)
      continue;
    strncpy(desc, token, MAX_DESC - 1);
    desc[MAX_DESC - 1] = '\0';

    token = strtok(NULL, ",");
    if (!token)
      continue;
    strncpy(project, token, MAX_PROJECT - 1);
    project[MAX_PROJECT - 1] = '\0';

    token = strtok(NULL, ",");
    if (!token)
      continue;
    strncpy(path, token, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    token = strtok(NULL, ",");
    if (!token)
      continue;
    is_done = atoi(token);

    token = strtok(NULL, ",");
    if (!token)
      continue;
    priority = atoi(token);

    Task *new_task =
        create_task(id, title, desc, project, path, (Priority)priority);
    if (new_task) {
      new_task->is_done = (bool)is_done;
      add_task(&head, new_task);
      if (id > *last_id) {
        *last_id = id;
      }
    }
  }

  free(line);
  fclose(file);
  return head;
}
