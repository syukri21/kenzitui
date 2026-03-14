#ifndef PHAB_FETCH_H
#define PHAB_FETCH_H

int fetch_sprint_tasks_to_file(int sprint_id, const char *output_path,
                               const char *cookie_source_path);

#endif
