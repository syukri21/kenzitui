#ifndef APP_CONFIG_H
#define APP_CONFIG_H

typedef struct AppKeyConfig {
  int search;
  int clear_search;
  int fetch;
  int add;
  int edit;
  int del;
  int priority;
  int move_next_phase;
  int move_prev_phase;
  int open;
  int open_context;
  int generate_context;
  int help;
  int done;
  int nav_left;
  int nav_up;
  int nav_down;
  int nav_right;
} AppKeyConfig;

typedef struct AppColorConfig {
  int title;
  int selected_border;
  int bracket;
  int ticket;
  int points;
  int tags;
  int text;
  int border;
  int path;
} AppColorConfig;

typedef struct AppCompactConfig {
  int show_tags;
  int show_path;
  int show_context;
} AppCompactConfig;

typedef struct AppConfig {
  AppKeyConfig keys;
  AppColorConfig colors;
  AppCompactConfig compact;
} AppConfig;

const AppConfig *app_config_get(void);
void app_config_set_defaults(void);
void app_config_load(const char *path);

#endif
