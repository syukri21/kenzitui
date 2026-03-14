#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_NAME 100
#define MAX_TITLE MAX_NAME
#define MAX_DESC 256
#define MAX_PROJECT 50
#define MAX_PATH 256
#define MAX_PHASE 32
#define MAX_TAGS 128
#define MAX_TICKET 24
#define MAX_NEXT_MEETING 32

typedef enum { LOW, MEDIUM, HIGH } Priority;

typedef struct Task {
  int id;
  char name[MAX_NAME];
  char description[MAX_DESC];
  char project[MAX_PROJECT];
  char path[MAX_PATH];
  char context_path[MAX_PATH];

  // Phabricator-oriented fields
  char phase[MAX_PHASE];
  int points;
  char tags[MAX_TAGS];
  char ticket[MAX_TICKET];
  char next_sprint_meeting[MAX_NEXT_MEETING];

  bool is_done;
  Priority priority;
  struct Task *next;
} Task;

Task *create_task(int id, const char *name, const char *desc,
                  const char *project, const char *path, Priority priority);
void task_auto_fill_context_path(Task *task);
int task_build_context_path(char *out, size_t out_size, const char *ticket,
                            const char *task_name);
void task_set_phab_fields(Task *task, const char *phase, int points,
                          const char *tags, const char *ticket,
                          const char *next_sprint_meeting);
void add_task(Task **head, Task *new_task);
void mark_task_done(Task *head, int id);
void delete_task(Task **head, int id);
void print_all_tasks(Task *head);
void free_all_tasks(Task *head);
void save_tasks_to_file(Task *head, const char *filename);
Task *load_tasks_from_file(const char *filename, int *last_id);

#endif
