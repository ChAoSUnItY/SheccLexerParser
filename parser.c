/* Shecc Syntactic analysis */

#include "predefs.c"

int write_symbol(char *data, int len)
{
    int start_len = elf_data_idx;
    elf_write_data_str(data, len);
    return start_len;
}

int get_size(var_t *var, type_t *type)
{
    if (var->is_ptr || var->is_func)
        return PTR_SIZE;
    return type->size;
}

int global_var_idx = 0;
int global_label_idx = 0;
char global_str_buf[MAX_VAR_LEN];

char *gen_name()
{
    sprintf(global_str_buf, ".t%d", global_var_idx++);
    return global_str_buf;
}

char *gen_label()
{
    sprintf(global_str_buf, ".label.%d", global_label_idx++);
    return global_str_buf;
}

var_t *require_var(block_t *blk)
{
    return &blk->locals[blk->next_local++];
}

var_t *operand_stack[MAX_OPERAND_STACK_SIZE];
int operand_stack_idx = 0;

void opstack_push(var_t *var)
{
    operand_stack[operand_stack_idx++] = var;
}

var_t *opstack_pop()
{
    return operand_stack[--operand_stack_idx];
}

int parse_numeric_constant()
{
    int i = 0;
    int val = 0;
    char buffer[MAX_VAR_LEN];
    expect_token(T_numeric);
    token_str(&cur_token, buffer);

    while (buffer[i]) {
        if (i == 1 && buffer[i] == 'x') {
            /* hexadecimal */

            val = 0;
            i = 2;

            while (buffer[i]) {
                char c = buffer[i++];
                val = val << 4;
                if (is_digit(c)) {
                    val += c - '0';
                }
                c |= 32;
                if (c >= 'a' && c <= 'f') {
                    val += (c - 'a') + 10;
                } else {
                    error("Invalid hexadecimal character");
                }
            }

            return val;
        }

        val = val * 10 + buffer[i++] - '0';
    }

    return val;
}

void parse_param_list_decl(func_t *func, int anon);

void parse_inner_var_decl(var_t *var, int anon, int is_param)
{
    var->init_val = 0;
    var->is_ptr = peek_token(T_asterisk);
    next_token();

    if (peek_token(T_open_bracket)) {
        func_t func;
        next_token();
        expect_token(T_asterisk);
        expect_token(T_identifier);
        token_str(&cur_token, var->var_name);
        expect_token(T_close_bracket);
        parse_param_list_decl(&func, 1);
        var->is_func = 1;
    } else {
        if (!anon) {
            expect_token(T_identifier);
            token_str(&cur_token, var->var_name);

            if (!peek_token(T_open_bracket) && !is_param) {
                if (var->is_global) {
                    ph1_ir_t *ir = add_global_ir(OP_allocat);
                    ir->src0 = var;
                    opstack_push(var);
                } else {
                    ph1_ir_t *ir = add_ph1_ir(OP_allocat);
                    ir->src0 = var;
                }
            }
        }

        if (peek_token(T_open_square)) {
            char buffer[10];

            if (peek_token(T_numeric)) {
                var->array_size = parse_numeric_constant();
            } else {
                var->is_ptr++;
            }
        } else {
            var->array_size = 0;
        }

        var->is_func = 0;
    }
}

void parse_full_var_decl(var_t *var, int anon, int is_param)
{
    /* ignore struct definition */
    expect_token(T_struct);
    expect_token(T_identifier);
    token_str(&cur_token, var->type_name);
    parse_inner_var_decl(var, anon, is_param);
}

void parse_partial_var_decl(var_t *var, var_t *template)
{
    strcpy(var->type_name, template->var_name);
    parse_inner_var_decl(var, 0, 0);
}

void parse_param_list_decl(func_t *func, int anon)
{
    int param_cnt = 0;

    expect_token(T_open_bracket);

    while (peek_token(T_identifier)) {
        parse_full_var_decl(&func->param_defs[param_cnt++], anon, 1);
        expect_token(T_comma);
    }

    if (peek_token(T_elipsis)) {
        next_token();
        func->va_args = 1;
        param_cnt = MAX_PARAMS;
    }

    expect_token(T_close_bracket);
    func->num_params = param_cnt;
}

void parse_literal_param(block_t *parent)
{
    ph1_ir_t *ph1_ir;
    var_t *var;
    int index;
    char literal[MAX_TOKEN_LEN];

    expect_token(T_string);
    token_str(&cur_token, literal);
    index = write_symbol(literal, strlen(literal) + 1);

    ph1_ir = add_ph1_ir(OP_load_data_address);
    var = require_var(var);
    strcpy(var->var_name, gen_name());
    var->init_val = index;
    ph1_ir->dest = var;
    opstack_push(var);
}

void parse_numberic_param(block_t *parent, int is_neg)
{
    ph1_ir_t *ph1_ir;
    var_t *var;
    char token[MAX_ID_LEN];
    int value = 0, i = 0;
    char c;

    expect_token(T_numeric);
    token_str(&cur_token, token);

    if (token[0] == '-') {
        is_neg = 1 - is_neg;
        i++;
    }

    if (token[0] == '0' && token[1] == 'x') {
        i = 2;

        do {
            c = token[i++];

            if (is_digit(c)) {
                c -= '0';
            } else {
                c |= 32;

                if (c >= 'a' && c <= 'f') {
                    c = (c - 'a') + 10;
                } else {
                    error("Invalid numeric constant");
                }
            }

            value = (value * 16) + c;
        } while (is_hex(token[i]));
    } else {
        do {
            c = token[i++] - '0';
            value = (value * 10) + c;
        } while (is_digit(token[i]));
    }

    if (is_neg) {
        value = -value;
    }

    ph1_ir = add_ph1_ir(OP_load_constant);
    var = require_var(parent);
    var->init_val = value;
    strcpy(var->var_name, gen_name());
    ph1_ir->dest = var;
    opstack_push(var);
}

void parse_char_param(block_t *parent)
{
    char token[5];
    ph1_ir_t *ph1_ir;
    var_t *var;

    expect_token(T_char);
    token_str(&cur_token, token);

    ph1_ir = add_ph1_ir(OP_load_constant);
    var = require_var(parent);
    var->init_val = token[0];
    strcpy(var->var_name, gen_name());
    ph1_ir->dest = var;
    opstack_push(var);
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

        next_token();
        expect_token(T_identifier);
        token_str(&cur_token, alias);

        if (peek_token(T_numeric)) {
            next_token();
            token_str(&cur_token, token);
            add_alias(alias, token);
        } else if (peek_token(T_string)) {
            next_token();
            token_str(&cur_token, token);
            add_alias(alias, token);
        } else if (peek_token(T_open_bracket)) {
            next_token();
            token_str(&cur_token, token);
            macro_t *macro = add_macro(token);

            skip_newline = 0;
            while (peek_token(T_identifier)) {
                next_token();
                token_str(&cur_token, alias);
                strcpy(macro->param_defs[macro->num_param_defs++].var_name, alias);
                expect_token(T_comma);
            }

            if (peek_token(T_elipsis)) {
                next_token();
                macro->is_variadic = 1;
            }

            macro->start_source_idx = source_idx;
            skip_macro_body();
        }
    } else if (peek_token(T_preproc_undef)) {
        next_token();

        preproc_aliasing = 0;
        expect_token(T_identifier);
        token_str(&cur_token, token);
        preproc_aliasing = 1;

        remove_alias(token);
        remove_macro(token);
    } else if (peek_token(T_preproc_error)) {
        int i = 0;

        next_token(); /* Consumes #error token */

        while (!is_newline(peek_char(i))) {
            token[i++] = peek_char(i);
        }
        token[i] = 0;

        error(token);
    } else if (peek_token(T_typedef)) {
        next_token();

        if (peek_token(T_enum)) {
            int val = 0;
            type_t *type = add_type();
            type->base_type = TYPE_int;
            type->size = 4;

            next_token();
            expect_token(T_open_curly);

            do {
                expect_token(T_identifier);
                token_str(&cur_token, token);

                if (peek_token(T_assign)) {
                    next_token();
                    val = parse_numeric_constant();
                }

                add_constant(token, val++);
            } while (peek_token(T_comma));

            expect_token(T_close_curly);
            expect_token(T_identifier);
            token_str(&cur_token, token);
            strcpy(type->type_name, token);
            expect_token(T_semicolon);
        } else if (peek_token(T_struct)) {
            int i = 0, size = 0;
            type_t *type = add_type();

            next_token();

            if (peek_token(T_identifier)) {
                next_token();
            }

            expect_token(T_open_curly);

            do {
                var_t *var = &type->fields[i++];

            } while (!peek_token(T_close_curly));
        }
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
