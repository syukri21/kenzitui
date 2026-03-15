#define _GNU_SOURCE

#include "app_config.h"
#include "tuiaction_actions.h"
#include <ncurses.h>

static int canonical_key(int ch) {
  const AppConfig *cfg = app_config_get();
  if (ch == cfg->keys.search)
    return SEARCH_KEY;
  if (ch == cfg->keys.clear_search)
    return CLEAR_SEARCH_KEY;
  if (ch == cfg->keys.fetch)
    return FETCH_KEY;
  if (ch == cfg->keys.add)
    return ADD_KEY;
  if (ch == cfg->keys.edit)
    return EDIT_KEY;
  if (ch == cfg->keys.edit_path)
    return EDIT_PATH_KEY;
  if (ch == cfg->keys.del)
    return DELETE_KEY;
  if (ch == cfg->keys.priority)
    return PRIORITY_KEY;
  if (ch == cfg->keys.move_next_phase)
    return MOVE_KEY;
  if (ch == cfg->keys.move_prev_phase)
    return MOVE_BACK_KEY;
  if (ch == cfg->keys.open)
    return OPEN_KEY;
  if (ch == cfg->keys.open_context)
    return OPEN_CONTEXT_KEY;
  if (ch == cfg->keys.generate_context)
    return GENERATE_CONTEXT_KEY;
  if (ch == cfg->keys.help)
    return HELP_KEY;
  if (ch == cfg->keys.done)
    return DONE_KEY;
  if (ch == cfg->keys.nav_left)
    return NAV_LEFT_KEY;
  if (ch == cfg->keys.nav_up)
    return NAV_UP_KEY;
  if (ch == cfg->keys.nav_down)
    return NAV_DOWN_KEY;
  if (ch == cfg->keys.nav_right)
    return NAV_RIGHT_KEY;
  return ch;
}

void execute(TuiAction *action) {
  if (action == NULL || action->state == NULL) {
    return;
  }

  switch (canonical_key(action->ch)) {
  case SEARCH_KEY:
    open_search(action);
    break;
  case CLEAR_SEARCH_KEY:
    action->state->search_query[0] = '\0';
    break;
  case FETCH_KEY:
    tui_action_handle_fetch(action);
    break;
  case NAV_DOWN_KEY:
  case KEY_DOWN:
    tui_action_nav_vertical(action, +1);
    break;
  case NAV_UP_KEY:
  case KEY_UP:
    tui_action_nav_vertical(action, -1);
    break;
  case NAV_LEFT_KEY:
  case KEY_LEFT:
    tui_action_nav_horizontal(action, -1);
    break;
  case NAV_RIGHT_KEY:
  case KEY_RIGHT:
    tui_action_nav_horizontal(action, +1);
    break;
  case DONE_KEY:
    tui_action_toggle_done(action);
    break;
  case DELETE_KEY:
    tui_action_delete(action);
    break;
  case ADD_KEY:
    tui_action_add(action);
    break;
  case EDIT_KEY:
    tui_action_edit(action);
    break;
  case EDIT_PATH_KEY:
    tui_action_edit_project_path(action);
    break;
  case PRIORITY_KEY:
    tui_action_cycle_priority(action);
    break;
  case MOVE_KEY:
    tui_action_move_phase_next(action);
    break;
  case MOVE_BACK_KEY:
    tui_action_move_phase_prev(action);
    break;
  case OPEN_KEY:
    tui_action_open_path(action);
    break;
  case OPEN_CONTEXT_KEY:
    tui_action_open_context_path(action);
    break;
  case GENERATE_CONTEXT_KEY:
    tui_action_generate_context_path(action);
    break;
  case HELP_KEY:
    tui_show_keybindings_help();
    break;
  default:
    break;
  }
}
