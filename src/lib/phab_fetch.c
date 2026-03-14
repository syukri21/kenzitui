#define _GNU_SOURCE

#include "phab_fetch.h"
#include "kenzutls.h"
#include "task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ImportedTask {
  int tid;
  char name[MAX_NAME];
  char phase[MAX_PHASE];
  int points;
  char tags[MAX_TAGS];
  char ticket[MAX_TICKET];
  char next_sprint_meeting[MAX_NEXT_MEETING];
} ImportedTask;

typedef struct AssignedPHID {
  char value[64];
} AssignedPHID;

static int find_template_entry(const char *html, const char *phid,
                               const char **out_start, const char **out_end);

static int is_shell_safe(const char *value) {
  return value != NULL && strchr(value, '\'') == NULL;
}

static void sanitize_csv_field(char *value) {
  if (value == NULL) {
    return;
  }
  for (size_t i = 0; value[i] != '\0'; i++) {
    if (value[i] == ',' || value[i] == '\n' || value[i] == '\r') {
      value[i] = ' ';
    }
  }
}

static char *read_all_stream(FILE *stream) {
  size_t cap = 8192;
  size_t len = 0;
  char *buffer = (char *)malloc(cap);
  if (buffer == NULL) {
    return NULL;
  }

  for (;;) {
    if (len + 4096 + 1 > cap) {
      size_t next_cap = cap * 2;
      char *next = (char *)realloc(buffer, next_cap);
      if (next == NULL) {
        free(buffer);
        return NULL;
      }
      buffer = next;
      cap = next_cap;
    }

    size_t n = fread(buffer + len, 1, 4096, stream);
    len += n;

    if (n < 4096) {
      if (feof(stream)) {
        break;
      }
      free(buffer);
      return NULL;
    }
  }

  buffer[len] = '\0';
  return buffer;
}

static char *extract_cookie_from_file(const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }

  char line[4096];
  while (fgets(line, sizeof(line), file) != NULL) {
    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }

    if (strncmp(line, "KENZITUI_PHAB_COOKIE=", 20) != 0 &&
        strncmp(line, "PHAB_COOKIE=", 12) != 0) {
      continue;
    }

    char *eq = strchr(line, '=');
    if (eq == NULL) {
      continue;
    }
    char *value = eq + 1;
    while (*value == ' ' || *value == '\t') {
      value++;
    }

    remove_trailing_newline(value);
    size_t len = strlen(value);
    if (len >= 2 &&
        ((value[0] == '"' && value[len - 1] == '"') ||
         (value[0] == '\'' && value[len - 1] == '\''))) {
      value[len - 1] = '\0';
      value++;
      len -= 2;
    }

    if (len == 0) {
      continue;
    }

    char *cookie = (char *)malloc(len + 1);
    if (cookie == NULL) {
      fclose(file);
      return NULL;
    }
    memcpy(cookie, value, len);
    cookie[len] = '\0';
    fclose(file);
    return cookie;
  }

  fclose(file);
  return NULL;
}

static char *extract_owner_from_file(const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }

  char line[4096];
  while (fgets(line, sizeof(line), file) != NULL) {
    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }

    if (strncmp(line, "KENZITUI_PHAB_USER=", 19) != 0 &&
        strncmp(line, "PHAB_USER=", 10) != 0) {
      continue;
    }

    char *eq = strchr(line, '=');
    if (eq == NULL) {
      continue;
    }
    char *value = eq + 1;
    while (*value == ' ' || *value == '\t') {
      value++;
    }

    remove_trailing_newline(value);
    size_t len = strlen(value);
    if (len >= 2 &&
        ((value[0] == '"' && value[len - 1] == '"') ||
         (value[0] == '\'' && value[len - 1] == '\''))) {
      value[len - 1] = '\0';
      value++;
      len -= 2;
    }

    if (len == 0) {
      continue;
    }

    char *owner = (char *)malloc(len + 1);
    if (owner == NULL) {
      fclose(file);
      return NULL;
    }
    memcpy(owner, value, len);
    owner[len] = '\0';
    fclose(file);
    return owner;
  }

  fclose(file);
  return NULL;
}

static int extract_points_from_card_block(const char *anchor_start,
                                          const char *anchor_end) {
  if (anchor_start == NULL || anchor_end == NULL || anchor_end <= anchor_start) {
    return -1;
  }

  (void)anchor_end;
  const char *search_end = anchor_start + 2200;
  const char *marker = strstr(anchor_start, "phui-workcard-points");
  if (marker == NULL || marker >= search_end) {
    return -1;
  }

  const char *core = strstr(marker, "phui-tag-core");
  if (core == NULL || core >= search_end) {
    return -1;
  }

  const char *cursor = strstr(core, "\\u003e");
  if (cursor != NULL && cursor < search_end) {
    cursor += 6;
  } else {
    cursor = strchr(core, '>');
    if (cursor == NULL || cursor >= search_end) {
      return -1;
    }
    cursor += 1;
  }

  while (cursor < search_end && *cursor != '\0' &&
         isspace((unsigned char)*cursor)) {
    cursor++;
  }

  if (cursor >= search_end || *cursor == '\0' ||
      !isdigit((unsigned char)*cursor)) {
    return -1;
  }

  int points = 0;
  while (cursor < search_end && *cursor != '\0' &&
         isdigit((unsigned char)*cursor)) {
    points = points * 10 + (*cursor - '0');
    cursor++;
  }

  return points;
}

static int extract_points_from_phid_card(const char *html, const char *phid) {
  const char *entry = NULL;
  const char *entry_end = NULL;
  if (!find_template_entry(html, phid, &entry, &entry_end)) {
    return -1;
  }

  return extract_points_from_card_block(entry, entry_end);
}

static char *load_cookie(const char *cookie_source_path) {
  const char *cookie_env = getenv("KENZITUI_PHAB_COOKIE");
  if (cookie_env != NULL && cookie_env[0] != '\0') {
    return strdup(cookie_env);
  }

  cookie_env = getenv("PHAB_COOKIE");
  if (cookie_env != NULL && cookie_env[0] != '\0') {
    return strdup(cookie_env);
  }

  return extract_cookie_from_file(cookie_source_path);
}

static char *load_owner_username(const char *cookie_source_path) {
  const char *owner_env = getenv("KENZITUI_PHAB_USER");
  if (owner_env != NULL && owner_env[0] != '\0') {
    return strdup(owner_env);
  }
  owner_env = getenv("PHAB_USER");
  if (owner_env != NULL && owner_env[0] != '\0') {
    return strdup(owner_env);
  }

  char *from_file = extract_owner_from_file(cookie_source_path);
  if (from_file != NULL && from_file[0] != '\0') {
    return from_file;
  }
  free(from_file);

  owner_env = getenv("USER");
  if (owner_env != NULL && owner_env[0] != '\0') {
    return strdup(owner_env);
  }

  return NULL;
}

static char *fetch_dashboard_html(int sprint_id, const char *cookie) {
  if (!is_shell_safe(cookie)) {
    return NULL;
  }

  char command[2048];
  snprintf(command, sizeof(command),
           "curl -fsSL --compressed -A 'kenzitui-fetch/1.0' -b '%s' "
           "'https://p.cermati.com/project/view/%d/'",
           cookie, sprint_id);

  FILE *pipe = popen(command, "r");
  if (pipe == NULL) {
    return NULL;
  }

  char *html = read_all_stream(pipe);
  int rc = pclose(pipe);
  if (rc != 0) {
    free(html);
    return NULL;
  }

  return html;
}

static void html_entity_decode(char *text) {
  if (text == NULL) {
    return;
  }

  char *src = text;
  char *dst = text;
  while (*src != '\0') {
    if (strncmp(src, "&amp;", 5) == 0) {
      *dst++ = '&';
      src += 5;
    } else if (strncmp(src, "&lt;", 4) == 0) {
      *dst++ = '<';
      src += 4;
    } else if (strncmp(src, "&gt;", 4) == 0) {
      *dst++ = '>';
      src += 4;
    } else if (strncmp(src, "&quot;", 6) == 0) {
      *dst++ = '"';
      src += 6;
    } else if (strncmp(src, "&#39;", 5) == 0) {
      *dst++ = '\'';
      src += 5;
    } else if (strncmp(src, "&nbsp;", 6) == 0) {
      *dst++ = ' ';
      src += 6;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

static void trim_whitespace(char *value) {
  if (value == NULL || value[0] == '\0') {
    return;
  }

  size_t start = 0;
  while (value[start] != '\0' && isspace((unsigned char)value[start])) {
    start++;
  }

  if (start > 0) {
    memmove(value, value + start, strlen(value + start) + 1);
  }

  size_t len = strlen(value);
  while (len > 0 && isspace((unsigned char)value[len - 1])) {
    value[len - 1] = '\0';
    len--;
  }
}

static int task_index_by_tid(ImportedTask *items, size_t count, int tid) {
  for (size_t i = 0; i < count; i++) {
    if (items[i].tid == tid) {
      return (int)i;
    }
  }
  return -1;
}

static int append_imported_task(ImportedTask **items, size_t *count, size_t *cap,
                                int tid, const char *name) {
  if (task_index_by_tid(*items, *count, tid) >= 0) {
    return 1;
  }

  if (*count == *cap) {
    size_t next_cap = (*cap == 0) ? 16 : (*cap * 2);
    ImportedTask *next =
        (ImportedTask *)realloc(*items, next_cap * sizeof(ImportedTask));
    if (next == NULL) {
      return 0;
    }
    *items = next;
    *cap = next_cap;
  }

  ImportedTask *slot = &((*items)[*count]);
  slot->tid = tid;
  strncpy(slot->name, name, MAX_NAME - 1);
  slot->name[MAX_NAME - 1] = '\0';
  sanitize_csv_field(slot->name);
  strncpy(slot->phase, "Backlog", MAX_PHASE - 1);
  slot->phase[MAX_PHASE - 1] = '\0';
  slot->points = 0;
  slot->tags[0] = '\0';
  snprintf(slot->ticket, sizeof(slot->ticket), "T%d", tid);
  slot->next_sprint_meeting[0] = '\0';
  (*count)++;
  return 1;
}

static void set_imported_metadata(ImportedTask *items, size_t count, int tid,
                                  const char *phase, int points,
                                  const char *tags,
                                  const char *next_sprint_meeting) {
  int idx = task_index_by_tid(items, count, tid);
  if (idx < 0) {
    return;
  }

  if (phase != NULL && phase[0] != '\0') {
    strncpy(items[idx].phase, phase, sizeof(items[idx].phase) - 1);
    items[idx].phase[sizeof(items[idx].phase) - 1] = '\0';
  }
  if (points > 0) {
    items[idx].points = points;
  }
  if (tags != NULL && tags[0] != '\0') {
    strncpy(items[idx].tags, tags, sizeof(items[idx].tags) - 1);
    items[idx].tags[sizeof(items[idx].tags) - 1] = '\0';
  }
  if (next_sprint_meeting != NULL && next_sprint_meeting[0] != '\0') {
    strncpy(items[idx].next_sprint_meeting, next_sprint_meeting,
            sizeof(items[idx].next_sprint_meeting) - 1);
    items[idx].next_sprint_meeting[sizeof(items[idx].next_sprint_meeting) - 1] =
        '\0';
  }
}

static void normalize_tag(char *tag) {
  for (size_t i = 0; tag[i] != '\0'; i++) {
    if (tag[i] == '_') {
      tag[i] = '-';
    }
  }
}

static int append_assigned_phid(AssignedPHID **items, size_t *count, size_t *cap,
                                const char *value) {
  for (size_t i = 0; i < *count; i++) {
    if (strcmp((*items)[i].value, value) == 0) {
      return 1;
    }
  }

  if (*count == *cap) {
    size_t next_cap = (*cap == 0) ? 16 : (*cap * 2);
    AssignedPHID *next =
        (AssignedPHID *)realloc(*items, next_cap * sizeof(AssignedPHID));
    if (next == NULL) {
      return 0;
    }
    *items = next;
    *cap = next_cap;
  }

  strncpy((*items)[*count].value, value, sizeof((*items)[*count].value) - 1);
  (*items)[*count].value[sizeof((*items)[*count].value) - 1] = '\0';
  (*count)++;
  return 1;
}

static int parse_assigned_phids(const char *html, const char *owner_username,
                                AssignedPHID **out_items, size_t *out_count) {
  AssignedPHID *items = NULL;
  size_t count = 0;
  size_t cap = 0;
  const char *cursor = html;
  const char *owner_prefix = "&quot;:{&quot;owner&quot;:[1,&quot;";

  while ((cursor = strstr(cursor, "PHID-TASK-")) != NULL) {
    size_t len = 0;
    while ((isalnum((unsigned char)cursor[len]) || cursor[len] == '-') &&
           len < 63) {
      len++;
    }
    if (len == 0) {
      cursor++;
      continue;
    }

    char phid[64];
    memcpy(phid, cursor, len);
    phid[len] = '\0';

    const char *after = cursor + len;
    if (owner_username != NULL &&
        strncmp(after, owner_prefix, strlen(owner_prefix)) == 0) {
      const char *owner = after + strlen(owner_prefix);
      size_t owner_len = strlen(owner_username);
      if (strncasecmp(owner, owner_username, owner_len) == 0 &&
          !append_assigned_phid(&items, &count, &cap, phid)) {
        free(items);
        return 0;
      }
    }
    cursor += len;
  }

  *out_items = items;
  *out_count = count;
  return 1;
}

static int parse_tid_from_encoded(const char *text, int *out_tid,
                                  const char **out_after_tid) {
  const char *task = strstr(text, "\\/T");
  int offset = 3;
  if (task == NULL) {
    task = strstr(text, "/T");
    offset = 2;
  }
  if (task == NULL || !isdigit((unsigned char)task[offset])) {
    return 0;
  }

  int tid = 0;
  const char *cursor = task + offset;
  while (isdigit((unsigned char)*cursor)) {
    tid = (tid * 10) + (*cursor - '0');
    cursor++;
  }

  *out_tid = tid;
  *out_after_tid = cursor;
  return 1;
}

static int find_template_entry(const char *html, const char *phid,
                               const char **out_start, const char **out_end) {
  const char *tmpl = strstr(html, "&quot;templateMap&quot;:{");
  if (tmpl == NULL) {
    return 0;
  }
  const char *tmpl_end = strstr(tmpl, "&quot;orderMaps&quot;:");
  if (tmpl_end == NULL) {
    return 0;
  }

  char key[128];
  snprintf(key, sizeof(key), "&quot;%s&quot;:&quot;", phid);
  const char *entry = strstr(tmpl, key);
  if (entry == NULL || entry >= tmpl_end) {
    return 0;
  }
  entry += strlen(key);

  const char *next = strstr(entry, "&quot;,&quot;PHID-TASK-");
  if (next == NULL || next > tmpl_end) {
    next = strstr(entry, "&quot;},&quot;orderMaps&quot;");
  }
  if (next == NULL || next > tmpl_end) {
    next = tmpl_end;
  }

  *out_start = entry;
  *out_end = next;
  return 1;
}

static int extract_task_from_phid(const char *html, const char *phid, int *out_tid,
                                  char *out_title, size_t title_size) {
  const char *entry = NULL;
  const char *entry_end = NULL;
  if (!find_template_entry(html, phid, &entry, &entry_end)) {
    return 0;
  }

  const char *after_tid = NULL;
  if (!parse_tid_from_encoded(entry, out_tid, &after_tid) || after_tid >= entry_end) {
    return 0;
  }

  const char *gt = strstr(after_tid, "\\u003e");
  if (gt == NULL || gt >= entry_end) {
    snprintf(out_title, title_size, "T%d", *out_tid);
    return 1;
  } else {
    gt += 6;
  }

  const char *lt = strstr(gt, "\\u003c\\/a");
  if (lt == NULL || lt > entry_end) {
    snprintf(out_title, title_size, "T%d", *out_tid);
    return 1;
  }

  size_t len = (size_t)(lt - gt);
  if (len >= title_size) {
    len = title_size - 1;
  }
  memcpy(out_title, gt, len);
  out_title[len] = '\0';

  html_entity_decode(out_title);
  trim_whitespace(out_title);
  sanitize_csv_field(out_title);
  if (out_title[0] == '\0') {
    snprintf(out_title, title_size, "T%d", *out_tid);
  }

  return 1;
}

static int extract_points_from_phid(const char *html, const char *phid) {
  char key[160];
  snprintf(key, sizeof(key), "&quot;%s&quot;:{&quot;points&quot;:", phid);
  const char *pos = strstr(html, key);
  if (pos == NULL) {
    snprintf(key, sizeof(key), "\"%s\":{\"points\":", phid);
    pos = strstr(html, key);
    if (pos == NULL) {
      return -1;
    }
  }

  pos += strlen(key);
  return atoi(pos);
}

static void extract_phase_from_phid(const char *html, const char *phid,
                                    char *phase, size_t phase_size) {
  phase[0] = '\0';
  const char *cursor = html;
  const char *marker =
      "&quot;content&quot;:&quot;Move to column \\u003cstrong\\u003e";

  while ((cursor = strstr(cursor, marker)) != NULL) {
    cursor += strlen(marker);
    const char *phase_end = strstr(cursor, "\\u003c\\/strong");
    if (phase_end == NULL) {
      cursor++;
      continue;
    }

    size_t len = (size_t)(phase_end - cursor);
    if (len >= phase_size) {
      len = phase_size - 1;
    }

    const char *cards = strstr(phase_end, "&quot;cardPHIDs&quot;:[");
    if (cards == NULL) {
      cursor = phase_end + 1;
      continue;
    }
    const char *cards_end = strchr(cards, ']');
    if (cards_end == NULL) {
      cursor = cards + 1;
      continue;
    }

    const char *phid_pos = strstr(cards, phid);
    if (phid_pos != NULL && phid_pos < cards_end) {
      memcpy(phase, cursor, len);
      phase[len] = '\0';
      return;
    }
    cursor = cards_end;
  }
}

static void extract_tags_from_phid(const char *html, const char *phid, char *tags,
                                   size_t tags_size) {
  tags[0] = '\0';
  const char *entry = NULL;
  const char *entry_end = NULL;
  if (!find_template_entry(html, phid, &entry, &entry_end)) {
    return;
  }

  const char *scan = entry;
  size_t written = 0;
  while ((scan = strstr(scan, "\\/tag\\/")) != NULL && scan < entry_end) {
    scan += 7;
    const char *end = strstr(scan, "\\/");
    if (end == NULL || end > entry_end) {
      break;
    }

    char tag[32];
    size_t len = (size_t)(end - scan);
    if (len == 0 || len >= sizeof(tag)) {
      scan = end + 2;
      continue;
    }

    memcpy(tag, scan, len);
    tag[len] = '\0';
    normalize_tag(tag);

    if (strstr(tags, tag) == NULL) {
      if (written > 0 && written + 1 < tags_size) {
        tags[written++] = '|';
      }
      size_t left = tags_size - written - 1;
      size_t copy_len = strlen(tag);
      if (copy_len > left) {
        copy_len = left;
      }
      memcpy(tags + written, tag, copy_len);
      written += copy_len;
      tags[written] = '\0';
      if (written + 1 >= tags_size) {
        break;
      }
    }
    scan = end + 2;
  }
}

static void extract_next_sprint_meeting(const char *html, char *out,
                                        size_t out_size) {
  out[0] = '\0';
  const char *cursor = html;
  while ((cursor = strstr(cursor, "_20")) != NULL) {
    if (strlen(cursor) < 18) {
      break;
    }
    if (isdigit((unsigned char)cursor[1]) && isdigit((unsigned char)cursor[2]) &&
        isdigit((unsigned char)cursor[3]) && isdigit((unsigned char)cursor[4]) &&
        isdigit((unsigned char)cursor[5]) && isdigit((unsigned char)cursor[6]) &&
        isdigit((unsigned char)cursor[7]) && isdigit((unsigned char)cursor[8]) &&
        cursor[9] == '-' && isdigit((unsigned char)cursor[10]) &&
        isdigit((unsigned char)cursor[11]) && isdigit((unsigned char)cursor[12]) &&
        isdigit((unsigned char)cursor[13]) && isdigit((unsigned char)cursor[14]) &&
        isdigit((unsigned char)cursor[15]) && isdigit((unsigned char)cursor[16]) &&
        isdigit((unsigned char)cursor[17])) {
      snprintf(out, out_size, "%.4s-%.2s-%.2s", cursor + 10, cursor + 14,
               cursor + 16);
      return;
    }
    cursor++;
  }
}

static int parse_plain_anchor_tasks(const char *html, ImportedTask **out_items,
                                    size_t *out_count) {
  ImportedTask *items = NULL;
  size_t count = 0;
  size_t cap = 0;
  const char *cursor = html;

  while ((cursor = strstr(cursor, "href=\"/T")) != NULL) {
    cursor += 8;
    if (!isdigit((unsigned char)*cursor)) {
      continue;
    }

    int tid = 0;
    while (isdigit((unsigned char)*cursor)) {
      tid = (tid * 10) + (*cursor - '0');
      cursor++;
    }

    const char *tag_end = strchr(cursor, '>');
    if (tag_end == NULL) {
      break;
    }
    const char *title_end = strchr(tag_end + 1, '<');
    if (title_end == NULL) {
      break;
    }

    char title[MAX_TITLE];
    size_t title_len = (size_t)(title_end - (tag_end + 1));
    if (title_len >= sizeof(title)) {
      title_len = sizeof(title) - 1;
    }
    memcpy(title, tag_end + 1, title_len);
    title[title_len] = '\0';
    html_entity_decode(title);
    trim_whitespace(title);
    if (title[0] == '\0') {
      snprintf(title, sizeof(title), "T%d", tid);
    }

    if (!append_imported_task(&items, &count, &cap, tid, title)) {
      free(items);
      return 0;
    }
    cursor = title_end;
  }

  *out_items = items;
  *out_count = count;
  return 1;
}

static int parse_encoded_anchor_tasks(const char *html, ImportedTask **items,
                                      size_t *count, size_t *cap) {
  const char *start = strstr(html, "&quot;templateMap&quot;:{");
  if (start == NULL) {
    return 1;
  }
  const char *end = strstr(start, "&quot;orderMaps&quot;:");
  if (end == NULL) {
    return 1;
  }

  const char *cursor = start;

  while ((cursor = strstr(cursor, "\\/T")) != NULL && cursor < end) {
    const char *digits = cursor + 3;
    if (!isdigit((unsigned char)*digits)) {
      cursor += 3;
      continue;
    }

    int tid = 0;
    while (isdigit((unsigned char)*digits)) {
      tid = (tid * 10) + (*digits - '0');
      digits++;
    }

    const char *title_start = strstr(digits, "\\u003e");
    if (title_start == NULL) {
      cursor = digits;
      continue;
    }
    title_start += 6;

    const char *title_end = strstr(title_start, "\\u003c\\/a");
    if (title_end == NULL) {
      cursor = title_start;
      continue;
    }

    char title[MAX_TITLE];
    size_t len = (size_t)(title_end - title_start);
    if (len >= sizeof(title)) {
      len = sizeof(title) - 1;
    }
    memcpy(title, title_start, len);
    title[len] = '\0';
    html_entity_decode(title);
    trim_whitespace(title);
    if (title[0] == '\0') {
      snprintf(title, sizeof(title), "T%d", tid);
    }

    if (!append_imported_task(items, count, cap, tid, title)) {
      return 0;
    }
    int points = extract_points_from_card_block(cursor, end);
    if (points > 0) {
      set_imported_metadata(*items, *count, tid, NULL, points, NULL, NULL);
    }
    cursor = title_end + 8;
  }

  return 1;
}

static int parse_tasks_from_html(const char *html, const char *owner_username,
                                 ImportedTask **out_items, size_t *out_count) {
  ImportedTask *items = NULL;
  size_t count = 0;
  size_t cap = 0;

  AssignedPHID *phids = NULL;
  size_t phid_count = 0;
  if (!parse_assigned_phids(html, owner_username, &phids, &phid_count)) {
    return 0;
  }

  char next_sprint_meeting[MAX_NEXT_MEETING];
  extract_next_sprint_meeting(html, next_sprint_meeting,
                              sizeof(next_sprint_meeting));

  for (size_t i = 0; i < phid_count; i++) {
    int tid = 0;
    char name[MAX_NAME];
    if (!extract_task_from_phid(html, phids[i].value, &tid, name,
                                sizeof(name))) {
      continue;
    }

    if (!append_imported_task(&items, &count, &cap, tid, name)) {
      free(items);
      free(phids);
      return 0;
    }

    char phase[MAX_PHASE];
    char tags[MAX_TAGS];
    extract_phase_from_phid(html, phids[i].value, phase, sizeof(phase));
    extract_tags_from_phid(html, phids[i].value, tags, sizeof(tags));
    int points = extract_points_from_phid_card(html, phids[i].value);
    if (points < 0) {
      points = extract_points_from_phid(html, phids[i].value);
    }
    set_imported_metadata(items, count, tid, phase, points, tags,
                          next_sprint_meeting);
  }
  free(phids);

  if (!parse_encoded_anchor_tasks(html, &items, &count, &cap)) {
    free(items);
    return 0;
  }

  if (count == 0) {
    if (!parse_plain_anchor_tasks(html, &items, &count)) {
      return 0;
    }
  }

  *out_items = items;
  *out_count = count;
  return 1;
}

static Task *find_task_by_id(Task *head, int id) {
  Task *current = head;
  while (current != NULL) {
    if (current->id == id) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static size_t count_tasks(Task *head) {
  size_t n = 0;
  for (Task *current = head; current != NULL; current = current->next) {
    n++;
  }
  return n;
}

static void compute_merge_counts(Task *existing_head, ImportedTask *items,
                                 size_t imported_count, size_t *out_updated,
                                 size_t *out_added, size_t *out_kept) {
  size_t updated = 0;
  for (size_t i = 0; i < imported_count; i++) {
    if (find_task_by_id(existing_head, items[i].tid) != NULL) {
      updated++;
    }
  }
  size_t existing_total = count_tasks(existing_head);
  size_t added = (imported_count >= updated) ? (imported_count - updated) : 0;
  size_t kept = (existing_total >= updated) ? (existing_total - updated) : 0;

  *out_updated = updated;
  *out_added = added;
  *out_kept = kept;
}

static int merge_imported_tasks_into_existing(ImportedTask *items, size_t count,
                                              int sprint_id, Task **head,
                                              size_t *updated, size_t *added) {
  if (head == NULL) {
    return 0;
  }

  char project[MAX_PROJECT];
  snprintf(project, sizeof(project), "Sprint %d", sprint_id);
  *updated = 0;
  *added = 0;

  for (size_t i = 0; i < count; i++) {
    Task *existing = find_task_by_id(*head, items[i].tid);
    if (existing != NULL) {
      // Keep user-managed local fields (path/is_done/priority) untouched;
      // refresh only Phabricator-driven fields.
      strncpy(existing->name, items[i].name, sizeof(existing->name) - 1);
      existing->name[sizeof(existing->name) - 1] = '\0';

      char desc[MAX_DESC];
      snprintf(desc, sizeof(desc), "Imported from Phabricator T%d", items[i].tid);
      sanitize_csv_field(desc);
      strncpy(existing->description, desc, sizeof(existing->description) - 1);
      existing->description[sizeof(existing->description) - 1] = '\0';

      strncpy(existing->project, project, sizeof(existing->project) - 1);
      existing->project[sizeof(existing->project) - 1] = '\0';

      task_set_phab_fields(existing, items[i].phase, items[i].points, items[i].tags,
                           items[i].ticket, items[i].next_sprint_meeting);
      (*updated)++;
      continue;
    }

    char desc[MAX_DESC];
    snprintf(desc, sizeof(desc), "Imported from Phabricator T%d", items[i].tid);
    sanitize_csv_field(desc);

    Task *task =
        create_task(items[i].tid, items[i].name, desc, project, "", MEDIUM);
    if (task == NULL) {
      return 0;
    }
    task_set_phab_fields(task, items[i].phase, items[i].points, items[i].tags,
                         items[i].ticket, items[i].next_sprint_meeting);
    add_task(head, task);
    (*added)++;
  }

  return 1;
}

int fetch_sprint_tasks_to_file(int sprint_id, const char *output_path,
                               const char *cookie_source_path) {
  if (sprint_id <= 0 || output_path == NULL || cookie_source_path == NULL) {
    return -1;
  }

  char *cookie = load_cookie(cookie_source_path);
  if (cookie == NULL) {
    fprintf(stderr,
            "Failed to load Phabricator cookie. Set KENZITUI_PHAB_COOKIE / PHAB_COOKIE or define it in .env.\n");
    return -1;
  }

  char *html = fetch_dashboard_html(sprint_id, cookie);
  free(cookie);
  if (html == NULL) {
    fprintf(stderr, "Failed to fetch dashboard HTML for sprint %d.\n", sprint_id);
    return -1;
  }

  ImportedTask *items = NULL;
  size_t count = 0;
  char *owner_username = load_owner_username(cookie_source_path);
  if (!parse_tasks_from_html(html, owner_username, &items, &count)) {
    free(html);
    free(owner_username);
    fprintf(stderr, "Failed to parse tasks from dashboard HTML.\n");
    return -1;
  }
  free(owner_username);
  free(html);

  if (count == 0) {
    free(items);
    fprintf(stderr, "No tasks found for sprint %d.\n", sprint_id);
    return -1;
  }

  int last_id = 0;
  Task *head = load_tasks_from_file(output_path, &last_id);
  (void)last_id;
  size_t updated = 0;
  size_t added = 0;
  if (!merge_imported_tasks_into_existing(items, count, sprint_id, &head, &updated,
                                          &added)) {
    free(items);
    free_all_tasks(head);
    fprintf(stderr, "Failed to merge imported task list.\n");
    return -1;
  }
  free(items);

  save_tasks_to_file(head, output_path);
  free_all_tasks(head);

  printf("Fetched %zu task(s): updated %zu, added %zu (kept non-Phab tasks) into %s\n",
         count, updated, added, output_path);
  return 0;
}

int preview_fetch_sprint_tasks(int sprint_id, const char *output_path,
                               const char *cookie_source_path,
                               size_t *out_fetched, size_t *out_updated,
                               size_t *out_added, size_t *out_kept) {
  if (sprint_id <= 0 || output_path == NULL || cookie_source_path == NULL ||
      out_fetched == NULL || out_updated == NULL || out_added == NULL ||
      out_kept == NULL) {
    return -1;
  }

  *out_fetched = 0;
  *out_updated = 0;
  *out_added = 0;
  *out_kept = 0;

  char *cookie = load_cookie(cookie_source_path);
  if (cookie == NULL) {
    return -1;
  }

  char *html = fetch_dashboard_html(sprint_id, cookie);
  free(cookie);
  if (html == NULL) {
    return -1;
  }

  ImportedTask *items = NULL;
  size_t count = 0;
  char *owner_username = load_owner_username(cookie_source_path);
  int ok = parse_tasks_from_html(html, owner_username, &items, &count);
  free(owner_username);
  free(html);
  if (!ok || count == 0) {
    free(items);
    return -1;
  }

  int last_id = 0;
  Task *existing = load_tasks_from_file(output_path, &last_id);
  (void)last_id;
  compute_merge_counts(existing, items, count, out_updated, out_added, out_kept);
  *out_fetched = count;

  free_all_tasks(existing);
  free(items);
  return 0;
}

int phab_extract_phase_and_points_for_test(const char *html, const char *phid,
                                           char *out_phase,
                                           size_t out_phase_size,
                                           int *out_points) {
  if (html == NULL || phid == NULL || out_phase == NULL || out_points == NULL ||
      out_phase_size == 0) {
    return 0;
  }

  extract_phase_from_phid(html, phid, out_phase, out_phase_size);
  int points = extract_points_from_phid_card(html, phid);
  if (points < 0) {
    points = extract_points_from_phid(html, phid);
  }
  *out_points = points;
  return 1;
}

int phab_merge_tasks_from_html_for_test(const char *html, int sprint_id,
                                        const char *output_path) {
  if (html == NULL || output_path == NULL || sprint_id <= 0) {
    return 0;
  }

  ImportedTask *items = NULL;
  size_t count = 0;
  if (!parse_tasks_from_html(html, "syukri.khairi", &items, &count) ||
      count == 0) {
    free(items);
    return 0;
  }

  int last_id = 0;
  Task *head = load_tasks_from_file(output_path, &last_id);
  (void)last_id;
  size_t updated = 0;
  size_t added = 0;
  int ok =
      merge_imported_tasks_into_existing(items, count, sprint_id, &head, &updated,
                                         &added);
  free(items);
  if (!ok) {
    free_all_tasks(head);
    return 0;
  }

  save_tasks_to_file(head, output_path);
  free_all_tasks(head);
  return 1;
}
