/* C Language Lexical Analyzer */

#ifndef LEXER_C
#define LEXER_C
#include "dbg.c"

char peek_char(int offset);

int is_whitespace(char c)
{
    return (c == ' ' || c == '\t');
}

/* is it backslash-newline? */
int is_linebreak(char c)
{
    return c == '\\' && peek_char(1) == '\n';
}

int is_newline(char c)
{
    return (c == '\r' || c == '\n');
}

/* is it alphabet, number or '_'? */
int is_alnum(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_'));
}

int is_ident_start(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'));
}

int is_ident(char c)
{
    return (is_ident_start(c) || (c >= '0' && c <= '9'));
}

int is_digit(char c)
{
    return (c >= '0' && c <= '9') ? 1 : 0;
}

int is_hex(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == 'x' || (c >= 'A' && c <= 'F'));
}

// int is_numeric(char buffer[])
// {
//     int i, hex, size = strlen(buffer);

//     if (size > 2)
//         hex = (buffer[0] == '0' && buffer[1] == 'x') ? 1 : 0;
//     else
//         hex = 0;

//     for (i = 0; i < size; i++) {
//         if (hex && (is_hex(buffer[i]) == 0))
//             return 0;
//         if (!hex && (is_digit(buffer[i]) == 0))
//             return 0;
//     }
//     return 1;
// }

/* lexer tokens */
typedef enum {
    T_numeric,
    T_identifier,
    T_linebreak, /* used for preprocessor directive parsing */
    T_comma,     /* , */
    T_string,    /* null-terminated string */
    T_char,
    T_open_bracket,  /* ( */
    T_close_bracket, /* ) */
    T_open_curly,    /* { */
    T_close_curly,   /* } */
    T_open_square,   /* [ */
    T_close_square,  /* ] */
    T_asterisk,      /* '*' */
    T_divide,        /* / */
    T_mod,           /* % */
    T_bit_or,        /* | */
    T_bit_xor,       /* ^ */
    T_bit_not,       /* ~ */
    T_log_and,       /* && */
    T_log_or,        /* || */
    T_log_not,       /* ! */
    T_lt,            /* < */
    T_gt,            /* > */
    T_le,            /* <= */
    T_ge,            /* >= */
    T_lshift,        /* << */
    T_rshift,        /* >> */
    T_dot,           /* . */
    T_arrow,         /* -> */
    T_plus,          /* + */
    T_minus,         /* - */
    T_minuseq,       /* -= */
    T_pluseq,        /* += */
    T_oreq,          /* |= */
    T_andeq,         /* &= */
    T_eq,            /* == */
    T_noteq,         /* != */
    T_assign,        /* = */
    T_increment,     /* ++ */
    T_decrement,     /* -- */
    T_question,      /* ? */
    T_colon,         /* : */
    T_semicolon,     /* ; */
    T_eof,           /* end-of-file (EOF) */
    T_ampersand,     /* & */
    T_return,
    T_if,
    T_else,
    T_while,
    T_for,
    T_do,
    T_typedef,
    T_enum,
    T_struct,
    T_sizeof,
    T_elipsis, /* ... */
    T_switch,
    T_case,
    T_break,
    T_default,
    T_continue,
    /* Preprocessor phase usage only */
    T_preproc_define,
    T_preproc_path, /* Used for include path parsing */
    T_preproc_undef,
    T_preproc_error,
    T_preproc_if,
    T_preproc_elif,
    T_preproc_ifdef,
    T_preproc_else,
    T_preproc_endif,
    T_preproc_include,
} token_kind_t;

typedef struct {
    token_kind_t kind;
    int start_pos;
    int length;
} token_t;

void token_str(token_t *token, char *str)
{
    if (token->kind == T_string) {
        /* Discards "" and transform escaped character */
        int string_idx, output_idx = 0;

        for (string_idx = 1; string_idx < token->length - 1;) {
            char cur_char = SOURCE[token->start_pos + string_idx];

            if (cur_char == '\\') {
                string_idx++;
                cur_char = SOURCE[token->start_pos + string_idx];

                if (cur_char == 'n') {
                    str[output_idx++] = '\n';
                    continue;
                }
                if (cur_char == '"') {
                    str[output_idx++] = '"';
                    continue;
                }
                if (cur_char == 'r') {
                    str[output_idx++] = '\r';
                    continue;
                }
                if (cur_char == '\'') {
                    str[output_idx++] = '\'';
                    continue;
                }
                if (cur_char == 't') {
                    str[output_idx++] = '\t';
                    continue;
                }
                if (cur_char == '\\') {
                    str[output_idx++] = '\\';
                    continue;
                }

                error("Internal compiler error: esacped character should be checked in lexer");
            }

            str[output_idx] = cur_char;
            string_idx++;
            output_idx++;
        }

        str[output_idx] = 0;
        return;
    }
    if (token->kind == T_char) {
        /* Discards '' and transform escaped character */
        char cur_char = SOURCE[token->start_pos + 1];

        if (cur_char == '\\') {
            cur_char = SOURCE[token->start_pos + 2];

            if (cur_char == 'n') {
                str[0] = '\n';
            } else if (cur_char == '"') {
                str[0] = '"';
            } else if (cur_char == 'r') {
                str[0] = '\r';
            } else if (cur_char == '\'') {
                str[0] = '\'';
            } else if (cur_char == 't') {
                str[0] = '\t';
            } else if (cur_char == '\\') {
                str[0] = '\\';
            } else {
                error("Internal compiler error: esacped character should be checked in lexer");
            }
        }

        str[0] = cur_char;
        str[1] = 0;
        return;
    }
    strncpy(str, SOURCE + token->start_pos, token->length);
    str[token->length] = 0;
}

token_t cur_token;
/*
 * Used after a token is peeked and going to be consumed to prevent second
 * tokenize process.
 */
int peek_offset = 0;
int skip_newline = 1;
int preproc_path_parsing = 0;
int preproc_match;
/*
 * Allows replacing identifiers with alias value if alias exists. This is
 * disabled in certain cases, e.g. #undef.
 */
int preproc_aliasing = 1;

void skip_whitespace()
{
    char next_char;

    while ((next_char = peek_char(0))) {
        // TODO: Is it made for preproc lexing?
        if (is_linebreak(next_char)) {
            source_idx += 2;
            continue;
        }
        if (is_whitespace(next_char)) {
            ++source_idx;
            continue;
        }
        if (skip_newline && is_newline(next_char)) {
            ++source_idx;
            continue;
        }
        break;
    }
}

char peek_char(int offset)
{
    if (source_idx + offset >= MAX_SOURCE)
        return 0;

    return SOURCE[source_idx + offset];
}

void advance(int offset)
{
    source_idx += offset;
}

token_t *next_token();
void skip_macro_body()
{
    while (!is_newline(peek_char(0))) {
        next_token();
    }

    skip_newline = 1;
    skip_whitespace();
}

/*
 * Updates cur_token and source_idx based on given length
 */
token_t *update_token(token_kind_t kind, int length)
{
    cur_token.kind = kind;
    cur_token.start_pos = source_idx;
    cur_token.length = length;
    source_idx += length;
    return &cur_token;
}

token_t *next_token()
{
    /* fast path for peek and consume strategy */
    if (peek_offset) {
        source_idx += peek_offset;
        peek_offset = 0;
        return &cur_token;
    }

    skip_whitespace();
    char next_char = peek_char(0);

    if (preproc_path_parsing) {
        char path[MAX_TOKEN_LEN];
        int path_len = 1;

        if (next_char != '<') {
            error("Invalid include path syntax");
        }

        while (peek_char(path_len++) != '>')
            ;

        return update_token(T_preproc_path, path_len);
    }

    if (next_char == '#') {
        char token[MAX_TOKEN_LEN];
        int preproc_len = 1;

        while (is_alnum(peek_char(preproc_len))) {
            ++preproc_len;
        }
        strncpy(token, SOURCE + source_idx, preproc_len);
        token[preproc_len] = 0;

        if (!strcmp(token, "#include")) {
            return update_token(T_preproc_include, preproc_len);
        }
        if (!strcmp(token, "#define")) {
            return update_token(T_preproc_define, preproc_len);
        }
        if (!strcmp(token, "#undef")) {
            return update_token(T_preproc_undef, preproc_len);
        }
        if (!strcmp(token, "#error")) {
            return update_token(T_preproc_error, preproc_len);
        }
        if (!strcmp(token, "#if")) {
            return update_token(T_preproc_if, preproc_len);
        }
        if (!strcmp(token, "#elif")) {
            return update_token(T_preproc_elif, preproc_len);
        }
        if (!strcmp(token, "#else")) {
            return update_token(T_preproc_else, preproc_len);
        }
        if (!strcmp(token, "#ifdef")) {
            return update_token(T_preproc_ifdef, preproc_len);
        }
        if (!strcmp(token, "#endif")) {
            return update_token(T_preproc_endif, preproc_len);
        }

        error("Unknown directive");
    }

    /* C-style comments */
    if (next_char == '/') {
        next_char = peek_char(1);

        if (next_char == '*') {
            /* in a comment, skip until end */
            int offset = 2;

            while ((next_char = peek_char(offset++))) {
                if (next_char == '*') {
                    next_char = peek_char(offset);

                    if (next_char == '/') {
                        source_idx += offset + 1;
                        return next_token();
                    }
                }
            }
        } else if (next_char == '/') {
            int offset = 2;

            while (!is_newline(peek_char(offset))) {
                offset++;
            }
            source_idx += offset;
            return next_token();
        } else {
            return update_token(T_divide, 1);
        }

        /* TODO: check invalid cases */
        error("Unexpected '/'");
    }
    if (next_char == '(') {
        return update_token(T_open_bracket, 1);
    }
    if (next_char == ')') {
        return update_token(T_close_bracket, 1);
    }
    if (next_char == '{') {
        return update_token(T_open_curly, 1);
    }
    if (next_char == '}') {
        return update_token(T_close_curly, 1);
    }
    if (next_char == '[') {
        return update_token(T_open_square, 1);
    }
    if (next_char == ']') {
        return update_token(T_close_square, 1);
    }
    if (next_char == ',') {
        return update_token(T_comma, 1);
    }
    if (next_char == '^') {
        return update_token(T_bit_xor, 1);
    }
    if (next_char == '~') {
        return update_token(T_bit_not, 1);
    }
    if (next_char == '"') {
        char cur_char;
        int length = 2;

        while ((cur_char = peek_char(length)) != '"' && source_idx + length < MAX_SOURCE) {
            if (cur_char == '\\') {
                cur_char = peek_char(++length);

                if (!(cur_char == 'n' || cur_char == '"' || cur_char == 'r' || cur_char == '\'' || cur_char == 't' ||
                      cur_char == '\\')) {
                    /* Offsets to the escaped character */
                    source_idx += length - 1;

                    error("Unknown escape character");
                }
            }

            ++length;
        }

        // Checks right double quotation mark
        if (peek_char(length++) != '"')
            abort();

        return update_token(T_string, length);
    }
    if (next_char == '\'') {
        char cur_char;
        int length = 2;

        cur_char = peek_char(length);

        if (cur_char == '\\') {
            cur_char = peek_char(++length);

            if (!(cur_char == 'n' || cur_char == 'r' || cur_char == '\'' || cur_char == '"' || cur_char == 't' ||
                  cur_char == '\\')) {
                abort();
            }
        }

        // Checks right quotation mark
        if (peek_char(++length) != '\'') {
            abort();
        }

        return update_token(T_char, length);
    }
    if (next_char == '*') {
        return update_token(T_asterisk, 1);
    }
    if (next_char == '&') {
        next_char = peek_char(1);

        if (next_char == '&') {
            return update_token(T_log_and, 2);
        }
        if (next_char == '=') {
            return update_token(T_andeq, 2);
        }

        return update_token(T_ampersand, 1);
    }
    if (next_char == '|') {
        next_char = peek_char(1);

        if (next_char == '|') {
            return update_token(T_log_or, 2);
        }
        if (next_char == '=') {
            return update_token(T_oreq, 2);
        }

        return update_token(T_bit_or, 1);
    }
    if (next_char == '<') {
        next_char = peek_char(1);

        if (next_char == '=') {
            return update_token(T_le, 2);
        }
        if (next_char == '<') {
            return update_token(T_lshift, 2);
        }

        return update_token(T_lt, 1);
    }
    if (next_char == '>') {
        next_char = peek_char(1);

        if (next_char == '=') {
            return update_token(T_ge, 2);
        }
        if (next_char == '>') {
            return update_token(T_rshift, 2);
        }

        return update_token(T_gt, 1);
    }
    if (next_char == '!') {
        next_char = peek_char(1);

        if (next_char == '=') {
            return update_token(T_noteq, 2);
        }

        return update_token(T_log_not, 1);
    }
    if (next_char == '.') {
        next_char = peek_char(1);

        if (next_char == '.') {
            next_char = peek_char(2);

            if (next_char == '.') {
                return update_token(T_elipsis, 3);
            }

            abort();
        }

        return update_token(T_dot, 1);
    }
    if (next_char == '-') {
        next_char = peek_char(1);

        if (next_char == '>') {
            return update_token(T_arrow, 2);
        }

        if (next_char == '-') {
            return update_token(T_decrement, 2);
        }

        if (next_char == '=') {
            return update_token(T_minuseq, 2);
        }

        return update_token(T_minus, 1);
    }
    if (next_char == '+') {
        next_char = peek_char(1);

        if (next_char == '+') {
            return update_token(T_increment, 2);
        }
        if (next_char == '=') {
            return update_token(T_pluseq, 2);
        }

        return update_token(T_plus, 1);
    }
    if (next_char == ';') {
        return update_token(T_semicolon, 1);
    }
    if (next_char == '?') {
        return update_token(T_question, 1);
    }
    if (next_char == ':') {
        return update_token(T_colon, 1);
    }
    if (next_char == '=') {
        next_char = peek_char(1);

        if (next_char == '=') {
            return update_token(T_eq, 2);
        }

        return update_token(T_assign, 1);
    }

    if (is_digit(next_char)) {
        int length = 1;

        while (is_hex(peek_char(length))) {
            ++length;
        }

        return update_token(T_numeric, length);
    }

    if (is_ident_start(next_char)) {
        char ident[MAX_ID_LEN];
        int length = 1;

        while (is_ident(peek_char(length))) {
            ++length;
        }

        strncpy(ident, SOURCE + source_idx, length);
        token_kind_t kind = T_identifier;

        if (!strcmp(ident, "if"))
            kind = T_if;
        if (!strcmp(ident, "while"))
            kind = T_while;
        if (!strcmp(ident, "for"))
            kind = T_for;
        if (!strcmp(ident, "do"))
            kind = T_do;
        if (!strcmp(ident, "else"))
            kind = T_else;
        if (!strcmp(ident, "return"))
            kind = T_return;
        if (!strcmp(ident, "typedef"))
            kind = T_typedef;
        if (!strcmp(ident, "enum"))
            kind = T_enum;
        if (!strcmp(ident, "struct"))
            kind = T_struct;
        if (!strcmp(ident, "sizeof"))
            kind = T_sizeof;
        if (!strcmp(ident, "switch"))
            kind = T_switch;
        if (!strcmp(ident, "case"))
            kind = T_case;
        if (!strcmp(ident, "break"))
            kind = T_break;
        if (!strcmp(ident, "default"))
            kind = T_default;
        if (!strcmp(ident, "continue"))
            kind = T_continue;

        return update_token(kind, length);
    }

    if (next_char == 0) {
        return update_token(T_eof, 0);
    }

    return 0;
}

int peek_token(token_kind_t kind)
{
    int original_idx = source_idx;
    token_kind_t token_kind = next_token()->kind;
    peek_offset = source_idx - original_idx;
    source_idx = original_idx;
    return kind == token_kind;
}

void expect_token(token_kind_t kind)
{
    int original_idx = source_idx;
    token_kind_t token_kind = next_token()->kind;

    if (kind != token_kind) {
        source_idx = original_idx;
        error("token kind mismatch");
    }
}

int accept_token(token_kind_t kind)
{
    if (peek_token(kind)) {
        next_token();
        return 1;
    }

    return 0;
}

#endif
