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

void parse_expr(block_t *parent);

int parse_numeric_constant()
{
    int i = 0;
    int val = 0;
    char buffer[MAX_VAR_LEN];

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

    if (accept_token(T_open_bracket)) {
        func_t func;
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

        if (accept_token(T_open_square)) {
            char buffer[10];

            if (peek_token(T_numeric)) {
                var->array_size = parse_numeric_constant();
                next_token();
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
    accept_token(T_struct);
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
        accept_token(T_comma);
    }

    if (accept_token(T_elipsis)) {
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

void parse_ternary_operation(block_t *parent);

void parse_func_parameters(block_t *parent)
{
    int i, param_num = 0;
    var_t *params[MAX_PARAMS];

    expect_token(T_open_bracket);

    while (!peek_token(T_close_bracket)) {
        parse_expr(parent);
        parse_ternary_operation(parent);

        params[param_num++] = opstack_pop();
        expect_token(T_comma);
    }
    next_token();

    for (i = 0; i < param_num; i++) {
        ph1_ir_t *ph1_ir = add_ph1_ir(OP_push);
        ph1_ir->src0 = params[i];
    }
}

void parse_func_call(func_t *func, block_t *parent)
{
    ph1_ir_t *ph1_ir;

    parse_func_parameters(parent);

    ph1_ir = add_ph1_ir(OP_call);
    ph1_ir->param_num = func->num_params;
    strcpy(ph1_ir->func_name, func->return_def.var_name);
}

void parse_indirect_call(block_t *parent)
{
    ph1_ir_t *ph1_ir;

    parse_func_parameters(parent);

    ph1_ir = add_ph1_ir(OP_indirect);
    ph1_ir->src0 = opstack_pop();
}

ph1_ir_t side_effect[10];
int se_idx = 0;

void parse_lvalue(lvalue_t *lvalue, var_t *var, block_t *parent, int eval, opcode_t prefix_op);

void parse_expr_operand(block_t *parent)
{
    ph1_ir_t *ph1_ir;
    var_t *var;
    int is_neg = 0;

    if (accept_token(T_minus)) {
        is_neg = 1;

        if (!(peek_token(T_numeric) || peek_token(T_identifier) || peek_token(T_open_bracket))) {
            error("Unexpected token after unary minus");
        }
    }

    if (accept_token(T_string)) {
        parse_literal_param(parent);
    } else if (accept_token(T_char)) {
        parse_char_param(parent);
    } else if (accept_token(T_numeric)) {
        parse_numberic_param(parent, is_neg);
    } else if (accept_token(T_log_not)) {
        parse_expr_operand(parent);

        ph1_ir = add_ph1_ir(OP_log_not);
        ph1_ir->src0 = opstack_pop();
        var = require_var(parent);
        strcpy(var->var_name, gen_name());
        ph1_ir->dest = var;
        opstack_push(var);
    } else if (accept_token(T_bit_not)) {
        parse_expr_operand(parent);

        ph1_ir = add_ph1_ir(OP_bit_not);
        ph1_ir->src0 = opstack_pop();
        var = require_var(parent);
        strcpy(var->var_name, gen_name());
        ph1_ir->dest = var;
        opstack_push(var);
    } else if (accept_token(T_ampersand)) {
        char token[MAX_VAR_LEN];
        var_t *var;
        lvalue_t lvalue;

        expect_token(T_identifier);
        token_str(&cur_token, token);
        parse_lvalue(&lvalue, var, parent, 0, OP_generic);

        if (!lvalue.is_reference) {
            ph1_ir = add_ph1_ir(OP_address_of);
            ph1_ir->src0 = opstack_pop();
            var = require_var(parent);
            strcpy(var->var_name, gen_name());
            ph1_ir->dest = var;
            opstack_push(var);
        }
    } else if (accept_token(T_asterisk)) {
        char token[MAX_VAR_LEN];
        var_t *var;
        lvalue_t lvalue;

        /* TODO: Remove hardcoded optional bracket parsing */
        accept_token(T_open_bracket);
        expect_token(T_identifier);
        token_str(&cur_token, token);
        var = find_var(token, parent);
        parse_lvalue(&lvalue, var, parent, 1, OP_generic);
        accept_token(T_close_bracket);

        ph1_ir = add_ph1_ir(OP_read);
        ph1_ir->src0 = opstack_pop();
        var = require_var(parent);

        if (lvalue.is_ptr > 1)
            ph1_ir->size = PTR_SIZE;
        else
            ph1_ir->size = lvalue.type->size;

        strcpy(var->var_name, gen_name());
        ph1_ir->dest = var;
        opstack_push(var);
    } else if (accept_token(T_open_bracket)) {
        parse_expr(parent);
        parse_ternary_operation(parent);
        expect_token(T_close_bracket);
    } else if (accept_token(T_sizeof)) {
        char token[MAX_TYPE_LEN];
        type_t *type;

        expect_token(T_open_bracket);
        expect_token(T_identifier);
        token_str(&cur_token, token);
        expect_token(T_close_bracket);

        type = find_type(token);

        if (!type)
            error("Unable to find type");

        ph1_ir = add_ph1_ir(OP_load_constant);
        var = require_var(parent);
        var->init_val = type->size;
        strcpy(var->var_name, gen_name());
        ph1_ir->dest = var;
        opstack_push(var);
    } else {
        /* function call, constant or variable - read token and determine */
        opcode_t prefix_op = OP_generic;
        char token[MAX_ID_LEN];
        func_t *func;
        var_t *target_var;
        constant_t *constant;
        int macro_param_idx;
        macro_t *macro;

        if (accept_token(T_increment))
            prefix_op = OP_add;
        else if (accept_token(T_decrement))
            prefix_op = OP_sub;

        peek_token(T_identifier);
        token_str(&cur_token, token);

        /* is a constant or variable? */
        constant = find_constant(token);
        target_var = find_var(token, parent);
        func = find_func(token);
        macro_param_idx = find_macro_param_src_idx(token, parent);
        macro = find_macro(token);

        if (!strcmp(token, "__VA_ARGS__")) {
            /* `source_idx` has pointed at the character after __VA_ARGS__ */
            int i, remainder, t = source_idx;
            macro_t *macro = parent->macro;

            if (!macro)
                error("The '__VA_ARGS__' identifier can only be used in macro");
            if (!macro->is_variadic)
                error("Unexpected identifier '__VA_ARGS__'");

            remainder = macro->num_params - macro->num_param_defs;
            for (i = 0; i < remainder; i++) {
                source_idx = macro->params[macro->num_params - remainder + i];
                parse_expr(parent);
            }
            source_idx = t;
        } else if (macro) {
            if (parent->macro)
                error("Nested macro is not yet supported");

            parent->macro = macro;
            macro->num_params = 0;
            expect_token(T_identifier);

            /* `source_idx` has pointed at the first parameter */
            while (!peek_token(T_close_bracket)) {
                macro->params[macro->num_params++] = source_idx;
                do {
                    next_token();
                } while (next_token != T_comma && next_token != T_close_bracket);
            }
            /* move `source_idx` to the macro body */
            macro_return_idx = source_idx;
            source_idx = macro->start_source_idx;
            expect_token(T_close_bracket);

            skip_newline = 0;
            parse_expr(parent);

            /* cleanup */
            skip_newline = 1;
            parent->macro = NULL;
            macro_return_idx = 0;
        } else if (macro_param_idx) {
            /* "expand" the argument from where it comes from */
            int t = source_idx;
            source_idx = macro_param_idx;
            next_token();
            parse_expr(parent);
            source_idx = t;
            next_token();
        } else if (constant) {
            ph1_ir = add_ph1_ir(OP_load_constant);
            var = require_var(parent);
            var->init_val = constant->value;
            strcpy(var->var_name, gen_name());
            ph1_ir->dest = var;
            opstack_push(var);
            expect_token(T_identifier);
        } else if (var) {
            /* evalue lvalue expression */
            lvalue_t lvalue;
            parse_lvalue(&lvalue, var, parent, 1, prefix_op);

            /* is it an indirect call with function pointer? */
            if (peek_token(T_open_bracket)) {
                parse_indirect_call(parent);

                ph1_ir = add_ph1_ir(OP_func_ret);
                var = require_var(parent);
                strcpy(var->var_name, gen_name());
                ph1_ir->dest = var;
                opstack_push(var);
            }
        } else if (func) {
            expect_token(T_identifier);

            if (peek_token(T_open_bracket)) {
                parse_func_call(func, parent);

                ph1_ir = add_ph1_ir(OP_func_ret);
                var = require_var(parent);
                strcpy(var->var_name, gen_name());
                ph1_ir->dest = var;
                opstack_push(var);
            } else {
                /* indirective function pointer assignment */
                var = require_var(parent);
                var->is_func = 1;
                strcpy(var->var_name, token);
                opstack_push(var);
            }
        } else {
            printf("%s\n", token);
            /* unknown expression */
            error("Unrecognized expression token");
        }

        if (is_neg) {
            ph1_ir = add_ph1_ir(OP_negate);
            ph1_ir->src0 = opstack_pop();
            var = require_var(parent);
            strcpy(var->var_name, gen_name());
            ph1_ir->dest = var;
            opstack_push(var);
        }
    }
}

int get_operator_precedence(opcode_t op)
{
    /* https://www.cs.uic.edu/~i109/Notes/COperatorPrecedenceTable.pdf */
    switch (op) {
    case OP_ternary:
        return 3;
    case OP_log_or:
        return 4;
    case OP_log_and:
        return 5;
    case OP_bit_or:
        return 6;
    case OP_bit_xor:
        return 7;
    case OP_bit_and:
        return 8;
    case OP_eq:
    case OP_neq:
        return 9;
    case OP_lt:
    case OP_leq:
    case OP_gt:
    case OP_geq:
        return 10;
    case OP_add:
    case OP_sub:
        return 12;
    case OP_mul:
    case OP_div:
    case OP_mod:
        return 13;
    default:
        return 0;
    }
}

opcode_t get_operator()
{
    opcode_t op = OP_generic;
    if (accept_token(T_plus))
        op = OP_add;
    else if (accept_token(T_minus))
        op = OP_sub;
    else if (accept_token(T_asterisk))
        op = OP_mul;
    else if (accept_token(T_divide))
        op = OP_div;
    else if (accept_token(T_mod))
        op = OP_mod;
    else if (accept_token(T_lshift))
        op = OP_lshift;
    else if (accept_token(T_rshift))
        op = OP_rshift;
    else if (accept_token(T_log_and))
        op = OP_log_and;
    else if (accept_token(T_log_or))
        op = OP_log_or;
    else if (accept_token(T_eq))
        op = OP_eq;
    else if (accept_token(T_noteq))
        op = OP_neq;
    else if (accept_token(T_lt))
        op = OP_lt;
    else if (accept_token(T_le))
        op = OP_leq;
    else if (accept_token(T_gt))
        op = OP_gt;
    else if (accept_token(T_ge))
        op = OP_geq;
    else if (accept_token(T_ampersand))
        op = OP_bit_and;
    else if (accept_token(T_bit_or))
        op = OP_bit_or;
    else if (accept_token(T_bit_xor))
        op = OP_bit_xor;
    else if (accept_token(T_question))
        op = OP_ternary;
    return op;
}

void parse_expr(block_t *parent)
{
    ph1_ir_t *ph1_ir;
    var_t *var;
    opcode_t op;
    opcode_t oper_stack[10];
    int oper_stack_idx = 0;

    parse_expr_operand(parent);

    op = get_operator();
    if (op == OP_generic || op == OP_ternary)
        return;

    oper_stack[oper_stack_idx++] = op;
    parse_expr_operand(parent);
    op = get_operator();

    while (op != OP_generic && op != OP_ternary) {
        if (oper_stack_idx > 0) {
            int same = 0;
            do {
                opcode_t top_op = oper_stack[oper_stack_idx - 1];
                if (get_operator_precedence(top_op) >= get_operator_precedence(op)) {
                    ph1_ir = add_ph1_ir(top_op);
                    ph1_ir->src1 = opstack_pop();
                    ph1_ir->src0 = opstack_pop();
                    var = require_var(parent);
                    strcpy(var->var_name, gen_name());
                    ph1_ir->dest = var;
                    opstack_push(var);

                    oper_stack_idx--;
                } else
                    same = 1;
            } while (oper_stack_idx > 0 && same == 0);
        }
        parse_expr_operand(parent);
        oper_stack[oper_stack_idx++] = op;
        op = get_operator();
    }

    while (oper_stack_idx > 0) {
        ph1_ir = add_ph1_ir(oper_stack[--oper_stack_idx]);
        ph1_ir->src1 = opstack_pop();
        ph1_ir->src0 = opstack_pop();
        var = require_var(parent);
        strcpy(var->var_name, gen_name());
        ph1_ir->dest = var;
        opstack_push(var);
    }
}

void parse_lvalue(lvalue_t *lvalue, var_t *var, block_t *parent, int eval, opcode_t prefix_op)
{
    ph1_ir_t *ph1_ir;
    var_t *vd;
    int is_address_got = 0;
    int is_member = 0;

    /* already peeked and have the variable */
    expect_token(T_identifier);

    lvalue->type = find_type(var->type_name);
    lvalue->size = get_size(var, lvalue->type);
    lvalue->is_ptr = var->is_ptr;
    lvalue->is_func = var->is_func;
    lvalue->is_reference = 0;

    opstack_push(var);

    if (peek_token(T_open_square) || peek_token(T_arrow) || peek_token(T_dot))
        lvalue->is_reference = 1;

    while (peek_token(T_open_square) || peek_token(T_arrow) || peek_token(T_dot)) {
        if (accept_token(T_open_square)) {
            /* var must be either a pointer or an array of some type */
            if (var->is_ptr == 0 && var->array_size == 0)
                error("Cannot apply square operator to non-pointer");

            /* if nested pointer, still pointer */
            if (var->is_ptr <= 1 && var->array_size == 0)
                lvalue->size = lvalue->type->size;

            parse_expr(parent);

            /* multiply by element size */
            if (lvalue->size != 1) {
                ph1_ir = add_ph1_ir(OP_load_constant);
                vd = require_var(parent);
                vd->init_val = lvalue->size;
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);

                ph1_ir = add_ph1_ir(OP_mul);
                ph1_ir->src1 = opstack_pop();
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);
            }

            ph1_ir = add_ph1_ir(OP_add);
            ph1_ir->src1 = opstack_pop();
            ph1_ir->src0 = opstack_pop();
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;
            opstack_push(vd);

            expect_token(T_close_square);
            is_address_got = 1;
            is_member = 1;
            lvalue->is_reference = 1;
        } else {
            char token[MAX_ID_LEN];

            if (accept_token(T_arrow)) {
                /* resolve where the pointer points at from the calculated
                 * address in a structure */
                if (is_member == 1) {
                    ph1_ir = add_ph1_ir(OP_read);
                    ph1_ir->src0 = opstack_pop();
                    vd = require_var(parent);
                    strcpy(vd->var_name, gen_name());
                    ph1_ir->dest = vd;
                    opstack_push(vd);
                    ph1_ir->size = 4;
                }
            } else {
                expect_token(T_dot);

                if (is_address_got == 0) {
                    ph1_ir = add_ph1_ir(OP_address_of);
                    ph1_ir->src0 = opstack_pop();
                    vd = require_var(parent);
                    strcpy(vd->var_name, gen_name());
                    ph1_ir->dest = vd;
                    opstack_push(vd);

                    is_address_got = 1;
                }
            }

            expect_token(T_identifier);
            token_str(&cur_token, token);

            /* change type currently pointed to */
            var = find_member(token, lvalue->type);
            lvalue->type = find_type(var->type_name);
            lvalue->is_ptr = var->is_ptr;
            lvalue->is_func = var->is_func;
            lvalue->size = get_size(var, lvalue->type);

            /* if it is an array, get the address of first element instead of
             * its value */
            if (var->array_size > 0)
                lvalue->is_reference = 0;

            /* move pointer to offset of structure */
            ph1_ir = add_ph1_ir(OP_load_constant);
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = var->offset;
            ph1_ir->dest = vd;
            opstack_push(vd);

            ph1_ir = add_ph1_ir(OP_add);
            ph1_ir->src1 = opstack_pop();
            ph1_ir->src0 = opstack_pop();
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;
            opstack_push(vd);

            is_address_got = 1;
            is_member = 1;
        }
    }

    if (!eval)
        return;

    if (peek_token(T_plus) && (var->is_ptr || var->array_size)) {
        while (peek_token(T_plus) && (var->is_ptr || var->array_size)) {
            expect_token(T_plus);

            if (lvalue->is_reference) {
                ph1_ir = add_ph1_ir(OP_read);
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                ph1_ir->size = lvalue->size;
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);
            }

            parse_expr_operand(parent);

            lvalue->size = lvalue->type->size;

            if (lvalue->size > 1) {
                ph1_ir = add_ph1_ir(OP_load_constant);
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                vd->init_val = lvalue->size;
                ph1_ir->dest = vd;
                opstack_push(vd);

                ph1_ir = add_ph1_ir(OP_mul);
                ph1_ir->src1 = opstack_pop();
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);
            }

            ph1_ir = add_ph1_ir(OP_add);
            ph1_ir->src1 = opstack_pop();
            ph1_ir->src0 = opstack_pop();
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;
            opstack_push(vd);
        }
    } else {
        var_t *t;

        /**
         * If operand is a reference, read the value and push to stack
         * for the incoming addition/subtraction. Otherwise, use the
         * top element of stack as the one of operands and the destination.
         */
        if (lvalue->is_reference) {
            ph1_ir = add_ph1_ir(OP_read);
            ph1_ir->src0 = operand_stack[operand_stack_idx - 1];
            t = require_var(parent);
            ph1_ir->size = lvalue->size;
            strcpy(t->var_name, gen_name());
            ph1_ir->dest = t;
            opstack_push(t);
        }
        if (prefix_op != OP_generic) {
            ph1_ir = add_ph1_ir(OP_load_constant);
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = 1;
            ph1_ir->dest = vd;
            opstack_push(vd);

            ph1_ir = add_ph1_ir(prefix_op);
            ph1_ir->src1 = opstack_pop();
            if (lvalue->is_reference)
                ph1_ir->src0 = opstack_pop();
            else
                ph1_ir->src0 = operand_stack[operand_stack_idx - 1];
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;

            if (lvalue->is_reference) {
                ph1_ir = add_ph1_ir(OP_write);
                ph1_ir->src0 = vd;
                ph1_ir->dest = opstack_pop();
                ph1_ir->size = lvalue->size;
            } else {
                ph1_ir = add_ph1_ir(OP_assign);
                ph1_ir->src0 = vd;
                ph1_ir->dest = operand_stack[operand_stack_idx - 1];
            }
        } else if (peek_token(T_increment) || peek_token(T_decrement)) {
            side_effect[se_idx].op = OP_load_constant;
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = 1;
            side_effect[se_idx].dest = vd;
            se_idx++;

            side_effect[se_idx].op = accept_token(T_increment) ? OP_add : OP_sub;
            side_effect[se_idx].src1 = vd;
            if (lvalue->is_reference)
                side_effect[se_idx].src0 = opstack_pop();
            else
                side_effect[se_idx].src0 = operand_stack[operand_stack_idx - 1];
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            side_effect[se_idx].dest = vd;
            se_idx++;

            if (lvalue->is_reference) {
                side_effect[se_idx].op = OP_write;
                side_effect[se_idx].src0 = vd;
                side_effect[se_idx].dest = opstack_pop();
                side_effect[se_idx].size = lvalue->size;
                opstack_push(t);
                se_idx++;
            } else {
                side_effect[se_idx].op = OP_assign;
                side_effect[se_idx].src0 = vd;
                side_effect[se_idx].dest = operand_stack[operand_stack_idx - 1];
                se_idx++;
            }
        } else {
            if (lvalue->is_reference) {
                /* pop the address and keep the read value */
                t = opstack_pop();
                opstack_pop();
                opstack_push(t);
            }
        }
    }
}

void parse_ternary_operation(block_t *parent)
{
    ph1_ir_t *ph1_ir;
    var_t *vd, *var;
    char true_label[MAX_VAR_LEN], false_label[MAX_VAR_LEN], end_label[MAX_VAR_LEN];

    strcpy(true_label, gen_label());
    strcpy(false_label, gen_label());
    strcpy(end_label, gen_label());

    if (!accept_token(T_question))
        return;

    /* ternary-operator */
    ph1_ir = add_ph1_ir(OP_branch);
    ph1_ir->dest = opstack_pop();
    vd = require_var(parent);
    strcpy(vd->var_name, true_label);
    ph1_ir->src0 = vd;
    vd = require_var(parent);
    strcpy(vd->var_name, false_label);
    ph1_ir->src1 = vd;

    /* true branch */
    ph1_ir = add_ph1_ir(OP_label);
    vd = require_var(parent);
    strcpy(vd->var_name, true_label);
    ph1_ir->src0 = vd;

    parse_expr(parent);
    if (!accept_token(T_colon))
        return;

    ph1_ir = add_ph1_ir(OP_assign);
    ph1_ir->src0 = opstack_pop();
    var = require_var(parent);
    strcpy(var->var_name, gen_name());
    ph1_ir->dest = var;

    /* jump true branch to end of expression */
    ph1_ir = add_ph1_ir(OP_jump);
    vd = require_var(parent);
    strcpy(vd->var_name, end_label);
    ph1_ir->dest = vd;

    /* false branch */
    ph1_ir = add_ph1_ir(OP_label);
    vd = require_var(parent);
    strcpy(vd->var_name, false_label);
    ph1_ir->src0 = vd;
    parse_expr(parent);

    ph1_ir = add_ph1_ir(OP_assign);
    ph1_ir->src0 = opstack_pop();
    ph1_ir->dest = var;

    ph1_ir = add_ph1_ir(OP_label);
    vd = require_var(parent);
    strcpy(vd->var_name, end_label);
    ph1_ir->src0 = vd;

    opstack_push(var);
}

int parse_body_assignment(char *token, block_t *parent, opcode_t prefix_op)
{
    var_t *var = find_local_var(token, parent);

    if (!var)
        var = find_global_var(token);

    if (var) {
        ph1_ir_t *ph1_ir;
        int one = 0;
        opcode_t op = OP_generic;
        lvalue_t lvalue;
        int size = 0;

        /* has memory address that we want to set */
        parse_lvalue(&lvalue, var, parent, 0, OP_generic);
        size = lvalue.size;

        if (accept_token(T_increment)) {
            op = OP_add;
            one = 1;
        } else if (accept_token(T_decrement)) {
            op = OP_sub;
            one = 1;
        } else if (accept_token(T_pluseq)) {
            op = OP_add;
        } else if (accept_token(T_minuseq)) {
            op = OP_sub;
        } else if (accept_token(T_oreq)) {
            op = OP_bit_or;
        } else if (accept_token(T_andeq)) {
            op = OP_bit_and;
        } else if (peek_token(T_open_bracket)) {
            var_t *vd;
            /* dereference lvalue into function address */
            ph1_ir = add_ph1_ir(OP_read);
            ph1_ir->src0 = opstack_pop();
            ph1_ir->size = PTR_SIZE;
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;
            opstack_push(vd);

            parse_indirect_call(parent);
            return 1;
        } else if (prefix_op == OP_generic) {
            expect_token(T_assign);
        } else {
            op = prefix_op;
            one = 1;
        }

        if (op != OP_generic) {
            var_t *vd, *t;
            int increment_size = 1;

            /* if we have a pointer, shift it by element size */
            if (lvalue.is_ptr)
                increment_size = lvalue.type->size;

            /**
             * If operand is a reference, read the value and push to stack
             * for the incoming addition/subtraction. Otherwise, use the
             * top element of stack as the one of operands and the destination.
             */
            if (one == 1) {
                if (lvalue.is_reference) {
                    ph1_ir = add_ph1_ir(OP_read);
                    t = opstack_pop();
                    ph1_ir->src0 = t;
                    ph1_ir->size = lvalue.size;
                    vd = require_var(parent);
                    strcpy(vd->var_name, gen_name());
                    ph1_ir->dest = vd;
                    opstack_push(vd);
                } else
                    t = operand_stack[operand_stack_idx - 1];

                ph1_ir = add_ph1_ir(OP_load_constant);
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                vd->init_val = increment_size;
                ph1_ir->dest = vd;

                ph1_ir = add_ph1_ir(op);
                ph1_ir->src1 = vd;
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;

                if (lvalue.is_reference) {
                    ph1_ir = add_ph1_ir(OP_write);
                    ph1_ir->src0 = vd;
                    ph1_ir->dest = t;
                    ph1_ir->size = size;
                } else {
                    ph1_ir = add_ph1_ir(OP_assign);
                    ph1_ir->src0 = vd;
                    ph1_ir->dest = t;
                }
            } else {
                if (lvalue.is_reference) {
                    ph1_ir = add_ph1_ir(OP_read);
                    t = opstack_pop();
                    ph1_ir->src0 = t;
                    vd = require_var(parent);
                    ph1_ir->size = lvalue.size;
                    strcpy(vd->var_name, gen_name());
                    ph1_ir->dest = vd;
                    opstack_push(vd);
                } else
                    t = operand_stack[operand_stack_idx - 1];

                parse_expr(parent);

                ph1_ir = add_ph1_ir(OP_load_constant);
                vd = require_var(parent);
                vd->init_val = increment_size;
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);

                ph1_ir = add_ph1_ir(OP_mul);
                ph1_ir->src1 = opstack_pop();
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;
                opstack_push(vd);

                ph1_ir = add_ph1_ir(op);
                ph1_ir->src1 = opstack_pop();
                ph1_ir->src0 = opstack_pop();
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;

                if (lvalue.is_reference) {
                    ph1_ir = add_ph1_ir(OP_write);
                    ph1_ir->src0 = vd;
                    ph1_ir->dest = t;
                    ph1_ir->size = lvalue.size;
                } else {
                    ph1_ir = add_ph1_ir(OP_assign);
                    ph1_ir->src0 = vd;
                    ph1_ir->dest = t;
                }
            }
        } else {
            parse_expr(parent);
            parse_ternary_operation(parent);

            if (lvalue.is_func) {
                ph1_ir = add_ph1_ir(OP_write);
                ph1_ir->src0 = opstack_pop();
                ph1_ir->dest = opstack_pop();
            } else if (lvalue.is_reference) {
                ph1_ir = add_ph1_ir(OP_write);
                ph1_ir->src0 = opstack_pop();
                ph1_ir->dest = opstack_pop();
                ph1_ir->size = size;
            } else {
                ph1_ir = add_ph1_ir(OP_assign);
                ph1_ir->src0 = opstack_pop();
                ph1_ir->dest = opstack_pop();
            }
        }

        return 1;
    }

    return 0;
}

int parse_numeric_sconstant()
{
    /* return signed constant */
    int isneg = 0, res;
    char buffer[10];

    if (accept_token(T_minus))
        isneg = 1;
    if (peek_token(T_numeric)) {
        token_str(&cur_token, buffer);
        res = parse_numeric_constant(buffer);
    } else
        error("Invalid value after assignment");

    expect_token(T_numeric);

    if (isneg)
        return (-1) * res;

    return res;
}

int eval_expression_imm(opcode_t op, int op1, int op2)
{
    /* return immediate result */
    int tmp = op2;
    int res = 0;
    switch (op) {
    case OP_add:
        res = op1 + op2;
        break;
    case OP_sub:
        res = op1 - op2;
        break;
    case OP_mul:
        res = op1 * op2;
        break;
    case OP_div:
        res = op1 / op2;
        break;
    case OP_mod:
        /* TODO: provide arithmetic & operation instead of '&=' */
        /* TODO: do optimization for local expression */
        tmp &= (tmp - 1);
        if ((op2 != 0) && (tmp == 0)) {
            res = op1;
            res &= (op2 - 1);
        } else
            res = op1 % op2;
        break;
    case OP_lshift:
        res = op1 << op2;
        break;
    case OP_rshift:
        res = op1 >> op2;
        break;
    case OP_lt:
        res = op1 < op2 ? 1 : 0;
        break;
    case OP_gt:
        res = op1 > op2 ? 1 : 0;
        break;
    case OP_leq:
        res = op1 <= op2 ? 1 : 0;
        break;
    case OP_geq:
        res = op1 >= op2 ? 1 : 0;
        break;
    default:
        error("The requested operation is not supported.");
    }
    return res;
}

int parse_global_assignment(char *token);

void eval_ternary_imm(int cond, char *token)
{
    if (cond == 0) {
        while (!peek_token(T_colon)) {
            next_token();
        }

        next_token();
        parse_global_assignment(token);
    } else {
        parse_global_assignment(token);
        expect_token(T_colon);

        while (!peek_token(T_semicolon)) {
            next_token();
        }
    }
}

int parse_global_assignment(char *token)
{
    ph1_ir_t *ph1_ir;
    var_t *vd;
    block_t *parent = &BLOCKS[0];

    /* global initialization must be constant */
    var_t *var = find_global_var(token);

    if (var) {
        opcode_t op_stack[10];
        opcode_t op, next_op;
        int val_stack[10];
        int op_stack_index = 0, val_stack_index = 0;
        int operand1, operand2;
        operand1 = parse_numeric_sconstant();
        op = get_operator();

        /* only one value after assignment */
        if (op == OP_generic) {
            ph1_ir = add_global_ir(OP_load_constant);
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = operand1;
            ph1_ir->dest = vd;

            ph1_ir = add_global_ir(OP_assign);
            ph1_ir->src0 = vd;
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = opstack_pop();
            return 1;
        } else if (op == OP_ternary) {
            expect_token(T_question);
            eval_ternary_imm(operand1, token);
            return 1;
        }

        operand2 = parse_numeric_sconstant();
        next_op = get_operator();

        if (next_op == OP_generic) {
            /* only two operands, apply and return */
            ph1_ir = add_global_ir(OP_load_constant);
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = eval_expression_imm(op, operand1, operand2);
            ph1_ir->dest = vd;

            ph1_ir = add_global_ir(OP_assign);
            ph1_ir->src0 = vd;
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = opstack_pop();

            return 1;
        } else if (op == OP_ternary) {
            int cond;

            expect_token(T_question);
            cond = eval_expression_imm(op, operand1, operand2);
            eval_ternary_imm(cond, token);

            return 1;
        }

        /* using stack if operands more than two */
        op_stack[op_stack_index++] = op;
        op = next_op;
        val_stack[val_stack_index++] = operand1;
        val_stack[val_stack_index++] = operand2;

        while (op != OP_generic && op != OP_ternary) {
            if (op_stack_index > 0) {
                /* we have a continuation, use stack */
                int same_op = 0;
                do {
                    opcode_t stack_op = op_stack[op_stack_index - 1];
                    if (get_operator_precedence(stack_op) >= get_operator_precedence(op)) {
                        operand1 = val_stack[val_stack_index - 2];
                        operand2 = val_stack[val_stack_index - 1];
                        val_stack_index -= 2;

                        /* apply stack operator and push result back */
                        val_stack[val_stack_index++] = eval_expression_imm(stack_op, operand1, operand2);

                        /* pop op stack */
                        op_stack_index--;
                    } else {
                        same_op = 1;
                    }
                    /* continue util next operation is higher prio */
                } while (op_stack_index > 0 && same_op == 0);
            }
            /* push next operand on stack */
            val_stack[val_stack_index++] = parse_numeric_sconstant();
            /* push operator on stack */
            op_stack[op_stack_index++] = op;
            op = get_operator();
        }
        /* unwind stack and apply operations */
        while (op_stack_index > 0) {
            opcode_t stack_op = op_stack[op_stack_index - 1];

            /* pop stack and apply operators */
            operand1 = val_stack[val_stack_index - 2];
            operand2 = val_stack[val_stack_index - 1];
            val_stack_index -= 2;

            /* apply stack operator and push value back on stack */
            val_stack[val_stack_index++] = eval_expression_imm(stack_op, operand1, operand2);

            if (op_stack_index == 1) {
                if (op == OP_ternary) {
                    expect_token(T_question);
                    eval_ternary_imm(val_stack[0], token);
                } else {
                    ph1_ir = add_global_ir(OP_load_constant);
                    vd = require_var(parent);
                    strcpy(vd->var_name, gen_name());
                    vd->init_val = val_stack[0];
                    ph1_ir->dest = vd;

                    ph1_ir = add_global_ir(OP_assign);
                    ph1_ir->src0 = vd;
                    vd = require_var(parent);
                    strcpy(vd->var_name, gen_name());
                    ph1_ir->dest = opstack_pop();
                }
                return 1;
            }

            /* pop op stack */
            op_stack_index--;
        }
        if (op == OP_ternary) {
            expect_token(T_question);
            eval_ternary_imm(val_stack[0], token);
        } else {
            ph1_ir = add_global_ir(OP_load_constant);
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            vd->init_val = val_stack[0];
            ph1_ir->dest = vd;

            ph1_ir = add_global_ir(OP_assign);
            ph1_ir->src0 = vd;
            vd = require_var(parent);
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = opstack_pop();
        }

        return 1;
    }

    return 0;
}

var_t *break_exit[MAX_NESTING];
int break_exit_idx = 0;
var_t *continue_pos[MAX_NESTING];
int continue_pos_idx = 0;

void perform_side_effect()
{
    int i;

    for (i = 0; i < se_idx; i++) {
        ph1_ir_t *ph1_ir = add_ph1_ir(side_effect[i].op);
        memcpy(ph1_ir, &side_effect[i], sizeof(ph1_ir_t));
    }

    se_idx = 0;
}

void parse_code_block(func_t *func, macro_t *macro, block_t *parent);

void parse_body_statement(block_t *parent)
{
    char token[MAX_ID_LEN];
    ph1_ir_t *ph1_ir;
    macro_t *macro;
    func_t *func;
    type_t *type;
    var_t *vd, *var;
    opcode_t prefix_op = OP_generic;

    /* statement can be:
     *   function call, variable declaration, assignment operation,
     *   keyword, block
     */

    if (peek_token(T_open_curly)) {
        parse_code_block(parent->func, parent->macro, parent);
        return;
    }

    if (accept_token(T_return)) {
        /* return void */
        if (accept_token(T_semicolon)) {
            add_ph1_ir(OP_return);
            return;
        }

        /* get expression value into return value */
        parse_expr(parent);
        parse_ternary_operation(parent);

        /* apply side effect before function return */
        perform_side_effect();
        expect_token(T_semicolon);

        ph1_ir = add_ph1_ir(OP_return);
        ph1_ir->src0 = opstack_pop();
        return;
    }

    if (accept_token(T_if)) {
        char label_true[MAX_VAR_LEN], label_false[MAX_VAR_LEN], label_endif[MAX_VAR_LEN];
        strcpy(label_true, gen_label());
        strcpy(label_false, gen_label());
        strcpy(label_endif, gen_label());

        expect_token(T_open_bracket);
        parse_expr(parent);
        expect_token(T_close_bracket);

        ph1_ir = add_ph1_ir(OP_branch);
        ph1_ir->dest = opstack_pop();
        vd = require_var(parent);
        strcpy(vd->var_name, label_true);
        ph1_ir->src0 = vd;
        vd = require_var(parent);
        strcpy(vd->var_name, label_false);
        ph1_ir->src1 = vd;

        ph1_ir = add_ph1_ir(OP_label);
        vd = require_var(parent);
        strcpy(vd->var_name, label_true);
        ph1_ir->src0 = vd;

        parse_body_statement(parent);

        /* if we have an "else" block, jump to finish */
        if (accept_token(T_else)) {
            /* jump true branch to finish */
            ph1_ir = add_ph1_ir(OP_jump);
            vd = require_var(parent);
            strcpy(vd->var_name, label_endif);
            ph1_ir->dest = vd;

            /* false branch */
            ph1_ir = add_ph1_ir(OP_label);
            vd = require_var(parent);
            strcpy(vd->var_name, label_false);
            ph1_ir->src0 = vd;

            parse_body_statement(parent);

            ph1_ir = add_ph1_ir(OP_label);
            vd = require_var(parent);
            strcpy(vd->var_name, label_endif);
            ph1_ir->src0 = vd;
        } else {
            /* this is done, and link false jump */
            ph1_ir = add_ph1_ir(OP_label);
            vd = require_var(parent);
            strcpy(vd->var_name, label_false);
            ph1_ir->src0 = vd;
        }
        return;
    }

    if (accept_token(T_while)) {
        var_t *var_continue, *var_break;
        char label_start[MAX_VAR_LEN], label_body[MAX_VAR_LEN], label_end[MAX_VAR_LEN];
        strcpy(label_start, gen_label());
        strcpy(label_body, gen_label());
        strcpy(label_end, gen_label());

        ph1_ir = add_ph1_ir(OP_label);
        var_continue = require_var(parent);
        strcpy(var_continue->var_name, label_start);
        ph1_ir->src0 = var_continue;

        continue_pos[continue_pos_idx++] = var_continue;
        var_break = require_var(parent);
        strcpy(var_break->var_name, label_end);
        break_exit[break_exit_idx++] = var_break;

        expect_token(T_open_bracket);
        parse_expr(parent);
        expect_token(T_close_bracket);

        ph1_ir = add_ph1_ir(OP_branch);
        ph1_ir->dest = opstack_pop();
        vd = require_var(parent);
        strcpy(vd->var_name, label_body);
        ph1_ir->src0 = vd;
        vd = require_var(parent);
        strcpy(vd->var_name, label_end);
        ph1_ir->src1 = vd;

        ph1_ir = add_ph1_ir(OP_label);
        vd = require_var(parent);
        strcpy(vd->var_name, label_body);
        ph1_ir->src0 = vd;

        parse_body_statement(parent);

        continue_pos_idx--;
        break_exit_idx--;

        /* create exit jump for breaks */
        ph1_ir = add_ph1_ir(OP_jump);
        vd = require_var(parent);
        strcpy(vd->var_name, label_start);
        ph1_ir->dest = vd;

        ph1_ir = add_ph1_ir(OP_label);
        ph1_ir->src0 = var_break;

        /* workaround to keep variables alive */
        var_continue->init_val = ph1_ir_idx - 1;
        return;
    }

    if (accept_token(T_switch)) {
        char token_string[MAX_TOKEN_LEN];
        var_t *var_break;
        int is_default = 0;
        char true_label[MAX_VAR_LEN], false_label[MAX_VAR_LEN];
        strcpy(true_label, gen_label());
        strcpy(false_label, gen_label());

        expect_token(T_open_bracket);
        parse_expr(parent);
        expect_token(T_close_bracket);

        /* create exit jump for breaks */
        var_break = require_var(parent);
        break_exit[break_exit_idx++] = var_break;

        expect_token(T_open_curly);
        while (peek_token(T_default) || peek_token(T_case)) {
            if (accept_token(T_default))
                is_default = 1;
            else {
                int case_val;

                accept_token(T_case);
                if (peek_token(T_numeric)) {
                    case_val = parse_numeric_constant(token_string);
                    expect_token(T_numeric); /* already read it */
                } else {
                    constant_t *cd = find_constant(token_string);
                    case_val = cd->value;
                    expect_token(T_identifier); /* already read it */
                }

                ph1_ir = add_ph1_ir(OP_load_constant);
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                vd->init_val = case_val;
                ph1_ir->dest = vd;
                opstack_push(vd);

                ph1_ir = add_ph1_ir(OP_eq);
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->src0 = opstack_pop();
                ph1_ir->src1 = operand_stack[operand_stack_idx - 1];
                vd = require_var(parent);
                strcpy(vd->var_name, gen_name());
                ph1_ir->dest = vd;

                ph1_ir = add_ph1_ir(OP_branch);
                ph1_ir->dest = vd;
                vd = require_var(parent);
                strcpy(vd->var_name, true_label);
                ph1_ir->src0 = vd;
                vd = require_var(parent);
                strcpy(vd->var_name, false_label);
                ph1_ir->src1 = vd;
            }
            expect_token(T_colon);

            /* body is optional, can be another case */
            if (!is_default && !peek_token(T_case) && !peek_token(T_close_curly) && !peek_token(T_default)) {
                ph1_ir = add_ph1_ir(OP_label);
                vd = require_var(parent);
                strcpy(vd->var_name, true_label);
                ph1_ir->src0 = vd;

                /* only create new true label at the first line of case body */
                strcpy(true_label, gen_label());
            }

            while (!peek_token(T_case) && !peek_token(T_close_curly) && !peek_token(T_default))
                parse_body_statement(parent);

            ph1_ir = add_ph1_ir(OP_label);
            vd = require_var(parent);
            strcpy(vd->var_name, false_label);
            ph1_ir->src0 = vd;

            /* only create new false label at the last line of case body */
            strcpy(false_label, gen_label());
        }

        /* remove the expression in switch() */
        opstack_pop();
        expect_token(T_close_curly);

        strcpy(var_break->var_name, vd->var_name);
        break_exit_idx--;
        return;
    }

    if (accept_token(T_break)) {
        ph1_ir = add_ph1_ir(OP_jump);
        ph1_ir->dest = break_exit[break_exit_idx - 1];
    }

    if (accept_token(T_continue)) {
        ph1_ir = add_ph1_ir(OP_jump);
        ph1_ir->dest = continue_pos[continue_pos_idx - 1];
    }

    if (accept_token(T_for)) {
        var_t *var_condition, *var_break, *var_inc;
        char cond[MAX_VAR_LEN], body[MAX_VAR_LEN], inc[MAX_VAR_LEN], end[MAX_VAR_LEN];
        strcpy(cond, gen_label());
        strcpy(body, gen_label());
        strcpy(inc, gen_label());
        strcpy(end, gen_label());

        expect_token(T_open_bracket);

        /* setup - execute once */
        if (!accept_token(T_semicolon)) {
            peek_token(T_identifier);
            token_str(&cur_token, token);
            parse_body_assignment(token, parent, OP_generic);
            peek_token(T_semicolon);
        }

        /* condition - check before the loop */
        ph1_ir = add_ph1_ir(OP_label);
        var_condition = require_var(parent);
        strcpy(var_condition->var_name, cond);
        ph1_ir->src0 = var_condition;

        if (!accept_token(T_semicolon)) {
            parse_expr(parent);
            expect_token(T_semicolon);
        } else {
            /* always true */
            ph1_ir = add_ph1_ir(OP_load_constant);
            vd = require_var(parent);
            vd->init_val = 1;
            strcpy(vd->var_name, gen_name());
            ph1_ir->dest = vd;
            opstack_push(vd);
        }

        ph1_ir = add_ph1_ir(OP_branch);
        ph1_ir->dest = opstack_pop();
        vd = require_var(parent);
        strcpy(vd->var_name, body);
        ph1_ir->src0 = vd;
        vd = require_var(parent);
        strcpy(vd->var_name, end);
        ph1_ir->src1 = vd;

        var_break = require_var(parent);
        strcpy(var_break->var_name, end);

        break_exit[break_exit_idx++] = var_break;

        /* increment after each loop */
        ph1_ir = add_ph1_ir(OP_label);
        var_inc = require_var(parent);
        strcpy(var_inc->var_name, inc);
        ph1_ir->src0 = var_inc;

        continue_pos[continue_pos_idx++] = var_inc;

        if (!accept_token(T_close_bracket)) {
            if (accept_token(T_increment))
                prefix_op = OP_add;
            else if (accept_token(T_decrement))
                prefix_op = OP_sub;
            peek_token(T_identifier);
            token_str(&cur_token, token);
            parse_body_assignment(token, parent, prefix_op);
            expect_token(T_close_bracket);
        }

        /* jump back to condition */
        ph1_ir = add_ph1_ir(OP_jump);
        vd = require_var(parent);
        strcpy(vd->var_name, cond);
        ph1_ir->dest = vd;

        /* loop body */
        ph1_ir = add_ph1_ir(OP_label);
        vd = require_var(parent);
        strcpy(vd->var_name, body);
        ph1_ir->src0 = vd;
        parse_body_statement(parent);

        /* jump to increment */
        ph1_ir = add_ph1_ir(OP_jump);
        vd = require_var(parent);
        strcpy(vd->var_name, inc);
        ph1_ir->dest = vd;

        ph1_ir = add_ph1_ir(OP_label);
        ph1_ir->src0 = var_break;

        var_condition->init_val = ph1_ir_idx - 1;

        continue_pos_idx--;
        break_exit_idx--;
        return;
    }

    if (accept_token(T_do)) {
        var_t *var_start, *var_condition, *var_break;

        ph1_ir = add_ph1_ir(OP_label);
        var_start = require_var(parent);
        strcpy(var_start->var_name, gen_label());
        ph1_ir->src0 = var_start;

        var_condition = require_var(parent);
        strcpy(var_condition->var_name, gen_label());

        continue_pos[continue_pos_idx++] = var_condition;

        var_break = require_var(parent);
        strcpy(var_break->var_name, gen_label());

        break_exit[break_exit_idx++] = var_break;

        parse_body_statement(parent);

        expect_token(T_while);
        expect_token(T_open_bracket);

        ph1_ir = add_ph1_ir(OP_label);
        vd = require_var(parent);
        strcpy(vd->var_name, var_condition->var_name);
        ph1_ir->src0 = vd;

        parse_expr(parent);
        expect_token(T_close_bracket);

        ph1_ir = add_ph1_ir(OP_branch);
        ph1_ir->dest = opstack_pop();
        vd = require_var(parent);
        strcpy(vd->var_name, var_start->var_name);
        ph1_ir->src0 = vd;
        vd = require_var(parent);
        strcpy(vd->var_name, var_break->var_name);
        ph1_ir->src1 = vd;

        ph1_ir = add_ph1_ir(OP_label);
        ph1_ir->src0 = var_break;

        var_start->init_val = ph1_ir_idx - 1;
        expect_token(T_semicolon);

        continue_pos_idx--;
        break_exit_idx--;
        return;
    }

    /* empty statement */
    if (accept_token(T_semicolon))
        return;

    /* statement with prefix */
    if (accept_token(T_increment))
        prefix_op = OP_add;
    else if (accept_token(T_decrement))
        prefix_op = OP_sub;
    /* must be an identifier */
    expect_token(T_identifier);
    token_str(&cur_token, token);

    /* is it a variable declaration? */
    type = find_type(token);

    if (type) {
        var = require_var(parent);
        parse_full_var_decl(var, 0, 0);

        if (accept_token(T_assign)) {
            parse_expr(parent);
            parse_ternary_operation(parent);

            ph1_ir = add_ph1_ir(OP_assign);
            ph1_ir->src0 = opstack_pop();
            ph1_ir->dest = var;
        }

        while (accept_token(T_comma)) {
            var_t *nv;

            /* add sequence point at T_comma */
            perform_side_effect();

            /* multiple (partial) declarations */
            nv = require_var(parent);
            parse_partial_var_decl(nv, var); /* partial */

            if (accept_token(T_assign)) {
                parse_expr(parent);

                ph1_ir = add_ph1_ir(OP_assign);
                ph1_ir->src0 = opstack_pop();
                ph1_ir->dest = nv;
            }
        }

        expect_token(T_semicolon);
        return;
    }

    macro = find_macro(token);

    if (macro) {
        if (parent->macro)
            error("Nested macro is not yet supported");

        parent->macro = macro;
        macro->num_params = 0;
        expect_token(T_identifier);

        /* `source_idx` has pointed at the first parameter */
        while (!peek_token(T_close_bracket)) {
            macro->params[macro->num_params++] = source_idx;

            do {
                next_token();
            } while (!peek_token(T_comma) && !peek_token(T_close_bracket));
        }
        /* move `source_idx` to the macro body */
        macro_return_idx = source_idx;
        source_idx = macro->start_source_idx;
        expect_token(T_close_bracket);

        skip_newline = 0;
        parse_body_statement(parent);

        /* cleanup */
        skip_newline = 1;
        parent->macro = NULL;
        macro_return_idx = 0;
        return;
    }

    /* is a function call? */
    func = find_func(token);

    if (func) {
        expect_token(T_identifier);
        parse_func_call(func, parent);
        perform_side_effect();
        expect_token(T_semicolon);
        return;
    }

    /* is an assignment? */
    if (parse_body_assignment(token, parent, prefix_op)) {
        perform_side_effect();
        expect_token(T_semicolon);
        return;
    }

    error("Unrecognized statement token");
}

void parse_code_block(func_t *func, macro_t *macro, block_t *parent)
{
    block_t *blk = add_block(parent, func, macro);

    add_ph1_ir(OP_block_start);
    expect_token(T_open_curly);

    while (!accept_token(T_close_curly)) {
        parse_body_statement(blk);
        perform_side_effect();
    }

    add_ph1_ir(OP_block_end);
}

void parse_func_body(func_t *fdef)
{
    parse_code_block(fdef, NULL, NULL);
}

/* if first token is type */
void parse_global_decl(block_t *block)
{
    var_t *var = require_var(block);
    var->is_global = 1;

    /* new function, or variables under parent */
    parse_full_var_decl(var, 0, 0);

    if (peek_token(T_open_bracket)) {
        /* function */
        func_t *fd = add_func(var->var_name);
        memcpy(&fd->return_def, var, sizeof(var_t));
        block->next_local--;

        parse_param_list_decl(fd, 0);

        if (peek_token(T_open_curly)) {
            ph1_ir_t *ph1_ir = add_ph1_ir(OP_define);
            strcpy(ph1_ir->func_name, var->var_name);

            parse_func_body(fd);
            return;
        }

        if (accept_token(T_semicolon)) /* forward definition */
            return;

        error("Syntax error in global declaration");
    }

    /* NO support for global initialization */
    /* is a variable */
    if (accept_token(T_assign)) {
        if (var->is_ptr == 0 && var->array_size == 0) {
            parse_global_assignment(var->var_name);
            expect_token(T_semicolon);
            return;
        }
        /* TODO: support global initialization for array and pointer */
        error("Global initialization for array and pointer not supported");
    } else if (accept_token(T_comma))
        /* TODO: continuation */
        error("Global continuation not supported");
    else if (accept_token(T_semicolon)) {
        opstack_pop();
        return;
    }

    error("Syntax error in global declaration");
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
