/* Local work debug utility functions */

#include "globals.c"
#include "predefs.c"
#include <stdio.h>

void dbg_file_structure()
{
	char line_string[MAX_LINE_LEN];

	for (int i = 0; i < lines_idx; i++) {
		include_info_t *info = find_include_info(LINES[i]);
		int len = i == lines_idx - 1 ? source_idx - LINES[i] - 1 : LINES[i + 1] - LINES[i] - 1;
		strncpy(line_string, SOURCE + LINES[i], len);
		line_string[len] = 0;

		printf("[(%-20s) %-5d (idx: %-5d)]: %s\n", info->include_file_path, i + 1, LINES[i], line_string);
	}
}
