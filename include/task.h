#ifndef TASK_H
#define TASK_H

#include <stdbool.h>

#define MAX_TITLE 100
#define MAX_DESC 256
#define MAX_PROJECT 50
#define MAX_PATH 256

typedef enum { LOW, MEDIUM, HIGH } Priority;

typedef struct Task {
  int id;
  char title[MAX_TITLE];
  char description[MAX_DESC];
  char project[MAX_PROJECT];
  char path[MAX_PATH];
  bool is_done;
  Priority priority;
  struct Task *next; // This points to the next task in the list!
} Task;

// Function prototypes
Task *create_task(int id, const char *title, const char *desc, const char *project,
                  const char *path, Priority priority);
void add_task(Task **head, Task *new_task);
void mark_task_done(Task *head, int id);
void delete_task(Task **head, int id);
void print_all_tasks(Task *head);
void free_all_tasks(Task *head);
void save_tasks_to_file(Task *head, const char *filename);
Task *load_tasks_from_file(const char *filename, int *last_id);

#endif
