#define _GNU_SOURCE

#include "app_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AppConfig g_cfg;

static void trim(char *s) {
  if (s == NULL || s[0] == '\0') {
    return;
  }
  size_t start = 0;
  while (s[start] != '\0' && isspace((unsigned char)s[start])) {
    start++;
  }
  if (start > 0) {
    memmove(s, s + start, strlen(s + start) + 1);
  }
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[len - 1] = '\0';
    len--;
  }
}

static int parse_bool(const char *value) {
  if (value == NULL) {
    return 0;
  }
  if (strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
      strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
    return 1;
  }
  return 0;
}

static int parse_color(const char *value) {
  if (value == NULL) {
    return -1;
  }
  if (strcasecmp(value, "black") == 0) {
    return 0;
  }
  if (strcasecmp(value, "red") == 0) {
    return 1;
  }
  if (strcasecmp(value, "green") == 0) {
    return 2;
  }
  if (strcasecmp(value, "yellow") == 0) {
    return 3;
  }
  if (strcasecmp(value, "blue") == 0) {
    return 4;
  }
  if (strcasecmp(value, "magenta") == 0) {
    return 5;
  }
  if (strcasecmp(value, "cyan") == 0) {
    return 6;
  }
  if (strcasecmp(value, "white") == 0) {
    return 7;
  }
  if (strcasecmp(value, "default") == 0) {
    return -1;
  }
  return atoi(value);
}

static int parse_key(const char *value, int fallback) {
  if (value == NULL || value[0] == '\0') {
    return fallback;
  }
  if (strcasecmp(value, "space") == 0) {
    return ' ';
  }
  return (unsigned char)value[0];
}

const AppConfig *app_config_get(void) { return &g_cfg; }

void app_config_set_defaults(void) {
  memset(&g_cfg, 0, sizeof(g_cfg));

  g_cfg.keys.search = '/';
  g_cfg.keys.clear_search = 'c';
  g_cfg.keys.fetch = 'f';
  g_cfg.keys.add = 'a';
  g_cfg.keys.edit = 'e';
  g_cfg.keys.del = 'd';
  g_cfg.keys.priority = 'p';
  g_cfg.keys.move_next_phase = 'm';
  g_cfg.keys.move_prev_phase = 'M';
  g_cfg.keys.open = 'o';
  g_cfg.keys.open_context = 'O';
  g_cfg.keys.generate_context = '0';
  g_cfg.keys.done = ' ';
  g_cfg.keys.nav_left = 'h';
  g_cfg.keys.nav_up = 'k';
  g_cfg.keys.nav_down = 'j';
  g_cfg.keys.nav_right = 'l';

  g_cfg.colors.title = 6; // cyan
  g_cfg.colors.selected_border = 1; // red
  g_cfg.colors.bracket = 2; // green
  g_cfg.colors.ticket = 6; // cyan
  g_cfg.colors.points = 3; // yellow
  g_cfg.colors.tags = 5; // magenta
  g_cfg.colors.text = 7; // white
  g_cfg.colors.border = 4; // blue
  g_cfg.colors.path = 6; // cyan

  g_cfg.compact.show_tags = 1;
  g_cfg.compact.show_path = 1;
  g_cfg.compact.show_context = 0;
}

void app_config_load(const char *path) {
  app_config_set_defaults();
  if (path == NULL || path[0] == '\0') {
    return;
  }

  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return;
  }

  char line[256];
  while (fgets(line, sizeof(line), f) != NULL) {
    trim(line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    char *eq = strchr(line, '=');
    if (eq == NULL) {
      continue;
    }
    *eq = '\0';
    char *key = line;
    char *value = eq + 1;
    trim(key);
    trim(value);

    if (strcmp(key, "key.search") == 0) {
      g_cfg.keys.search = parse_key(value, g_cfg.keys.search);
    } else if (strcmp(key, "key.clear_search") == 0) {
      g_cfg.keys.clear_search = parse_key(value, g_cfg.keys.clear_search);
    } else if (strcmp(key, "key.fetch") == 0) {
      g_cfg.keys.fetch = parse_key(value, g_cfg.keys.fetch);
    } else if (strcmp(key, "key.add") == 0) {
      g_cfg.keys.add = parse_key(value, g_cfg.keys.add);
    } else if (strcmp(key, "key.edit") == 0) {
      g_cfg.keys.edit = parse_key(value, g_cfg.keys.edit);
    } else if (strcmp(key, "key.delete") == 0) {
      g_cfg.keys.del = parse_key(value, g_cfg.keys.del);
    } else if (strcmp(key, "key.priority") == 0) {
      g_cfg.keys.priority = parse_key(value, g_cfg.keys.priority);
    } else if (strcmp(key, "key.move_next_phase") == 0) {
      g_cfg.keys.move_next_phase = parse_key(value, g_cfg.keys.move_next_phase);
    } else if (strcmp(key, "key.move_prev_phase") == 0) {
      g_cfg.keys.move_prev_phase = parse_key(value, g_cfg.keys.move_prev_phase);
    } else if (strcmp(key, "key.open") == 0) {
      g_cfg.keys.open = parse_key(value, g_cfg.keys.open);
    } else if (strcmp(key, "key.open_context") == 0) {
      g_cfg.keys.open_context = parse_key(value, g_cfg.keys.open_context);
    } else if (strcmp(key, "key.generate_context") == 0) {
      g_cfg.keys.generate_context =
          parse_key(value, g_cfg.keys.generate_context);
    } else if (strcmp(key, "key.done") == 0) {
      g_cfg.keys.done = parse_key(value, g_cfg.keys.done);
    } else if (strcmp(key, "key.nav_left") == 0) {
      g_cfg.keys.nav_left = parse_key(value, g_cfg.keys.nav_left);
    } else if (strcmp(key, "key.nav_up") == 0) {
      g_cfg.keys.nav_up = parse_key(value, g_cfg.keys.nav_up);
    } else if (strcmp(key, "key.nav_down") == 0) {
      g_cfg.keys.nav_down = parse_key(value, g_cfg.keys.nav_down);
    } else if (strcmp(key, "key.nav_right") == 0) {
      g_cfg.keys.nav_right = parse_key(value, g_cfg.keys.nav_right);
    } else if (strcmp(key, "color.title") == 0) {
      g_cfg.colors.title = parse_color(value);
    } else if (strcmp(key, "color.selected_border") == 0) {
      g_cfg.colors.selected_border = parse_color(value);
    } else if (strcmp(key, "color.bracket") == 0) {
      g_cfg.colors.bracket = parse_color(value);
    } else if (strcmp(key, "color.ticket") == 0) {
      g_cfg.colors.ticket = parse_color(value);
    } else if (strcmp(key, "color.points") == 0) {
      g_cfg.colors.points = parse_color(value);
    } else if (strcmp(key, "color.tags") == 0) {
      g_cfg.colors.tags = parse_color(value);
    } else if (strcmp(key, "color.text") == 0) {
      g_cfg.colors.text = parse_color(value);
    } else if (strcmp(key, "color.border") == 0) {
      g_cfg.colors.border = parse_color(value);
    } else if (strcmp(key, "color.path") == 0) {
      g_cfg.colors.path = parse_color(value);
    } else if (strcmp(key, "compact.show_tags") == 0) {
      g_cfg.compact.show_tags = parse_bool(value);
    } else if (strcmp(key, "compact.show_path") == 0) {
      g_cfg.compact.show_path = parse_bool(value);
    } else if (strcmp(key, "compact.show_context") == 0) {
      g_cfg.compact.show_context = parse_bool(value);
    }
  }

  fclose(f);
}
