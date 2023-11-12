#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "predefs.c"
#include "lexer.c"

void read_file(char *file_path)
{
    int sz;

    FILE *f = fopen(file_path, "rb");
    if (!f)
        abort();

    fseek(f, 0L, SEEK_END);
    sz = ftell(f);
    rewind(f);
    
    if(1 != fread(SOURCE, sz, 1, f)) {
        fclose(f);
    }

    fclose(f);
}

int main()
{
    SOURCE = malloc(MAX_SOURCE);
    read_file("example.c");

    for (next_token(); cur_token.kind != T_eof; next_token())
    {
        char token_string[MAX_TOKEN_LEN];

        token_str(&cur_token, token_string);

        printf("TOKEN(%s, %s)\n", token_kind_literals[cur_token.kind], token_string);
    }

    return 0;
}
