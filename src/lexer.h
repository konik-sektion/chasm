#ifndef CHASMC_LEXER_H
#define CHASMC_LEXER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TK_EOF = 0,
    TK_NL,
    TK_INDENT,
    TK_DEDENT,

    TK_IDENT,
    TK_INT,
    
    TK_HASH,
    TK_COMMA,
    TK_SEMI,
    TK_COMMA,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LBRACKET,
    TK_RBRACKET,
    
    TK_EQ,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_AMP,

    TK_RARROW,

    TK_AT,

    TK_DOLLAR,
    TK_STRING,
    TK_SCOPE,
    TK_PATH,
    TK_CHAR,
    TK_PERCENT_IDENT,
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* start;
    const char* end;
    int line;
    int col;
} Token;

typedef struct {
    const char* src;
    size_t len;
    size_t i;
    int line;
    int col;

    int indent_stack[128];
    int indent_top;
    bool at_line_start;
    int pending_dedents;
} Lexer;

void lexer_init(Lexer* L, const char* src, size_t len);
Token next_token(Lexer* L);
bool token_is(const Token* t, const char* lit);
char *token_str(const Token* t);

#endif
