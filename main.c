#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "predefs.c"
#include "lexer.c"

void read_file(char *file_path)
{
    char buffer[MAX_LINE_LEN];

    FILE *f = fopen(file_path, "rb");
    if (!f)
        abort();

    for (;;) {
        if (!fgets(buffer, MAX_LINE_LEN, f)) {
            fclose(f);
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
            strcpy(SOURCE + source_idx, buffer);
            source_idx += strlen(buffer);
        }
    }

    fclose(f);
}

int main()
{
    SOURCE = malloc(MAX_SOURCE);
    read_file("test/example.c");
    // rewinds
    source_idx = 0;

    for (next_token(); cur_token.kind != T_eof; next_token())
    {
        char token_string[MAX_TOKEN_LEN];

        token_str(&cur_token, token_string);

        printf("TOKEN(%s, %s)\n", token_kind_literals[cur_token.kind], token_string);
    }

    return 0;
}
