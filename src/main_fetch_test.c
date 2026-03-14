#define _GNU_SOURCE

#include "phab_fetch.h"
#include "task.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void test_extract_phase_and_points_from_card_template() {
  const char *phid = "PHID-TASK-abc123";
  const char *html =
      "&quot;templateMap&quot;:{"
      "&quot;PHID-TASK-abc123&quot;:&quot;"
      "\\/T148276\\u003e[Ledger Service] Create Buckets API\\u003c\\/a"
      "<li class=\\\"phui-oi-attribute\\\">"
      "<span class=\\\"phui-tag-view phui-workcard-points\\\">"
      "<span class=\\\"phui-tag-core \\\">\\u003e5\\u003c\\/span>"
      "</span></li>"
      "&quot;},&quot;orderMaps&quot;:"
      "{&quot;PHID-PCOL-1&quot;:["
      "{&quot;content&quot;:&quot;Move to column \\u003cstrong\\u003eBacklog\\u003c\\/strong"
      "&quot;,&quot;cardPHIDs&quot;:[&quot;PHID-TASK-other&quot;]},"
      "{&quot;content&quot;:&quot;Move to column \\u003cstrong\\u003eDoing\\u003c\\/strong"
      "&quot;,&quot;cardPHIDs&quot;:[&quot;PHID-TASK-abc123&quot;]}"
      "]}";

  char phase[32];
  int points = -1;
  int ok = phab_extract_phase_and_points_for_test(html, phid, phase,
                                                   sizeof(phase), &points);
  assert(ok == 1);
  assert(strcmp(phase, "Doing") == 0);
  assert(points == 5);
}

static void test_extract_points_fallback_from_property_map() {
  const char *phid = "PHID-TASK-fallback";
  const char *html =
      "&quot;templateMap&quot;:{"
      "&quot;PHID-TASK-fallback&quot;:&quot;"
      "\\/T148999\\u003eSome Task\\u003c\\/a"
      "&quot;},&quot;orderMaps&quot;:"
      "{&quot;PHID-PCOL-1&quot;:["
      "{&quot;content&quot;:&quot;Move to column \\u003cstrong\\u003eBacklog\\u003c\\/strong"
      "&quot;,&quot;cardPHIDs&quot;:[&quot;PHID-TASK-fallback&quot;]}"
      "]},"
      "&quot;PHID-TASK-fallback&quot;:{&quot;points&quot;:8}";

  char phase[32];
  int points = -1;
  int ok = phab_extract_phase_and_points_for_test(html, phid, phase,
                                                   sizeof(phase), &points);
  assert(ok == 1);
  assert(strcmp(phase, "Backlog") == 0);
  assert(points == 8);
}

static Task *find_by_id(Task *head, int id) {
  for (Task *current = head; current != NULL; current = current->next) {
    if (current->id == id) {
      return current;
    }
  }
  return NULL;
}

static void test_merge_keeps_local_tasks_and_path() {
  const char *fixture =
      "&quot;templateMap&quot;:{"
      "&quot;PHID-TASK-aa1&quot;:&quot;"
      "\\/T100\\u003eUpdated Task 100\\u003c\\/a"
      "<li><span class=\\\"phui-tag-view phui-workcard-points\\\">"
      "<span class=\\\"phui-tag-core \\\">\\u003e8\\u003c\\/span>"
      "</span></li>"
      "&quot;,&quot;PHID-TASK-aa2&quot;:&quot;"
      "\\/T101\\u003eNew Task 101\\u003c\\/a"
      "<li><span class=\\\"phui-tag-view phui-workcard-points\\\">"
      "<span class=\\\"phui-tag-core \\\">\\u003e3\\u003c\\/span>"
      "</span></li>"
      "&quot;},&quot;orderMaps&quot;:{"
      "&quot;PHID-PCOL-1&quot;:["
      "{&quot;content&quot;:&quot;Move to column \\u003cstrong\\u003eDoing\\u003c\\/strong"
      "&quot;,&quot;cardPHIDs&quot;:[&quot;PHID-TASK-aa1&quot;,&quot;PHID-TASK-aa2&quot;]}"
      "]},"
      "PHID-TASK-aa1&quot;:{&quot;owner&quot;:[1,&quot;syukri.khairi&quot;]},"
      "PHID-TASK-aa2&quot;:{&quot;owner&quot;:[1,&quot;syukri.khairi&quot;]}";

  const char *path = "/tmp/kenzitui_merge_test.dat";
  setenv("OBSIDIAN_PATH", "/vault", 1);
  Task *seed = NULL;
  Task *t100 = create_task(100, "Old Task 100", "old", "Local", "/keep/local",
                           HIGH);
  snprintf(t100->context_path, sizeof(t100->context_path),
           "/keep/context/T100_local.md");
  t100->is_done = true;
  add_task(&seed, t100);
  Task *t999 = create_task(999, "Local Only", "keep", "Local", "/local/only",
                           LOW);
  add_task(&seed, t999);
  save_tasks_to_file(seed, path);
  free_all_tasks(seed);

  int ok = phab_merge_tasks_from_html_for_test(fixture, 3014, path);
  assert(ok == 1);

  int last_id = 0;
  Task *loaded = load_tasks_from_file(path, &last_id);
  assert(loaded != NULL);

  Task *m100 = find_by_id(loaded, 100);
  assert(m100 != NULL);
  assert(strcmp(m100->name, "Updated Task 100") == 0);
  assert(strcmp(m100->phase, "Doing") == 0);
  assert(m100->points == 8);
  assert(strcmp(m100->path, "/keep/local") == 0);
  assert(strcmp(m100->context_path, "/keep/context/T100_local.md") == 0);
  assert(m100->is_done == true);
  assert(m100->priority == HIGH);

  Task *m999 = find_by_id(loaded, 999);
  assert(m999 != NULL);
  assert(strcmp(m999->name, "Local Only") == 0);
  assert(strcmp(m999->path, "/local/only") == 0);
  assert(strcmp(m999->context_path, "") == 0);

  Task *m101 = find_by_id(loaded, 101);
  assert(m101 != NULL);
  assert(strcmp(m101->phase, "Doing") == 0);
  assert(m101->points == 3);
  assert(strcmp(m101->context_path, "/vault/General/T101_New_Task_101.md") == 0);

  free_all_tasks(loaded);
  remove(path);
  unsetenv("OBSIDIAN_PATH");
}

int main() {
  test_extract_phase_and_points_from_card_template();
  test_extract_points_fallback_from_property_map();
  test_merge_keeps_local_tasks_and_path();
  printf("Fetch parser tests passed!\n");
  return 0;
}
