#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

static void die(const char* msg) {
    fprintf(stderr, "chasmc error: %s\n", msg);
    exit(1);
}

static char* read_file_all(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) die("cannot open input file");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) die("ftell failed");
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) die("out of memory");
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    if (out_len) *out_len = got;
    return buf;
}

// =============================== lexer ===============================

typedef enum {
    TK_EOF=0,
    TK_NL,
    TK_INDENT,
    TK_DEDENT,

    TK_IDENT,
    TK_INT,

    TK_HASH,
    TK_COLON,
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

    TK_RARROW,

    TK_AT,

    TK_DOLLAR,
    TK_STRING,
    TK_SCOPE,
    TK_PATH,
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

static bool is_ident_start(int c) {
    return isalpha(c) || c=='_';
}

static bool is_ident(int c) {
    return isalnum(c) || c=='_';
}

static bool is_path_char(int c) {
    return isalnum(c) || c=='_' || c=='/' || c=='.' || c=='-';
}

static Token make_token(Lexer* L, TokenKind k, const char* s, const char* e, int line, int col) {
    Token     t;
    t.kind  = k;
    t.start = s;
    t.end   = e;
    t.line  = line;
    t.col   = col;
    return    t;
}

static void skip_ws_inline(Lexer* L) {
    while (L->i < L->len) {
        char c = L->src[L->i];
        if (c==' ' || c=='\t' || c=='\r') {
            L->i++;
            L->col++;
            continue;
        }
        break;
    }
}

static void skip_comment(Lexer* L) {
    if (L->i + 2 < L->len && L->src[L->i]==';' &&  L->src[L->i+1]==';' && L->src[L->i+2]==';') {
        while (L->i < L->len && L->src[L->i] != '\n') {
            L->i++; L->col++;
        }
    }
}

static Token next_token(Lexer* L) {
    // handle pending dedents
    if (L->pending_dedents > 0) {
        L->pending_dedents--;
        return make_token(L, TK_DEDENT, L->src + L->i, L->src + L->i, L->line, L->col);
    }

    // eof -> emit remaining dedents then eof
    if (L->i >= L->len) {
        if (L->indent_top > 0) {
            L->indent_top--;
            return make_token(L, TK_DEDENT, L->src + L->i, L->src + L->i, L->line, L->col);
        }
        return make_token(L, TK_EOF, L->src + L->i, L->src + L->i, L->line, L->col);
    }

    // at start of a nl do a computey thing of indentation or whatever fuq u
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

        if (j < L->len && (L->src[j] == '\n')) {

        } else {
            int cur = L->indent_stack[L->indent_top];
            if (indent > cur) {
                // push
                L->indent_top++;
                L->indent_stack[L->indent_top] = indent;
                // advance to first non space char
                L->col += (int)(j - L->i);
                L->i = j;
                L->at_line_start = false;
                return make_token(L, TK_INDENT, L->src + L->i, L->src + L->i, line, col);
            } else if (indent < cur) {
                // pop until matches
                int pops = 0;
                while (L->indent_top > 0 && indent < L->indent_stack[L->indent_top]) {
                    L->indent_top--;
                    pops++;
                }
                if (indent != L->indent_stack[L->indent_top]) {
                    die("indentation error (misaligned dedent)");
                }
                L->col += (int)(j - L->i);
                L->i = j;
                L->at_line_start = false;

                L->pending_dedents = pops - 1;
                return make_token(L, TK_DEDENT, L->src + L->i, L->src + L->i, line, col);
            }

            // equal indentation
            L->col += (int)(j - L->i);
            L->i = j;
        }

        L->at_line_start = false;
    }

    // skip inline spaces and comments
    skip_ws_inline(L);
    skip_comment(L);
    

    // newline
    if (L->i < L->len && L->src[L->i] == '\n') {
        const char* s = L->src + L->i;
        L->i++;
        L->line++;
        L->col = 1;
        L->at_line_start = true;
        return make_token(L, TK_NL, s, s+1, L->line-1, 1);
    }

    if (L->i >= L->len) return next_token(L);

    int line = L->line, col = L->col;
    const char* s = L->src + L->i;
    char c = L->src[L->i++];
    L->col++;

    // punctuation
    switch (c) {
        case '#': return make_token(L, TK_HASH, s, s+1, line, col);
        case ':': {
            if (L->i < L->len && L->src[L->i] == ':') {
                L->i++;
                L->col++;
                return make_token(L, TK_SCOPE, s, s+2, line, col);
            }
            return make_token(L, TK_COLON, s, s+1, line, col);
        }
        case ';': return make_token(L, TK_SEMI, s, s+1, line, col);
        case ',': return make_token(L, TK_COMMA, s, s+1, line, col);
        case '(': return make_token(L, TK_LPAREN, s, s+1, line, col);
        case ')': return make_token(L, TK_RPAREN, s, s+1, line, col);
        case '{': return make_token(L, TK_LBRACE, s, s+1, line, col);
        case '}': return make_token(L, TK_RBRACE, s, s+1, line, col);
        case '[': return make_token(L, TK_LBRACKET, s, s+1, line, col);
        case ']': return make_token(L, TK_RBRACKET, s, s+1, line, col);
        case '=': return make_token(L, TK_EQ, s, s+1, line, col);
        case '+': return make_token(L, TK_PLUS, s, s+1, line, col);
        case '-': return make_token(L, TK_MINUS, s, s+1, line, col);
        case '$': return make_token(L, TK_DOLLAR, s, s+1, line, col);
        case '@': return make_token(L, TK_AT, s, s+1, line, col);
        case '>': {
            // maybe rarrow?!
            if (L->i < L->len && L->src[L->i] == '>') {
                L->i++;
                L->col++;
                return make_token(L, TK_RARROW, s, s+2, line, col);
            }
        } break;
        case '"': {
            // str literal (no escapes)
            const char* start = L->src + L->i;
            while (L->i < L->len && L->src[L->i] != '"') {
                if (L->src[L->i] == '\n') die("unterminated string literal");
                L->i++;
                L->col++;
            }
            if (L->i >= L->len) die("unterminated string literal");
            const char* end = L->src + L->i;

            L->i++;
            L->col++;

            // token spans without quotes (start .. end)
            Token t = make_token(L, TK_STRING, start, end, line, col);
            return t;
        }
        default: break;
    }

    if (c == '.' || c == '/') {
        while (L->i < L->len && is_path_char((unsigned char)L->src[L->i])) {
            L->i++;
            L->col++;
        }
        return make_token(L, TK_PATH, s, L->src + L->i, line, col);
    }

    // integer literals
    if (isdigit((unsigned char)c)) {
        while (L->i < L->len && isdigit((unsigned char)L->src[L->i])) {
            L->i++;
            L->col++;
        }
        return make_token(L, TK_INT, s, L->src + L->i, line, col);
    }

    // identifier
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
        return make_token(L, has_path ? TK_PATH : TK_IDENT, s, L->src + L->i, line, col);
    }

    die("invalid character");
    return make_token(L, TK_EOF, s, s, line, col);
}

static bool token_is(const Token* t, const char* lit) {
    size_t n = (size_t)(t->end - t->start);
    return strlen(lit)==n && strncmp(t->start, lit, n)==0;
}

static char* token_str(const Token* t) {
    size_t n = (size_t)(t->end - t->start);
    char* s = (char*)malloc(n+1);
    if (!s) die("oom");
    memcpy(s, t->start, n);
    s[n]=0;
    return s;
}

// =============================== parser emitter ===============================

typedef enum {
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    TY_NULL
} Type;

static Type parse_type_name(const Token* t) {
    if (token_is(t, "u8")) return TY_U8;
    if (token_is(t, "u16")) return TY_U16;
    if (token_is(t, "u32")) return TY_U32;
    if (token_is(t, "u64")) return TY_U64;
    if (token_is(t, "Null") || token_is(t, "null")) return TY_NULL;
    die("unknown type name");
    return TY_U64;
}

static const char* nasm_size(Type ty) {
    switch (ty) {
        case TY_U8: return "byte";
        case TY_U16: return "word";
        case TY_U32: return "dword";
        case TY_U64: return "qword";
        default: return "qword";
    }
}

// what is the best way to measure the length of my dict in bytes?
static int size_bytes(Type ty) {
    switch (ty) {
        case TY_U8: return 1;
        case TY_U16: return 2;
        case TY_U32: return 4;
        case TY_U64: return 8;
        default: return 0;
    }
}

typedef struct {
    char* name;
    Type ty;
    int rbp_off; // negative offset from rbp
} Local;

typedef struct {
    Local* locals;
    size_t nlocals, cap;
    int stack_used; // bytes
} FrameLayout;

static void add_local(FrameLayout* F, const char* name, Type ty) {
    if (F->nlocals+1 > F->cap) {
        F->cap = (F->cap==0)? 16 : F->cap*2;
        F->locals = (Local*)realloc(F->locals, F->cap*sizeof(Local));
        if (!F->locals) die("oom");
    }
    int sz = size_bytes(ty);
    F->stack_used += sz;
    if (F->stack_used % 8) F->stack_used += (8 - (F->stack_used % 8));

    Local L;
    L.name = strdup(name);
    L.ty = ty;
    L.rbp_off = -F->stack_used;
    F->locals[F->nlocals++] = L;
}

static Local* find_local(FrameLayout* F, const char* name) {
    for (size_t i=0;i<F->nlocals;i++) {
        if (strcmp(F->locals[i].name, name)==0) return &F->locals[i];
    }
    return NULL;
}

// output buffer
typedef struct {
    FILE* out;
} Out;

static void outln(Out* O, const char* s) { fprintf(O->out, "%s\n", s); }
static void outfmt(Out* O, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(O->out, fmt, ap);
    va_end(ap);
}

typedef struct {
    char* name;
    char* qualified;
} FuncSymbol;

typedef struct FuncTable {
    FuncSymbol* items;
    size_t count;
    size_t cap;
} FuncTable;

static void add_func_symbol(FuncTable* table, const char* name, const char* qualified) {
    if (table->count + 1 > table->cap) {
        table->cap = (table->cap == 0) ? 16 : table->cap * 2;
        table->items = (FuncSymbol*)realloc(table->items, table->cap * sizeof(FuncSymbol));
        if (!table->items) die("oom");
    }
    table->items[table->count++] = (FuncSymbol){ strdup(name), strdup(qualified) };
}

static const char* lookup_func_symbol(FuncTable* table, const char* name) {
    const char* hit = NULL;
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            if (hit) die("ambiguous function name; use namespace qualifier");
            hit = table->items[i].qualified;
        }
    }
    return hit;
}

static void free_func_table(FuncTable* table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].name);
        free(table->items[i].qualified);
    }
    free(table->items);
}

typedef struct {
    Lexer* L;
    Token cur;
    Out* O;
    char* current_namespace;
    char** using_namespaces;
    size_t using_count;
    size_t using_cap;
    struct FuncTable* func_table;
    const char* file_path;
} P;

static void next(P* p){ p->cur = next_token(p->L); }
static void expect(P* p, TokenKind k, const char* msg){ if (p->cur.kind!=k) die(msg); next(p); }

static void skip_nl(P* p){
    while (p->cur.kind==TK_NL) next(p);
}

static char* join_namespace(const char* ns, const char* name) {
    size_t nlen = strlen(ns);
    size_t mlen = strlen(name);
    char* out = (char*)malloc(nlen + 2 + mlen + 1);
    if (!out) die("oom");
    memcpy(out, ns, nlen);
    out[nlen] = '_';
    out[nlen + 1] = '_';
    memcpy(out + nlen + 2, name, mlen);
    out[nlen + 2 + mlen] = 0;
    return out;
}

static void add_using_namespace(P* p, const char* name) {
    if (p->using_count + 1 > p->using_cap) {
        p->using_cap = (p->using_cap == 0) ? 8 : p->using_cap * 2;
        p->using_namespaces = (char**)realloc(p->using_namespaces, p->using_cap * sizeof(char*));
        if (!p->using_namespaces) die("oom");
    }
    p->using_namespaces[p->using_count++] = strdup(name);
}

static char* resolve_definition_name(P* p, const char* name) {
    if (p->current_namespace) return join_namespace(p->current_namespace, name);
    return strdup(name);
}

static char* resolve_reference_name(P* p, const char* name, const char* explicit_ns) {
    if (explicit_ns) return join_namespace(explicit_ns, name);

    const char* local = lookup_func_symbol(p->func_table, name);
    if (local) return strdup(local);
    if (p->current_namespace) return join_namespace(p->current_namespace, name);
    if (p->using_count == 1) return join_namespace(p->using_namespaces[0], name);
    if (p->using_count > 1) die("ambiguous namespace reference; use <ns>::<name>");
    return strdup(name);
}

typedef struct {
    char* name;
    char* ns;
} QualifiedName;

static QualifiedName parse_qualified_name(P* p) {
    if (p->cur.kind != TK_IDENT) die("expected identifier");
    char* first = token_str(&p->cur);
    next(p);
    if (p->cur.kind == TK_SCOPE) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected identifier after '::'");
        char* second = token_str(&p->cur);
        next(p);
        return (QualifiedName){ second, first };
    }
    return (QualifiedName){ first, NULL };
}

// forward decls
static void emit_expr(P* p, FrameLayout* F);

static void emit_load_local(P* p, FrameLayout* F, const char* name) {
    Local* L = find_local(F, name);
    if (!L) die("unknown identifier (local not found)");

    // movzx rax, <size> [rbp+off]
    const char* sz = nasm_size(L->ty);
    if (L->ty == TY_U64) {
        outfmt(p->O, "    mov rax, %s [rbp%+d]\n", sz, L->rbp_off);
    } else {
        // movzx rax, byte/word/dword [rbp-off]
        outfmt(p->O, "    movzx rax, %s [rbp%+d]\n", sz, L->rbp_off);
    }
}

static void emit_store_local(P* p, FrameLayout* F, const char* name) {
    Local* L = find_local(F, name);
    if (!L) die("unknown identifier (local not found)");

    const char* sz = nasm_size(L->ty);
    // store low part of rax
    if (L->ty == TY_U8) outfmt(p->O, "    mov %s [rbp%+d], al\n", sz, L->rbp_off);
    else if (L->ty == TY_U16) outfmt(p->O, "    mov %s [rbp%+d], ax\n", sz, L->rbp_off);
    else if (L->ty == TY_U32) outfmt(p->O, "    mov %s [rbp%+d], eax\n", sz, L->rbp_off);
    else outfmt(p->O, "    mov %s [rbp%+d], rax\n", sz, L->rbp_off);
}

static void emit_call(P* p, FrameLayout* F, const char* callee) {
    static const char* argregs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    int argc = 0;

    if (p->cur.kind != TK_RPAREN) {
        for (;;) {
            emit_expr(p, F); // result in rax
            if (argc >= 6) die("too many args (supports 6)");
            outfmt(p->O, "    mov %s, rax\n", argregs[argc]);
            argc++;

            if (p->cur.kind == TK_COMMA) { next(p); continue; }
            break;
        }
    }
    expect(p, TK_RPAREN, "expected ')' after call args");

    // call
    outfmt(p->O, "    call %s\n", callee);
    // return value in rax
}

static void emit_factor(P* p, FrameLayout* F) {
    if (p->cur.kind == TK_INT) {
        char* n = token_str(&p->cur);
        outfmt(p->O, "    mov rax, %s\n", n);
        free(n);
        next(p);
        return;
    }
    if (p->cur.kind == TK_IDENT) {
        QualifiedName qn = parse_qualified_name(p);

        if (qn.ns) {
            if (p->cur.kind != TK_LPAREN) die("namespaced identifier must be a call");
            next(p);
            char* fname = resolve_reference_name(p, qn.name, qn.ns);
            emit_call(p, F, fname);
            free(fname);
            free(qn.name);
            free(qn.ns);
            return;
        }

        // function call?
        if (p->cur.kind == TK_LPAREN) {
            next(p);
            char* fname = resolve_reference_name(p, qn.name, NULL);
            emit_call(p, F, fname);
            free(fname);
            free(qn.name);
            return;
        }

        emit_load_local(p, F, qn.name);
        free(qn.name);
        return;
    }
    if (p->cur.kind == TK_LPAREN) {
        next(p);
        emit_expr(p, F);
        expect(p, TK_RPAREN, "expected ')'");
        return;
    }
    die("expected expression atom"); // legit kinda forgot what i meant by atom
}

static void emit_expr(P* p, FrameLayout* F) {
    emit_factor(p, F);
    while (p->cur.kind == TK_PLUS || p->cur.kind == TK_MINUS) {
        TokenKind op = p->cur.kind;
        next(p);

        // save left in rbx (scratch)
        outln(p->O, "    mov rbx, rax");
        emit_factor(p, F);

        if (op == TK_PLUS) outln(p->O, "    add rax, rbx");
        else outln(p->O, "    sub rbx, rax\n    mov rax, rbx"); // rax = left - right
    }
}

// =============================== statements ===============================

static void emit_inline_asm(P* p) {
    int brace_depth = 1;
    die("fuck you inline asm not yet implemented");
}

static void emit_macro_statement(P* p) {
    if (p->cur.kind != TK_IDENT) die("expected macro name after '$'");
    const char* start = p->cur.start;
    const char* end = p->cur.end;

    for (;;) {
        next(p);
        if (p->cur.kind == TK_SEMI) break;
        if (p->cur.kind == TK_EOF || p->cur.kind == TK_NL) die("expected ';' after macro invocation");
        end = p->cur.end;
    }

    size_t len = (size_t)(end - start);
    char* line = (char*)malloc(len + 1);
    if (!line) die("oom");
    memcpy(line, start, len);
    line[len] = 0;
    outfmt(p->O, "    %s\n", line);
    free(line);
    next(p);
}

static void parse_and_emit_func(P* p, const char* raw_name, bool is_global, bool is_inline) {
    (void)is_inline;

    char* fname = resolve_definition_name(p, raw_name);

    // parse params
    expect(p, TK_LPAREN, "expected '(' after func name");

    // collect param names/types
    typedef struct { char* name; Type ty; } Param;
    Param params[16];
    int nparams = 0;

    if (p->cur.kind != TK_RPAREN) {
        for (;;) {
            if (p->cur.kind != TK_IDENT) die("expected param name");
            char* pn = token_str(&p->cur);
            next(p);
            expect(p, TK_COLON, "expected ':' in param");
            if (p->cur.kind != TK_IDENT) die("expected type after ':'");
            Type ty = parse_type_name(&p->cur);
            next(p);

            params[nparams++] = (Param){pn, ty};

            if (p->cur.kind == TK_COMMA) { next(p); continue; }
            break;
        }
    }
    expect(p, TK_RPAREN, "expected ')' after params");

    // return type: >> type
    expect(p, TK_RARROW, "expected '>>' return type");
    if (p->cur.kind != TK_IDENT) die("expected return type name");
    Type ret_ty = parse_type_name(&p->cur);
    next(p);

    expect(p, TK_COLON, "expected ':' after function header");
    skip_nl(p);
    expect(p, TK_INDENT, "expected indented function body");

    // emit label
    if (is_global) outfmt(p->O, "global %s\n", fname);
    outfmt(p->O, "%s:\n", fname);

    // prologue
    outln(p->O, "    push rbp");
    outln(p->O, "    mov rbp, rsp");

    FrameLayout F = {0};

    // map params to stack locals for simplicity: allocate locals and store regs to them
    static const char* argregs[] = {"rdi","rsi","rdx","rcx","r8","r9"};
    for (int i=0;i<nparams;i++) {
        add_local(&F, params[i].name, params[i].ty);
    }

    // reserve stack space
    if (F.stack_used > 0) {
        outfmt(p->O, "    sub rsp, %d\n", F.stack_used);
    }

    // store incoming args to stack locals
    for (int i=0;i<nparams;i++) {
        Local* Lc = find_local(&F, params[i].name);
        const char* sz = nasm_size(Lc->ty);
        if (i >= 6) die("too many params (phase1 supports 6)");
        if (Lc->ty == TY_U8)  outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "dil");
        else if (Lc->ty == TY_U16) outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "di");
        else if (Lc->ty == TY_U32) outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "edi");
        else outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, argregs[i]);
    }

    // statement loop until DEDENT
    for (;;) {
        if (p->cur.kind == TK_DEDENT) {
            next(p);
            if (p->cur.kind == TK_IDENT && token_is(&p->cur, "end")) {
                next(p);
            }
            break;
        }
        if (p->cur.kind == TK_NL) { next(p); continue; }

        // let
        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "let")) {
            next(p);
            if (p->cur.kind != TK_IDENT) die("expected local name after let");
            Token lname = p->cur; next(p);
            expect(p, TK_COLON, "expected ':' after local name");
            if (p->cur.kind != TK_IDENT) die("expected type name");
            Type ty = parse_type_name(&p->cur); next(p);
            expect(p, TK_EQ, "expected '=' after let type");
            // compute expr into rax
            emit_expr(p, &F);
            expect(p, TK_SEMI, "expected ';' after let");
            next(p);

            add_local(&F, token_str(&lname), ty);
            die("ts isnt supported yet fuq u");
        }

        // ret
        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "ret")) {
            next(p);
            if (p->cur.kind != TK_SEMI) {
                emit_expr(p, &F); // into rax
            } else {
                // ret; => return 0 for u64, or 0 for smaller for now
                outln(p->O, "    xor rax, rax");
            }
            expect(p, TK_SEMI, "expected ';' after ret");
            next(p);

            // epilogue
            outln(p->O, "    leave");
            outln(p->O, "    ret");
            // function ends here; consume until DEDENT
            while (p->cur.kind != TK_DEDENT && p->cur.kind != TK_EOF) next(p);
            if (p->cur.kind == TK_DEDENT) next(p);
            if (p->cur.kind == TK_IDENT && token_is(&p->cur, "end")) {
                next(p);
            }
            break;
        }

        // call statement
        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "call")) {
            next(p);
            if (p->cur.kind != TK_IDENT) die("expected function name after call");
            QualifiedName qn = parse_qualified_name(p);
            expect(p, TK_LPAREN, "expected '(' after call name");
            next(p);
            char* fname = resolve_reference_name(p, qn.name, qn.ns);
            emit_call(p, &F, fname);
            free(fname);
            free(qn.name);
            free(qn.ns);
            expect(p, TK_SEMI, "expected ';' after call");
            next(p);
            continue;
        }

        // inline asm
        if (p->cur.kind == TK_AT) {
            die("@asm nyi fuq u");
        }

        if (p->cur.kind == TK_DOLLAR) {
            next(p);
            emit_macro_statement(p);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "end")) {
            next(p);
            break;
        }

        die("unsupported statement");
    }

    free(fname);
}

typedef struct {
    char** paths;
    size_t count;
    size_t cap;
} ImportSet;

static bool import_seen(ImportSet* set, const char* path) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->paths[i], path) == 0) return true;
    }
    return false;
}

static void add_import(ImportSet* set, const char* path) {
    if (set->count + 1 > set->cap) {
        set->cap = (set->cap == 0) ? 16 : set->cap * 2;
        set->paths = (char**)realloc(set->paths, set->cap * sizeof(char*));
        if (!set->paths) die("oom");
    }
    set->paths[set->count++] = strdup(path);
}

static char* resolve_import_path(const char* from_path, const char* import_path) {
    if (import_path[0] == '/') return strdup(import_path);
    const char* slash = strrchr(from_path, '/');
    if (!slash) return strdup(import_path);
    size_t dir_len = (size_t)(slash - from_path + 1);
    size_t rel_len = strlen(import_path);
    char* out = (char*)malloc(dir_len + rel_len + 1);
    if (!out) die("oom");
    memcpy(out, from_path, dir_len);
    memcpy(out + dir_len, import_path, rel_len);
    out[dir_len + rel_len] = 0;
    return out;
}

static void collect_func_symbols(const char* src, size_t len, FuncTable* table) {
    Lexer L = {0};
    L.src = src;
    L.len = len;
    L.i = 0;
    L.line = 1;
    L.col = 1;
    L.indent_top = 0;
    L.indent_stack[0] = 0;
    L.at_line_start = true;
    L.pending_dedents = 0;

    char* current_namespace = NULL;
    for (;;) {
        Token t = next_token(&L);
        if (t.kind == TK_EOF) break;
        if (t.kind == TK_HASH) {
            Token dir = next_token(&L);
            if (dir.kind != TK_IDENT) die("expected directive after #");
            if (token_is(&dir, "module")) {
                Token name = next_token(&L);
                if (name.kind != TK_IDENT) die("expected module name after #module");
                free(current_namespace);
                current_namespace = token_str(&name);
                continue;
            }
            if (token_is(&dir, "endmodule")) {
                free(current_namespace);
                current_namespace = NULL;
                continue;
            }
        }

        if (t.kind == TK_IDENT && (token_is(&t, "local") || token_is(&t, "global"))) {
            Token maybe_inline = next_token(&L);
            if (maybe_inline.kind == TK_IDENT && token_is(&maybe_inline, "inline")) {
                maybe_inline = next_token(&L);
            }
            if (maybe_inline.kind != TK_IDENT || !token_is(&maybe_inline, "func")) {
                die("expected 'func' after local/global");
            }
            Token name = next_token(&L);
            if (name.kind != TK_IDENT) die("expected function name");
            char* raw = token_str(&name);
            char* qualified = current_namespace ? join_namespace(current_namespace, raw) : strdup(raw);
            add_func_symbol(table, raw, qualified);
            free(raw);
            free(qualified);
            continue;
        }
    }

    free(current_namespace);
}

static void compile_file(const char* in_path, Out* O, ImportSet* imports, bool emit_header) {
    if (import_seen(imports, in_path)) return;
    add_import(imports, in_path);

    size_t len=0;
    char* src = read_file_all(in_path, &len);

    FuncTable table = {0};
    collect_func_symbols(src, len, &table);

    Lexer L = {0};
    L.src = src;
    L.len = len;
    L.i = 0;
    L.line = 1;
    L.col = 1;
    L.indent_top = 0;
    L.indent_stack[0] = 0;
    L.at_line_start = true;
    L.pending_dedents = 0;

    P p = { &L, {0}, O };
    p.current_namespace = NULL;
    p.using_namespaces = NULL;
    p.using_count = 0;
    p.using_cap = 0;
    p.func_table = &table;
    p.file_path = in_path;
    next(&p);

    if (emit_header) {
        outln(O, "default rel");
        outln(O, "section .text");
    }

    while (p.cur.kind != TK_EOF) {
        if (p.cur.kind == TK_NL) { next(&p); continue; }

        // #section, #module, #endmodule, #import, #uns
        if (p.cur.kind == TK_HASH) {
            next(&p);
            if (p.cur.kind != TK_IDENT) die("expected directive after #");
            if (token_is(&p.cur, "section")) {
                next(&p);
                if (p.cur.kind != TK_IDENT) die("expected section name");
                if (token_is(&p.cur,"program")) outln(O, "section .text");
                else if (token_is(&p.cur,"data")) outln(O, "section .data");
                else if (token_is(&p.cur,"rodata")) outln(O, "section .rodata");
                else if (token_is(&p.cur,"bss")) outln(O, "section .bss");
                else die("unknown section");
                next(&p);
                continue;
            }
            if (token_is(&p.cur, "module")) {
                next(&p);
                if (p.cur.kind != TK_IDENT) die("expected module name after #module");
                free(p.current_namespace);
                p.current_namespace = token_str(&p.cur);
                next(&p);
                continue;
            }
            if (token_is(&p.cur, "endmodule")) {
                if (!p.current_namespace) die("#endmodule without active module");
                free(p.current_namespace);
                p.current_namespace = NULL;
                next(&p);
                continue;
            }
            if (token_is(&p.cur, "import")) {
                next(&p);
                if (p.cur.kind != TK_IDENT && p.cur.kind != TK_STRING && p.cur.kind != TK_PATH) {
                    die("expected path after #import");
                }
                char* import_token = token_str(&p.cur);
                char* resolved = resolve_import_path(p.file_path, import_token);
                free(import_token);
                next(&p);
                compile_file(resolved, O, imports, false);
                free(resolved);
                continue;
            }
            if (token_is(&p.cur, "uns")) {
                next(&p);
                if (p.cur.kind != TK_IDENT) die("expected namespace after #uns");
                char* ns = token_str(&p.cur);
                add_using_namespace(&p, ns);
                free(ns);
                next(&p);
                continue;
            }
            die("unknown #directive");
        }

        // func declarations must be annotated with local/global
        if (p.cur.kind == TK_IDENT && (token_is(&p.cur, "local") || token_is(&p.cur, "global"))) {
            bool is_global = token_is(&p.cur, "global");
            next(&p);
            bool is_inline = false;
            if (p.cur.kind == TK_IDENT && token_is(&p.cur, "inline")) {
                is_inline = true;
                next(&p);
            }
            if (p.cur.kind != TK_IDENT || !token_is(&p.cur, "func")) {
                die("expected 'func' after local/global");
            }
            next(&p);
            if (p.cur.kind != TK_IDENT) die("expected function name");
            char* raw = token_str(&p.cur);
            next(&p);
            parse_and_emit_func(&p, raw, is_global, is_inline);
            free(raw);
            continue;
        }

        if (p.cur.kind == TK_IDENT && token_is(&p.cur, "func")) {
            die("functions must be declared with 'local func' or 'global func'");
        }

        if (p.cur.kind == TK_IDENT && token_is(&p.cur, "global")) {
            die("global is a modifier for func; use 'global func'");
        }

        die("unexpected top-level token");
    }

    for (size_t i = 0; i < p.using_count; i++) free(p.using_namespaces[i]);
    free(p.using_namespaces);
    free(p.current_namespace);
    free_func_table(&table);
    free(src);
}

static void translate(const char* in_path, const char* out_path) {
    FILE* out = fopen(out_path, "wb");
    if (!out) die("cannot open output file");

    Out O = { out };
    ImportSet imports = {0};
    compile_file(in_path, &O, &imports, true);
    for (size_t i = 0; i < imports.count; i++) free(imports.paths[i]);
    free(imports.paths);

    fclose(out);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: chasmc <input.chasm> -o <output.asm>\n");
        return 1;
    }

    const char* in_path = argv[1];
    const char* out_path = "out.asm";

    for (int i=2;i<argc;i++) {
        if (strcmp(argv[i], "-o")==0 && i+1<argc) {
            out_path = argv[i+1];
            i++;
        }
    }

    translate(in_path, out_path);
    printf("wrote %s\n", out_path);
    return 0;
}
