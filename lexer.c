/* C Language Lexical Analyzer */

int is_whitespace(char c)
{
    return (c == ' ' || c == '\t');
}

char peek_char(int offset);

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
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || (c == '_'));
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
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == 'x' ||
            (c >= 'A' && c <= 'F'));
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
    T_comma,  /* , */
    T_string, /* null-terminated string */
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
    T_define,
    T_undef,
    T_error,
    T_include,
    T_typedef,
    T_enum,
    T_struct,
    T_sizeof,
    T_elipsis, /* ... */
    T_switch,
    T_case,
    T_break,
    T_default,
    T_continue
} token_kind_t;

const char* token_kind_literals[] = {
    "T_numeric",
    "T_identifier",
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
    "T_define",
    "T_undef",
    "T_error",
    "T_include",
    "T_typedef",
    "T_enum",
    "T_struct",
    "T_sizeof",
    "T_elipsis", /* ... */
    "T_switch",
    "T_case",
    "T_break",
    "T_default",
    "T_continue"
};

typedef struct {
    token_kind_t kind;
    int line;
    int start_pos;
    int length;
} token_t;

void token_str(token_t *token, char *str)
{
    strncpy(str, SOURCE + token->start_pos, token->length);
    str[token->length] = 0;
}

token_t cur_token;
int cur_line = 1;
int skip_newline = 1;

void skip_whitespace()
{
    char next_char;

    while ((next_char = peek_char(0))) {
        if (is_linebreak(next_char))
        {
            source_idx += 2;
            continue;
        }
        if (is_whitespace(next_char)) {
            ++source_idx;
            continue;
        }
        if (skip_newline && is_newline(next_char)) {
            ++source_idx;
            ++cur_line;
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

/*
 * Updates cur_token and source_idx based on given length
 */
void update_token(token_kind_t kind, int length)
{
    cur_token.kind = kind;
    cur_token.line = cur_line;
    cur_token.start_pos = source_idx;
    cur_token.length = length;
    source_idx += length;
}

void next_token()
{
    skip_whitespace();
    char next_char = peek_char(0);

    if (next_char == '(') {
        update_token(T_open_bracket, 1);
        return;
    }
    if (next_char == ')') {
        update_token(T_close_bracket, 1);
        return;
    }
    if (next_char == '{') {
        update_token(T_open_curly, 1);
        return;
    }
    if (next_char == '}') {
        update_token(T_close_curly, 1);
        return;
    }
    if (next_char == '[') {
        update_token(T_open_square, 1);
        return;
    }
    if (next_char == ']') {
        update_token(T_close_square, 1);
        return;
    }
    if (next_char == ',') {
        update_token(T_comma, 1);
        return;
    }
    if (next_char == '^') {
        update_token(T_bit_xor, 1);
        return;
    }
    if (next_char == '~') {
        update_token(T_bit_not, 1);
        return;
    }
    if (next_char == '"') {
        char cur_char;
        int length = 2;

        while ((cur_char = peek_char(length)) != '"' && source_idx + length < MAX_SOURCE) {
            if (cur_char == '\\') {
                cur_char = peek_char(++length);

                if (!(cur_char == 'n' || cur_char == '"' || 
                    cur_char == 'r' || cur_char == '\'' ||
                    cur_char == 't' || cur_char == '\\')){
                    abort();
                }
            }

            ++length;
        }

        // Checks right double quotation mark
        if (peek_char(++length) != '"')
            abort();

        update_token(T_string, length);
        return;
    }
    if (next_char == '\'') {
        char cur_char;
        int length = 2;

        cur_char = peek_char(length);

        if (cur_char == '\\') {
            cur_char = peek_char(++length);

            if (!(cur_char == 'n' || cur_char == 'r' ||
                cur_char == '\'' || cur_char == '"' ||
                cur_char == 't' || cur_char == '\\')) {
                abort();
            }
        }

        // Checks right quotation mark
        if (peek_char(++length) != '\'')
            abort();

        update_token(T_char, length);
        return;
    }
    if (next_char == ';') {
        update_token(T_semicolon, 1);
        return;
    }

    if (is_digit(next_char)) {
        int length = 1;

        while (is_hex(peek_char(length + 1)))
            ++length;

        update_token(T_numeric, length);
    }

    if (is_ident_start(next_char)) {
        char ident[MAX_ID_LEN];
        int length = 1;

        while (is_ident(peek_char(length + 1)))
            ++length;
        ++length;

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
        
        update_token(kind, length);
        return;
    }

    if (next_char == 0) {
        update_token(T_eof, 1);
        return;
    }
}

