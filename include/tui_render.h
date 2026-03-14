#ifndef TUI_RENDER_H
#define TUI_RENDER_H

#include "task.h"

void init_tui_colors(void);
void display_tasks(Task *head, int selected_id, const char *search_query);

#endif
