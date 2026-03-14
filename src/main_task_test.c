#include "task.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_task_logic() {
  Task *head = NULL;
  int last_id = 0;

  // Test Creation
  Task *t1 = create_task(1, "Task 1", "Desc 1", "Proj 1", ".", HIGH);
  task_set_phab_fields(t1, "Doing", 5, "sprint-goal|backend", "T1",
                       "2026-03-27");
  add_task(&head, t1);

  Task *t2 = create_task(2, "Task 2", "Desc 2", "Proj 2", "/tmp", MEDIUM);
  task_set_phab_fields(t2, "Backlog", 3, "", "T2", "");
  add_task(&head, t2);
  assert(head != NULL);
  assert(head->id == 1);
  assert(head->next->id == 2);

  // Test Mark Done
  mark_task_done(head, 1);
  assert(head->is_done == true);
  mark_task_done(head, 1);
  assert(head->is_done == false);

  // Test Save/Load
  save_tasks_to_file(head, "test_tasks.dat");
  Task *loaded_head = load_tasks_from_file("test_tasks.dat", &last_id);
  assert(loaded_head != NULL);
  assert(loaded_head->id == 1);
  assert(strcmp(loaded_head->name, "Task 1") == 0);
  assert(strcmp(loaded_head->phase, "Doing") == 0);
  assert(loaded_head->points == 5);
  assert(strcmp(loaded_head->tags, "sprint-goal|backend") == 0);
  assert(strcmp(loaded_head->ticket, "T1") == 0);
  assert(strcmp(loaded_head->next_sprint_meeting, "2026-03-27") == 0);
  assert(loaded_head->next != NULL);
  assert(loaded_head->next->id == 2);
  assert(strcmp(loaded_head->next->phase, "Backlog") == 0);
  assert(loaded_head->next->points == 3);
  assert(strcmp(loaded_head->next->tags, "") == 0);
  assert(strcmp(loaded_head->next->ticket, "T2") == 0);
  assert(strcmp(loaded_head->next->next_sprint_meeting, "") == 0);
  assert(last_id == 2);

  // Test Delete
  delete_task(&head, 1);
  assert(head->id == 2);
  delete_task(&head, 2);
  assert(head == NULL);

  free_all_tasks(loaded_head);
  printf("All logic tests passed!\n");
}

int main() {
  test_task_logic();
  return 0;
}
