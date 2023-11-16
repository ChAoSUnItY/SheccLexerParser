/* Shecc Syntactic analysis */

#include "predefs.c"

void skip_macro_body()
{
    while (!is_newline(peek_char(0))) {
        next_token();
    }

    skip_newline = 1;
    skip_whitespace();
}

void parse_global_statement()
{
    char token[MAX_TOKEN_LEN];
    block_t *block = &BLOCKS[0];

    if (peek_token(T_preproc_include)) {
        next_token();
        preproc_path_parsing = 1;
        next_token();
        preproc_path_parsing = 0;
        token_str(&cur_token, token);

        if (!strcmp(token, "<stdio.h>")) {
            /* ignore, we include libc by default */
        }
    } else if (peek_token(T_preproc_define)) {
        char alias[MAX_VAR_LEN];
        char value[MAX_VAR_LEN];

        next_token();
        accept_token(T_identifier);
        token_str(&cur_token, alias);

        if (peek_token(T_numeric)) {
            next_token();
            token_str(&cur_token, value);
            add_alias(alias, value);
        } else if (peek_token(T_string)) {
            next_token();
            token_str(&cur_token, value);
            add_alias(alias, value);
        } else if (peek_token(T_open_bracket)) {
            next_token();
            token_str(&cur_token, token);
            macro_t *macro = add_macro(token);

            skip_newline = 0;
            while (peek_token(T_identifier)) {
                next_token();
                token_str(&cur_token, alias);
                strcpy(macro->param_defs[macro->num_param_defs++].var_name,
                       alias);
                accept_token(T_comma);
            }

            if (peek_token(T_elipsis)) {
                next_token();
                macro->is_variadic = 1;
            }

            macro->start_source_idx = source_idx;
            skip_macro_body();
        }
    }  else if (peek_token(T_preproc_undef)) {
        char alias[MAX_VAR_LEN];

        next_token();

        preproc_aliasing = 0;
        accept_token(T_identifier);
        token_str(&cur_token, alias);
        preproc_aliasing = 1;

        remove_alias(alias);
        remove_macro(alias);
    } else if (peek_token(T_preproc_error)) {
        next_token(); /* Consumes #error token */

        char error_diagnostic[MAX_LINE_LEN];
        int i = 0;

        while (!is_newline(peek_char(i))) {
            error_diagnostic[i++] = peek_char(i);
        }
        error_diagnostic[i] = 0;

        error(error_diagnostic);
    } else if (peek_token(T_typedef)) {
        
    } else {
        printf("%s\n", token_kind_literals[next_token()->kind]);
        error("Unexpected token");
    }
}

void parse_internal()
{
    /* parser initialization */
    type_t *type;
    func_t *fn;

    /* built-in types */
    type = add_named_type("void");
    type->base_type = TYPE_void;
    type->size = 0;

    type = add_named_type("char");
    type->base_type = TYPE_char;
    type->size = 1;

    type = add_named_type("int");
    type->base_type = TYPE_int;
    type->size = 4;

    add_block(NULL, NULL, NULL); /* global block */
    elf_add_symbol("", 0, 0);    /* undef symbol */

    /* architecture defines */
    add_alias(ARCH_PREDEFINED, "1");

    /* Linux syscall */
    fn = add_func("__syscall");
    fn->va_args = 1;

    /* lexer initialization */
    source_idx = 0;

    do {
        parse_global_statement();
    } while (!peek_token(T_eof));
}

void load_source_file(char *file_path)
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
			load_source_file(path);
		} else {
			add_line(source_idx);
			strcpy(SOURCE + source_idx, buffer);
			source_idx += strlen(buffer);
		}
	}

	fclose(f);
}

void parse(char *file_path)
{
    load_source_file(file_path);
    parse_internal();
}
