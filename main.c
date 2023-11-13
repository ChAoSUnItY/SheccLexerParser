#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "predefs.c"
#include "globals.c"
#include "dbg.c"
#include "lexer.c"

void read_file(char *file_path)
{
    char buffer[MAX_LINE_LEN];
    int start_source_pos = source_idx;

    FILE *f = fopen(file_path, "rb");
    if (!f)
        abort();

    for (;;) {
        if (!fgets(buffer, MAX_LINE_LEN, f)) {
            fclose(f);
            add_include_info(start_source_pos, source_idx, file_path);
            return;
        }
        if (!strncmp(buffer, "#include ", 9) && (buffer[9] == '"')) {
            char path[MAX_LINE_LEN];
            int c = strlen(file_path) - 1;
            while (c > 0 && file_path[c] != '/')
                c--;
            if (c) {
                /* prepend directory name */
                strncpy(path, file_path, c + 1);
                c++;
            }
            path[c] = 0;
            buffer[strlen(buffer) - 2] = 0;
            strcpy(path + c, buffer + 10);
            read_file(path);
        } else {
            add_line(source_idx);
            strcpy(SOURCE + source_idx, buffer);
            source_idx += strlen(buffer);
        }
    }

    fclose(f);
}

int main()
{
    global_init();

    char *file_path = "test/example.c", token_string[MAX_TOKEN_LEN], line_string[MAX_LINE_LEN];
    SOURCE = malloc(MAX_SOURCE);
    read_file(file_path);
    // rewinds
    source_idx = 0;

    for (next_token(); cur_token.kind != T_eof; next_token())
    {
        token_str(&cur_token, token_string);

        printf("TOKEN(%s, %s)\n", token_kind_literals[cur_token.kind], token_string);
    }

    dbg_file_structure();

    global_free();

    return 0;
}
