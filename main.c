#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config"

#include "dbg.c"
#include "elf.c"
#include "globals.c"
#include "predefs.c"
#include "lexer.c"
#include "parser.c"

int main()
{
	global_init();

	FILE_PATH = "test/example.c";
	SOURCE = malloc(MAX_SOURCE);
	
    parse(FILE_PATH);

	dbg_file_structure();

	global_release();

	return 0;
}
