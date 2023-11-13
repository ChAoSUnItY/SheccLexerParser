#ifndef GLOBALS_C
#define GLOBALS_C
#include <stdlib.h>
#include <string.h>
#include "predefs.c"

int *LINES;
int lines_idx = 0;

/* FIXME: Compute the actual line in the file somewhere here */
void add_line(int idx)
{
    LINES[lines_idx++] = idx;
}

int find_line_idx(int idx)
{
    int left_idx = 0, right_idx = lines_idx, mid;

    while (left_idx < right_idx) {
        mid = left_idx + (right_idx - left_idx) / 2;

        if (LINES[mid] < idx)
            left_idx = mid + 1;
        else
            right_idx = mid;
    }

    return left_idx;
}

typedef struct include_info_t {
    int start_idx;
    int end_idx;
    char include_file_path[MAX_FILE_PATH_LEN];
} include_info_t;

include_info_t *INCLUDE_INFOS;
int include_info_idx = 0;

int compute_diff(include_info_t *info)
{
    return info->end_idx - info->start_idx;
}

void add_include_info(int start_idx, int end_idx, char *include_file_path)
{
    include_info_t *info = &INCLUDE_INFOS[include_info_idx++];
    info->start_idx = start_idx;
    info->end_idx = end_idx;
    strcpy(info->include_file_path, include_file_path);
}

include_info_t *find_include_info(int idx)
{
    include_info_t *info = 0;
    int i;

    for (i = 0; i < include_info_idx; i++) {
        info = INCLUDE_INFOS + i;

        if (idx >= info->start_idx && idx <= info->end_idx)
            return info;
    }

    return info;
}

void global_init()
{
    LINES = malloc(MAX_LINES * sizeof(int));
    INCLUDE_INFOS = malloc(MAX_INCLUDE_INFOS * sizeof(include_info_t));
}

void global_free()
{
    free(LINES);
    free(INCLUDE_INFOS);
}

void error(char *msg)
{
    include_info_t *file_path = find_include_info(source_idx);
    int line_idx = find_line_idx(source_idx);
    int line = LINES[line_idx - 1];
    int offset = source_idx - line + 1;

    printf("[%s: Ln %d, Col %d] Error: %s\n", file_path->include_file_path, line_idx, offset, msg);
    // abort();
}

#endif
