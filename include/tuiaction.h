#ifndef TUIACTION_H
#define TUIACTION_H

#include "task.h"
#include <stdbool.h>

#define MAX_ACTION_NAME 50
#define MAX_ACTION_DEFINITION 256

#define SEARCH_KEY '/'
#define CLEAR_SEARCH_KEY 'c'
#define FETCH_KEY 'f'
#define ADD_KEY 'a'
#define EDIT_KEY 'e'
#define DELETE_KEY 'd'
#define PRIORITY_KEY 'p'
#define MOVE_KEY 'm'
#define MOVE_BACK_KEY 'M'
#define OPEN_KEY 'o'
#define DONE_KEY ' '
#define NAV_LEFT_KEY 'h'
#define NAV_UP_KEY 'k'
#define NAV_DOWN_KEY 'j'
#define NAV_RIGHT_KEY 'l'

typedef struct TuiAction {
  int ch;

  // App State
  Task **head;
  int *selected_id;
  int *next_id;

  // Search data
  int max_search_len;
  char *search_query;

  // Persistence target; when set, mutating actions can autosave.
  const char *task_file;
} TuiAction;

void execute(TuiAction *action);
void open_search(TuiAction *action);

// UI Helpers
void tui_input(const char *prompt, char *buffer, int max_len);
void tui_get_path_input(const char *prompt, char *buffer, int max_len);
Priority tui_get_priority_input();

#endif
