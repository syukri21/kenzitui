#include "phab_fetch.h"

#include <assert.h>
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

int main() {
  test_extract_phase_and_points_from_card_template();
  test_extract_points_fallback_from_property_map();
  printf("Fetch parser tests passed!\n");
  return 0;
}
