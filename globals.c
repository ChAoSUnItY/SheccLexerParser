#ifndef GLOBALS_C
#define GLOBALS_C
#include "predefs.c"
#include <stdlib.h>
#include <string.h>

/* Global objects */

block_t *BLOCKS;
int blocks_idx = 0;

macro_t *MACROS;
int macros_idx = 0;

/* the first element is reserved for global scope */
func_t *FUNCS;
int funcs_idx = 1;

/*
 * FUNC_TRIES is used to improve the performance of the find_func function.
 * Instead of searching through all functions and comparing their names, we can
 * utilize the trie data structure to search for existing functions efficiently.
 * The index starts from 1 because the first trie node represents an empty input
 * string, and it is not possible to record a function with an empty name.
 */
trie_t *FUNC_TRIES;
int func_tries_idx = 1;

type_t *TYPES;
int types_idx = 0;

ph1_ir_t *GLOBAL_IR;
int global_ir_idx = 0;

ph1_ir_t *PH1_IR;
int ph1_ir_idx = 0;

ph2_ir_t *PH2_IR;
int ph2_ir_idx = 0;

label_lut_t *LABEL_LUT;
int label_lut_idx = 0;

regfile_t REG[REG_CNT];

alias_t *ALIASES;
int aliases_idx = 0;

constant_t *CONSTANTS;
int constants_idx = 0;

include_info_t *INCLUDE_INFOS;
int include_info_idx = 0;

int *LINES;
int lines_idx = 0;

char *SOURCE;
int source_idx = 0;

char *FILE_PATH;

/* ELF sections */

char *elf_code;
int elf_code_idx = 0;
char *elf_data;
int elf_data_idx = 0;
char *elf_header;
int elf_header_idx = 0;
int elf_header_len = 0x54; /* ELF fixed: 0x34 + 1 * 0x20 */
int elf_code_start;
char *elf_symtab;
char *elf_strtab;
char *elf_section;

/**
 * insert_trie() - Inserts a new element into the trie structure.
 * @trie: A pointer to the trie where the name will be inserted.
 * @name: The name to be inserted into the trie.
 * @funcs_index: The index of the pointer to the func_t. The index is
 *               recorded in a 1-indexed format. Because the first element of
 *               `FUNCS` has been reserved, there's no need to shift it.
 * Return: The index of the pointer to the func_t.
 *
 * If the function has been inserted, the return value is the index of the
 * function in FUNCS. Otherwise, the return value is the value of the parameter
 * @funcs_index.
 */
int insert_trie(trie_t *trie, char *name, int funcs_index)
{
	char first_char = *name;
	int fc = first_char;
	int i;
	if (!fc) {
		if (!trie->index)
			trie->index = funcs_index;
		return trie->index;
	}
	if (!trie->next[fc]) {
		/* FIXME: The func_tries_idx variable may exceed the maximum number,
		 * which can lead to a segmentation fault. This issue is affected by the
		 * number of functions and the length of their names. The proper way to
		 * handle this is to dynamically allocate a new element.
		 */
		trie->next[fc] = func_tries_idx++;
		for (i = 0; i < 128; i++)
			FUNC_TRIES[trie->next[fc]].next[i] = 0;
		FUNC_TRIES[trie->next[fc]].index = 0;
	}
	return insert_trie(&FUNC_TRIES[trie->next[fc]], name + 1, funcs_index);
}

/**
 * find_trie() - search the index of the function name in the trie
 * @trie: A pointer to the trie where the name will be searched.
 * @name: The name to be searched.
 *
 * Return: The index of the pointer to the func_t.
 *
 * 0 - the name not found.
 * otherwise - the index of the founded index in the trie array.
 */
int find_trie(trie_t *trie, char *name)
{
	char first_char = *name;
	int fc = first_char;
	if (!fc)
		return trie->index;
	else if (!trie->next[fc])
		return 0;
	return find_trie(&FUNC_TRIES[trie->next[fc]], name + 1);
}

/* options */

int dump_ir = 0;

type_t *find_type(char *type_name)
{
	int i;
	for (i = 0; i < types_idx; i++)
		if (!strcmp(TYPES[i].type_name, type_name))
			return &TYPES[i];
	return NULL;
}

ph1_ir_t *add_global_ir(opcode_t op)
{
	ph1_ir_t *ir = &GLOBAL_IR[global_ir_idx++];
	ir->op = op;
	return ir;
}

ph1_ir_t *add_ph1_ir(opcode_t op)
{
	ph1_ir_t *ph1_ir = &PH1_IR[ph1_ir_idx++];
	ph1_ir->op = op;
	return ph1_ir;
}

ph2_ir_t *add_ph2_ir(opcode_t op)
{
	ph2_ir_t *ph2_ir = &PH2_IR[ph2_ir_idx++];
	ph2_ir->op = op;
	return ph2_ir;
}

void set_var_liveout(var_t *var, int end)
{
	if (var->liveness >= end)
		return;
	var->liveness = end;
}

void add_label(char *name, int offset)
{
	label_lut_t *lut = &LABEL_LUT[label_lut_idx++];
	strcpy(lut->name, name);
	lut->offset = offset;
}

int find_label_offset(char name[])
{
	int i;
	for (i = 0; i < label_lut_idx; i++)
		if (!strcmp(LABEL_LUT[i].name, name))
			return LABEL_LUT[i].offset;
	return -1;
}

block_t *add_block(block_t *parent, func_t *func, macro_t *macro)
{
	block_t *blk = &BLOCKS[blocks_idx];
	blk->index = blocks_idx++;
	blk->parent = parent;
	blk->func = func;
	blk->macro = macro;
	blk->next_local = 0;
	return blk;
}

void add_alias(char *alias, char *value)
{
	alias_t *al = &ALIASES[aliases_idx++];
	strcpy(al->alias, alias);
	strcpy(al->value, value);
	al->disabled = 0;
}

char *find_alias(char alias[])
{
	int i;
	for (i = 0; i < aliases_idx; i++)
		if (!ALIASES[i].disabled && !strcmp(alias, ALIASES[i].alias))
			return ALIASES[i].value;
	return NULL;
}

int remove_alias(char *alias)
{
	int i;
	for (i = 0; i < aliases_idx; i++) {
		if (!ALIASES[i].disabled && !strcmp(alias, ALIASES[i].alias)) {
			ALIASES[i].disabled = 1;
			return 1;
		}
	}
	return 0;
}

macro_t *add_macro(char *name)
{
	macro_t *ma = &MACROS[macros_idx++];
	strcpy(ma->name, name);
	ma->disabled = 0;
	return ma;
}

macro_t *find_macro(char *name)
{
	int i;
	for (i = 0; i < macros_idx; i++)
		if (!MACROS[i].disabled && !strcmp(name, MACROS[i].name))
			return &MACROS[i];
	return NULL;
}

int remove_macro(char *name)
{
	int i;
	for (i = 0; i < macros_idx; i++) {
		if (!MACROS[i].disabled && !strcmp(name, MACROS[i].name)) {
			MACROS[i].disabled = 1;
			return 1;
		}
	}
	return 0;
}

void error(char *msg);
int find_macro_param_src_idx(char *name, block_t *parent)
{
	int i;
	macro_t *macro = parent->macro;

	if (!parent)
		error("The macro expansion is not supported in the global scope");
	if (!parent->macro)
		return 0;

	for (i = 0; i < macro->num_param_defs; i++)
		if (!strcmp(macro->param_defs[i].var_name, name))
			return macro->params[i];
	return 0;
}

func_t *add_func(char *name)
{
	func_t *fn;
	int index = insert_trie(FUNC_TRIES, name, funcs_idx);
	if (index == funcs_idx) {
		fn = &FUNCS[funcs_idx++];
		strcpy(fn->return_def.var_name, name);
	}
	fn = &FUNCS[index];
	fn->stack_size = 4; /*starting point of stack */
	return fn;
}

type_t *add_type()
{
	return &TYPES[types_idx++];
}

type_t *add_named_type(char *name)
{
	type_t *type = add_type();
	strcpy(type->type_name, name);
	return type;
}

void add_constant(char alias[], int value)
{
	constant_t *constant = &CONSTANTS[constants_idx++];
	strcpy(constant->alias, alias);
	constant->value = value;
}

constant_t *find_constant(char alias[])
{
	int i;
	for (i = 0; i < constants_idx; i++)
		if (!strcmp(CONSTANTS[i].alias, alias))
			return &CONSTANTS[i];
	return NULL;
}

func_t *find_func(char func_name[])
{
	int index = find_trie(FUNC_TRIES, func_name);
	if (index)
		return &FUNCS[index];
	return NULL;
}

var_t *find_member(char token[], type_t *type)
{
	int i;
	for (i = 0; i < type->num_fields; i++)
		if (!strcmp(type->fields[i].var_name, token))
			return &type->fields[i];
	return NULL;
}

var_t *find_local_var(char *token, block_t *block)
{
	int i;
	func_t *fn = block->func;

	for (; block; block = block->parent) {
		for (i = 0; i < block->next_local; i++)
			if (!strcmp(block->locals[i].var_name, token))
				return &block->locals[i];
	}

	if (fn) {
		for (i = 0; i < fn->num_params; i++)
			if (!strcmp(fn->param_defs[i].var_name, token))
				return &fn->param_defs[i];
	}
	return NULL;
}

var_t *find_global_var(char *token)
{
	int i;
	block_t *block = &BLOCKS[0];

	for (i = 0; i < block->next_local; i++)
		if (!strcmp(block->locals[i].var_name, token))
			return &block->locals[i];
	return NULL;
}

var_t *find_var(char *token, block_t *parent)
{
	var_t *var = find_local_var(token, parent);
	if (!var)
		var = find_global_var(token);
	return var;
}

int size_var(var_t *var)
{
	int s = 0;

	if (var->is_ptr > 0 || var->is_func > 0) {
		s += 4;
	} else {
		type_t *td = find_type(var->type_name);
		int bs = td->size;
		if (var->array_size > 0) {
			int j = 0;
			for (; j < var->array_size; j++)
				s += bs;
		} else
			s += bs;
	}
	return s;
}

/* This routine is required because the global variable initializations are
 * not supported now.
 */
void global_init()
{
	elf_code_start = ELF_START + elf_header_len;

	BLOCKS = malloc(MAX_BLOCKS * sizeof(block_t));
	MACROS = malloc(MAX_ALIASES * sizeof(macro_t));
	FUNCS = malloc(MAX_FUNCS * sizeof(func_t));
	FUNC_TRIES = malloc(MAX_FUNC_TRIES * sizeof(trie_t));
	TYPES = malloc(MAX_TYPES * sizeof(type_t));
	GLOBAL_IR = malloc(MAX_GLOBAL_IR * sizeof(ph1_ir_t));
	PH1_IR = malloc(MAX_IR_INSTR * sizeof(ph1_ir_t));
	PH2_IR = malloc(MAX_IR_INSTR * sizeof(ph2_ir_t));
	LABEL_LUT = malloc(MAX_LABEL * sizeof(label_lut_t));
	SOURCE = malloc(MAX_SOURCE);
	ALIASES = malloc(MAX_ALIASES * sizeof(alias_t));
	CONSTANTS = malloc(MAX_CONSTANTS * sizeof(constant_t));
	LINES = malloc(MAX_LINES * sizeof(int));
	INCLUDE_INFOS = malloc(MAX_INCLUDE_INFOS * sizeof(include_info_t));

	elf_code = malloc(MAX_CODE);
	elf_data = malloc(MAX_DATA);
	elf_header = malloc(MAX_HEADER);
	elf_symtab = malloc(MAX_SYMTAB);
	elf_strtab = malloc(MAX_STRTAB);
	elf_section = malloc(MAX_SECTION);

	/* set starting point of global stack manually */
	FUNCS[0].stack_size = 4;
}

void global_release()
{
	free(BLOCKS);
	free(MACROS);
	free(FUNCS);
	free(FUNC_TRIES);
	free(TYPES);
	free(GLOBAL_IR);
	free(PH1_IR);
	free(PH2_IR);
	free(LABEL_LUT);
	free(SOURCE);
	free(ALIASES);
	free(CONSTANTS);
	free(LINES);
	free(INCLUDE_INFOS);

	free(elf_code);
	free(elf_data);
	free(elf_header);
	free(elf_symtab);
	free(elf_strtab);
	free(elf_section);
}

void print_indent(int indent)
{
	int i;
	for (i = 0; i < indent; i++)
		printf("\t");
}

void dump_ph1_ir()
{
	int indent = 0;
	ph1_ir_t *ph1_ir;
	func_t *fn;
	int i, j, k;
	char rd[MAX_VAR_LEN], op1[MAX_VAR_LEN], op2[MAX_VAR_LEN];

	for (i = 0; i < ph1_ir_idx; i++) {
		ph1_ir = &PH1_IR[i];

		if (ph1_ir->dest)
			strcpy(rd, ph1_ir->dest->var_name);
		if (ph1_ir->src0)
			strcpy(op1, ph1_ir->src0->var_name);
		if (ph1_ir->src1)
			strcpy(op2, ph1_ir->src1->var_name);

		switch (ph1_ir->op) {
		case OP_define:
			fn = find_func(ph1_ir->func_name);
			printf("def %s", fn->return_def.type_name);

			for (j = 0; j < fn->return_def.is_ptr; j++)
				printf("*");
			printf(" @%s(", ph1_ir->func_name);

			for (j = 0; j < fn->num_params; j++) {
				if (j != 0)
					printf(", ");
				printf("%s", fn->param_defs[j].type_name);

				for (k = 0; k < fn->param_defs[j].is_ptr; k++)
					printf("*");
				printf(" %%%s", fn->param_defs[j].var_name);
			}
			printf(")");
			break;
		case OP_block_start:
			print_indent(indent);
			printf("{");
			indent++;
			break;
		case OP_block_end:
			indent--;
			print_indent(indent);
			printf("}");
			break;
		case OP_allocat:
			print_indent(indent);
			printf("allocat %s", ph1_ir->src0->type_name);
			for (j = 0; j < ph1_ir->src0->is_ptr; j++)
				printf("*");
			printf(" %%%s", op1);

			if (ph1_ir->src0->array_size > 0)
				printf("[%d]", ph1_ir->src0->array_size);
			break;
		case OP_load_constant:
			print_indent(indent);
			printf("const %%%s, $%d", rd, ph1_ir->dest->init_val);
			break;
		case OP_load_data_address:
			print_indent(indent);
			/* offset from .data section */
			printf("%%%s = .data (%d)", rd, ph1_ir->dest->init_val);
			break;
		case OP_address_of:
			print_indent(indent);
			printf("%%%s = &(%%%s)", rd, op1);
			break;
		case OP_assign:
			print_indent(indent);
			printf("%%%s = %%%s", rd, op1);
			break;
		case OP_label:
			print_indent(0);
			printf("%s", op1);
			break;
		case OP_branch:
			print_indent(indent);
			printf("br %%%s, %s, %s", rd, op1, op2);
			break;
		case OP_jump:
			print_indent(indent);
			printf("j %s", rd);
			break;
		case OP_push:
			print_indent(indent);
			printf("push %%%s", op1);
			break;
		case OP_call:
			print_indent(indent);
			printf("call @%s, %d", ph1_ir->func_name, ph1_ir->param_num);
			break;
		case OP_func_ret:
			print_indent(indent);
			printf("retval %%%s", rd);
			break;
		case OP_return:
			print_indent(indent);
			if (ph1_ir->src0)
				printf("ret %%%s", op1);
			else
				printf("ret");
			break;
		case OP_read:
			print_indent(indent);
			printf("%%%s = (%%%s), %d", rd, op1, ph1_ir->size);
			break;
		case OP_write:
			print_indent(indent);
			if (ph1_ir->src0->is_func)
				printf("(%%%s) = @%s", rd, op1);
			else
				printf("(%%%s) = %%%s, %d", rd, op1, ph1_ir->size);
			break;
		case OP_indirect:
			print_indent(indent);
			printf("indirect call @(%%%s)", op1);
			break;
		case OP_negate:
			print_indent(indent);
			printf("neg %%%s, %%%s", rd, op1);
			break;
		case OP_add:
			print_indent(indent);
			printf("%%%s = add %%%s, %%%s", rd, op1, op2);
			break;
		case OP_sub:
			print_indent(indent);
			printf("%%%s = sub %%%s, %%%s", rd, op1, op2);
			break;
		case OP_mul:
			print_indent(indent);
			printf("%%%s = mul %%%s, %%%s", rd, op1, op2);
			break;
		case OP_div:
			print_indent(indent);
			printf("%%%s = div %%%s, %%%s", rd, op1, op2);
			break;
		case OP_mod:
			print_indent(indent);
			printf("%%%s = mod %%%s, %%%s", rd, op1, op2);
			break;
		case OP_eq:
			print_indent(indent);
			printf("%%%s = eq %%%s, %%%s", rd, op1, op2);
			break;
		case OP_neq:
			print_indent(indent);
			printf("%%%s = neq %%%s, %%%s", rd, op1, op2);
			break;
		case OP_gt:
			print_indent(indent);
			printf("%%%s = gt %%%s, %%%s", rd, op1, op2);
			break;
		case OP_lt:
			print_indent(indent);
			printf("%%%s = lt %%%s, %%%s", rd, op1, op2);
			break;
		case OP_geq:
			print_indent(indent);
			printf("%%%s = geq %%%s, %%%s", rd, op1, op2);
			break;
		case OP_leq:
			print_indent(indent);
			printf("%%%s = leq %%%s, %%%s", rd, op1, op2);
			break;
		case OP_bit_and:
			print_indent(indent);
			printf("%%%s = and %%%s, %%%s", rd, op1, op2);
			break;
		case OP_bit_or:
			print_indent(indent);
			printf("%%%s = or %%%s, %%%s", rd, op1, op2);
			break;
		case OP_bit_not:
			print_indent(indent);
			printf("%%%s = not %%%s", rd, op1);
			break;
		case OP_bit_xor:
			print_indent(indent);
			printf("%%%s = xor %%%s, %%%s", rd, op1, op2);
			break;
		case OP_log_and:
			print_indent(indent);
			printf("%%%s = and %%%s, %%%s", rd, op1, op2);
			break;
		case OP_log_or:
			print_indent(indent);
			printf("%%%s = or %%%s, %%%s", rd, op1, op2);
			break;
		case OP_log_not:
			print_indent(indent);
			printf("%%%s = not %%%s", rd, op1);
			break;
		case OP_rshift:
			print_indent(indent);
			printf("%%%s = rshift %%%s, %%%s", rd, op1, op2);
			break;
		case OP_lshift:
			print_indent(indent);
			printf("%%%s = lshift %%%s, %%%s", rd, op1, op2);
			break;
		default:
			break;
		}
		printf("\n");
	}
	printf("===\n");
}

// TODO: Sort functions below to above

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

void error(char *msg)
{
	include_info_t *info = find_include_info(source_idx);
	int file_line_idx = find_line_idx(info->start_idx);
	int line_idx = find_line_idx(source_idx);
	int actual_line = line_idx - file_line_idx;
	int line = LINES[line_idx - 1];
	int offset = source_idx - line + 1;

	printf("[%s: Ln %d, Col %d] Error: %s\n", info->include_file_path, actual_line, offset, msg);
	abort();
}

#endif
