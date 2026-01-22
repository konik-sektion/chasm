#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static bool is_ident_start(int c) {
    return isalpha(c) || c == '_';
}
static bool is_ident(int c) {
    return isalnum(c) || c == '_';
}
static bool is_path_char(int c) {
    return isalnum(c) || c == '_' || c == '/' || c == '.' || c == '-';
}
static bool is_percent_ident_char(int c) {
    return isalnum(c) || c == '_' || c == '%';
}

static Token make_token(TokenKind k, const char* s, const char* e, int line, int col) {
    Token t;
    t.kind = k;
    t.start = s;
    t.end = e;
    t.line = line;
    t.col = col;
    return t;
}

void lexer_init(Lexer* L, const char* src, size_t len) {
    L->src = src;
    L->len = len;
    L->i   = 0;
    L->line = 1;
    L-> col = 1;
    L->indent_top = 0;
    L->indent_stack[0] = 0;
    L->at_line_start = true;
    L->pending_dedents = 0;
}

static void skip_ws_inline(Lexer* L) {
    while (L->i < L->len) {
        char c = L->src[L->i];
        if (c == ' ' || c == '\t' || c == '\r') {
            L->i++;
            L->col++;
            continue;
        }
        break;
    }
}

static void skip_comment(Lexer* L) {
    if (L->i + 2 < L->len && L->src[L->i] == ';' && L->src[L->i+1] == ';' && L->src[L->i+2] == ';') {
        while (L->i < L->len && L->src[L->i] != '\n') {
            L->i++;
            L->col++;
        }
    }
}

Token next_token(Lexer* L) {
    if (L->pending_dedents > 0) {
        L->pending_dedents--;
        return make_token(TK_DEDENT, L->src + L->i, L->src + L->i, L->line, L->col);
    }

    if (L->i >= L->len) {
        if (L->indent_top > 0) {
            L->indent_top--;
            return make_token(TK_DEDENT, L->src + L->i, L->src + L->i, L->line, L->col);
        }
        return make_token(TK_EOF, L->src + L->i, L->src + L->i, L->line, L->col);
    }

    if (L->at_line_start) {
        int line = L->line, col = L->col;
        int indent = 0;
        size_t j = L->i;

        while (j < L->len) {
            char c = L->src[j];
            if (c == ' ') {
                indent++;
                j++;
                continue;
            }
            if (c == '\t') {
                indent += 4;
                j++;
                continue;
            }
            break;
        }

        if (!(j < L->len && (L->src[j] == '\n'))) {
            int cur = L->indent_stack[L->indent_top];
            if (indent > cur) {
                L->indent_top++;
                L->indent_stack[L->indent_top] = indent;
                L->col += (int)(j - L->i);
                L->i = j;
                L->at_line_start = false;
                return make_token(TK_INDENT, L->src + L->i, L->src + L->i, line, col);
            } else if (indent < cur) {
                int pops = 0;
                while (L->indent_top > 0 && indent < L->indent_stack[L->indent_top]) {
                    L->indent_top--;
                    pops++;
                }
                if (indent != L->indent_stack[L->indent_top]) {
                    die("indentation error");
                }
                L->col += (int)(j - L->i);
                L->i = j;
                L->at_line_start = false;
                L->pending_dedents = pops - 1;
                return make_token(TK_DEDENT, L->src + L->i, L->src + L->i, line, col);
            }
            L->col += (int)(j - L->i);
            L->i = j;
        }
        L->at_line_start = false;
    }

    skip_ws_inline(L);
    skip_comment(L);

    if (L->i < L->len && L->src[L->i] == '\n') {
        const char* s = L->src + L->i;
        L->i++;
        L->line++;
        L->col = 1;
        L->at_line_start = true;
        return make_token(TK_NL, s, s + 1, L->line - 1, 1);
    }

    if (L->i >= L->len) return next_token(L);

    int line = L->line, col = L->col;
    const char* s = L->src + L->i;
    char c = L->src[L->i++];
    L->col++;

    switch (c) {
        case '#':
            return make_token(TK_HASH, s, s + 1, line, col);
        case ':':
            if (L->i < L->len && L->src[L->i] == ':') {
                L->i++;
                L->col++;
                return make_token(TK_SCOPE, s, s + 2, line, col);
            }
            return make_token(TK_COLON, s, s + 1, line, col);
        case ';':
            return make_token(TK_SEMI, s, s + 1, line, col);
        case ',':
            return make_token(TK_COMMA, s, s + 1, line, col);
        case '(':
            return make_token(TK_LPAREN, s, s + 1, line, col);
        case ')':
            return make_token(TK_RPAREN, s, s + 1, line, col);
        case '{':
            return make_token(TK_LBRACE, s, s + 1, line, col);
        case '}':
            return make_token(TK_RBRACE, s, s + 1, line, col);
        case '[':
            return make_token(TK_LBRACKET, s, s + 1, line, col);
        case ']':
            return make_token(TK_RBRACKET, s, s + 1, line, col);
        case '=':
            return make_token(TK_EQ, s, s + 1, line, col);
        case '+':
            return make_token(TK_PLUS, s, s + 1, line, col);
        case '-':
            return make_token(TK_MINUS, s, s + 1, line, col);
        case '*':
            return make_token(TK_STAR, s, s + 1, line, col);
        case '/':
            return make_token(TK_SLASH, s, s + 1, line, col);
        case '&':
            return make_token(TK_AMP, s, s + 1, line, col);
        case '$':
            return make_token(TK_DOLLAR, s, s + 1, line, col);
        case '@':
            return make_token(TK_AT, s, s + 1, line, col);
        case '%': {
            while (L->i < L->len && is_percent_ident_char((unsigned char)L->src[L->i])) {
                L->i++;
                L->col++;
            }
            return make_token(TK_PERCENT_IDENT, s, L->src + L->i, line, col);
        }
        case '>':
            if (L->i < L->len && L->src[L->i] == '>') {
                L->i++;
                L->col++;
                return make_token(TK_RARROW, s, s + 2, line, col);
            }
            break;
        case '"': {
            const char *start = L->src + L->i;
            while (L->i < L->len && L->src[L->i] != '"') {
                if (L->src[L->i] == '\n') die("unterminated string literal");
                L->i++;
                L->col++;
            }
            if (L->i >= L->len) die("unterminated string literal");
            const char *end = L->src + L->i;
            L->i++;
            L->col++;
            return make_token(TK_STRING, start, end, line, col);
        }
        case '\'': {
            const char *start = L->src + L->i;
            while (L->i < L->len && L->src[L->i] != '\'') {
                if (L->src[L->i] == '\n') die("unterminated char literal");
                L->i++;
                L->col++;
            }
            if (L->i >= L->len) die("unterminated char literal");
            const char *end = L->src + L->i;
            L->i++;
            L->col++;
            return make_token(TK_CHAR, start, end, line, col);
        }
        default:
            break;
    }

    if (c == '.' || c == '/') {
        if (c == '0' && L->i < L->len && (L->src[L->i] == 'x' || L->src[L->i] == 'X')) {
            while (L->i < L->len && is_path_char((unsigned char)L->src[L->i])) {
                L->i++;
                L->col++;
        
            }
        } else {
            while (L->i < L->len && isdigit((unsigned char)L->src[L->i])) {
                L->i++;
                L->col++;
            }
        }
        return make_token(TK_INT, s, L->src + L->i, line, col);
    }

    if (is_ident_start((unsigned char)c)) {
        bool has_path = false;
        while (L->i < L->len && is_ident((unsigned char)L->src[L->i])) {
            L->i++;
            L->col++;
        }
        while (L->i < L->len && is_path_char((unsigned char)L->src[L->i])) {
            has_path = true;
            L->i++;
            L->col++;
        }
        return make_token(has_path ? TK_PATH : TK_IDENT, s, L->src + L->i, line, col);
    }
    die("invalid character");
    return make_token(TK_EOF, s, s, line, col);
}

bool token_is(const Token* t, const char* lit) {
    size_t n = (size_t)(t->end - t->start);
    char* s = (char *)malloc(n + 1);
    if (!s) die("Fatal: Out of Memory");
    memcpy(s, t->start, n);
    s[n] = 0;
    return s;
}
