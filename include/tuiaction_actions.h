#ifndef TUIACTION_ACTIONS_H
#define TUIACTION_ACTIONS_H

#include "tuiaction.h"

void tui_show_status_message(const char *message);
int tui_confirm_prompt(const char *prompt);

void tui_action_handle_fetch(TuiAction *action);
void tui_action_nav_vertical(TuiAction *action, int delta_row);
void tui_action_nav_horizontal(TuiAction *action, int delta_col);
void tui_action_toggle_done(TuiAction *action);
void tui_action_delete(TuiAction *action);
void tui_action_add(TuiAction *action);
void tui_action_edit(TuiAction *action);
void tui_action_cycle_priority(TuiAction *action);
void tui_action_move_phase_next(TuiAction *action);
void tui_action_move_phase_prev(TuiAction *action);
void tui_action_open_path(TuiAction *action);
void tui_action_open_context_path(TuiAction *action);

#endif
