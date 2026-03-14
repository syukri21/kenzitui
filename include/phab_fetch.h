#ifndef PHAB_FETCH_H
#define PHAB_FETCH_H

#include <stddef.h>

int fetch_sprint_tasks_to_file(int sprint_id, const char *output_path,
                               const char *cookie_source_path);
int preview_fetch_sprint_tasks(int sprint_id, const char *output_path,
                               const char *cookie_source_path,
                               size_t *out_fetched, size_t *out_updated,
                               size_t *out_added, size_t *out_kept);
const char *phab_fetch_last_error(void);

int phab_extract_phase_and_points_for_test(const char *html, const char *phid,
                                           char *out_phase,
                                           size_t out_phase_size,
                                           int *out_points);

int phab_merge_tasks_from_html_for_test(const char *html, int sprint_id,
                                        const char *output_path);

#endif
