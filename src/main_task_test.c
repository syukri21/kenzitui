#define _GNU_SOURCE

#include "kenzutls.h"
#include "task.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void test_shell_quote() {
  char out[128];
  assert(shell_quote_single("abc", out, sizeof(out)) == 1);
  assert(strcmp(out, "'abc'") == 0);

  assert(shell_quote_single("a'b", out, sizeof(out)) == 1);
  assert(strcmp(out, "'a'\\''b'") == 0);

  assert(shell_quote_single("", out, sizeof(out)) == 1);
  assert(strcmp(out, "''") == 0);

  char tiny[4];
  assert(shell_quote_single("abcdef", tiny, sizeof(tiny)) == 0);
}

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

  Task *t3 =
      create_task(3, "Task, \"Quoted\"", "Desc, with \"quotes\"",
                  "Proj,3", "/tmp/a,b", LOW);
  snprintf(t3->context_path, sizeof(t3->context_path),
           "/tmp/ctx,\"quoted\",3.md");
  task_set_phab_fields(t3, "Need CR", 8, "tag-one|tag,with,comma",
                       "T\"3,EX", "2026-04-01");
  add_task(&head, t3);
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
  assert(loaded_head->next->next != NULL);
  assert(loaded_head->next->next->id == 3);
  assert(strcmp(loaded_head->next->next->name, "Task, \"Quoted\"") == 0);
  assert(strcmp(loaded_head->next->next->description, "Desc, with \"quotes\"") ==
         0);
  assert(strcmp(loaded_head->next->next->project, "Proj,3") == 0);
  assert(strcmp(loaded_head->next->next->path, "/tmp/a,b") == 0);
  assert(strcmp(loaded_head->next->next->context_path, "/tmp/ctx,\"quoted\",3.md") ==
         0);
  assert(strcmp(loaded_head->next->next->phase, "Need CR") == 0);
  assert(loaded_head->next->next->points == 8);
  assert(strcmp(loaded_head->next->next->tags, "tag-one|tag,with,comma") == 0);
  assert(strcmp(loaded_head->next->next->ticket, "T\"3,EX") == 0);
  assert(strcmp(loaded_head->next->next->next_sprint_meeting, "2026-04-01") ==
         0);
  assert(last_id == 3);

  // Test Delete
  delete_task(&head, 1);
  assert(head->id == 2);
  delete_task(&head, 2);
  assert(head->id == 3);
  delete_task(&head, 3);
  assert(head == NULL);

  free_all_tasks(loaded_head);
  printf("All logic tests passed!\n");
}

void test_context_generation_and_compat() {
  char out[MAX_PATH];

  char orig_cwd[1024];
  assert(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL);
  char tmpdir[] = "/tmp/kenzitui_ctx_test_XXXXXX";
  assert(mkdtemp(tmpdir) != NULL);
  assert(chdir(tmpdir) == 0);

  unsetenv("OBSIDIAN_PATH");
  unsetenv("KENZITUI_OBSIDIAN_PATH");
  assert(task_build_context_path(out, sizeof(out), "T123",
                                 "[Ledger Service] Create Credit API") == 0);

  FILE *envf = fopen(".env", "w");
  assert(envf != NULL);
  fprintf(envf, "OBSIDIAN_PATH=/vault_from_env\n");
  fclose(envf);
  assert(task_build_context_path(out, sizeof(out), "T123",
                                 "[Ledger Service] Create Credit API") == 1);
  assert(strcmp(out,
                "/vault_from_env/LedgerService/T123_Create_Credit_API.md") ==
         0);
  remove(".env");

  setenv("OBSIDIAN_PATH", "/vault", 1);
  assert(task_build_context_path(out, sizeof(out), "T148272",
                                 "[Ledger Service] Create Credit API") == 1);
  assert(strcmp(out, "/vault/LedgerService/T148272_Create_Credit_API.md") == 0);

  Task *t = create_task(10, "[Ledger Service] Create Credit API", "d", "p",
                        "/tmp", MEDIUM);
  snprintf(t->ticket, sizeof(t->ticket), "T148272");
  task_auto_fill_context_path(t);
  assert(strcmp(t->context_path,
                "/vault/LedgerService/T148272_Create_Credit_API.md") == 0);
  free_all_tasks(t);

  FILE *f = fopen("/tmp/kenzitui_old_format.dat", "w");
  assert(f != NULL);
  fprintf(f, "77,Old,Desc,Proj,/tmp,0,1,Backlog,3,,T77,\n");
  fclose(f);

  int last_id = 0;
  Task *loaded = load_tasks_from_file("/tmp/kenzitui_old_format.dat", &last_id);
  assert(loaded != NULL);
  assert(loaded->id == 77);
  assert(strcmp(loaded->context_path, "") == 0);
  free_all_tasks(loaded);
  remove("/tmp/kenzitui_old_format.dat");

  assert(chdir(orig_cwd) == 0);
  rmdir(tmpdir);
}

int main() {
  test_shell_quote();
  test_task_logic();
  test_context_generation_and_compat();
  return 0;
}
