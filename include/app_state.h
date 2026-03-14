#ifndef APP_STATE_H
#define APP_STATE_H

#include "task.h"

typedef struct AppState {
  Task *head;
  int selected_id;
  int next_id;
  char search_query[MAX_TITLE];
  const char *task_file;
} AppState;

#endif
