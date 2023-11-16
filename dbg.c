/* Local work debug utility functions */
#ifndef DBG_C
#define DBG_C
#include "globals.c"
#include "predefs.c"
#include <stdio.h>

/* Debug usage, will remove in production */
const char* token_kind_literals[] = {
    "T_numeric",
    "T_identifier",
    "T_linebreak", /* used for preprocessor directive parsing */
    "T_comma",  /* , */
    "T_string", /* null-terminated string */
    "T_char",
    "T_open_bracket",  /* ( */
    "T_close_bracket", /* ) */
    "T_open_curly",    /* { */
    "T_close_curly",   /* } */
    "T_open_square",   /* [ */
    "T_close_square",  /* ] */
    "T_asterisk",      /* '*' */
    "T_divide",        /* / */
    "T_mod",           /* % */
    "T_bit_or",        /* | */
    "T_bit_xor",       /* ^ */
    "T_bit_not",       /* ~ */
    "T_log_and",       /* && */
    "T_log_or",        /* || */
    "T_log_not",       /* ! */
    "T_lt",            /* < */
    "T_gt",            /* > */
    "T_le",            /* <= */
    "T_ge",            /* >= */
    "T_lshift",        /* << */
    "T_rshift",        /* >> */
    "T_dot",           /* . */
    "T_arrow",         /* -> */
    "T_plus",          /* + */
    "T_minus",         /* - */
    "T_minuseq",       /* -= */
    "T_pluseq",        /* += */
    "T_oreq",          /* |= */
    "T_andeq",         /* &= */
    "T_eq",            /* == */
    "T_noteq",         /* != */
    "T_assign",        /* = */
    "T_increment",     /* ++ */
    "T_decrement",     /* -- */
    "T_question",      /* ? */
    "T_colon",         /* : */
    "T_semicolon",     /* ; */
    "T_eof",           /* end-of-file (EOF) */
    "T_ampersand",     /* & */
    "T_return",
    "T_if",
    "T_else",
    "T_while",
    "T_for",
    "T_do",
    "T_typedef",
    "T_enum",
    "T_struct",
    "T_sizeof",
    "T_elipsis", /* ... */
    "T_switch",
    "T_case",
    "T_break",
    "T_default",
    "T_continue",
    "T_preproc_define",
    "T_preproc_path",
    "T_preproc_undef",
    "T_preproc_error",
    "T_preproc_if",
    "T_preproc_elif",
    "T_preproc_ifdef",
    "T_preproc_else",
    "T_preproc_endif",
    "T_preproc_include"
};

void dbg_file_structure()
{
	char line_string[MAX_LINE_LEN];
   int len;

	for (int i = 0; i < lines_idx; i++) {
		include_info_t *info = find_include_info(LINES[i]);
      if (i == lines_idx - 1) {
         strcpy(line_string, SOURCE + LINES[i]);
      } else {
         len = LINES[i + 1] - LINES[i];
         strncpy(line_string, SOURCE + LINES[i], len);
         line_string[len] = 0;
      }
		

		printf("[(%-20s) %-5d (idx: %-5d)]: %s", info->include_file_path, i + 1, LINES[i], line_string);
	}
}

#endif
