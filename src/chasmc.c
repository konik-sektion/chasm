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

    TK_STRING,
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
        case ':': return make_token(L, TK_COLON, s, s+1, line, col);
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
        while (L->i < L->len && is_ident((unsigned char)L->src[L->i])) {
            L->i++;
            L->col++;
        }
        return make_token(L, TK_IDENT, s, L->src + L->i, line, col);
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
    L.name = xstrdup(name);
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
    Lexer* L;
    Token cur;
    Out* O;
} P;

static void next(P* p){ p->cur = next_token(p->L); }
static void expect(P* p, TokenKind k, const char* msg){ if (p->cur.kind!=k) die(msg); next(p); }

static void skip_nl(P* p){
    while (p->cur.kind==TK_NL) next(p);
}

// forward decls
static void emit_expr(P* p, FrameLayout* F);

static void emit_load_ident(P* p, FrameLayout* F, const Token* id) {
    char* name = token_str(id);
    Local* L = find_local(F, name);
    free(name);
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

static void emit_store_ident(P* p, FrameLayout* F, const Token* id) {
    char* name = token_str(id);
    Local* L = find_local(F, name);
    free(name);
    if (!L) die("unknown identifier (local not found)");

    const char* sz = nasm_size(L->ty);
    // store low part of rax
    if (L->ty == TY_U8) outfmt(p->O, "    mov %s [rbp%+d], al\n", sz, L->rbp_off);
    else if (L->ty == TY_U16) outfmt(p->O, "    mov %s [rbp%+d], ax\n", sz, L->rbp_off);
    else if (L->ty == TY_U32) outfmt(p->O, "    mov %s [rbp%+d], eax\n", sz, L->rbp_off);
    else outfmt(p->O, "    mov %s [rbp%+d], rax\n", sz, L->rbp_off);
}

static void emit_call(P* p, FrameLayout* F, const Token* callee) {
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
    char* fname = token_str(callee);
    outfmt(p->O, "    call %s\n", fname);
    free(fname);
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
        Token id = p->cur;
        next(p);

        // function call?
        if (p->cur.kind == TK_LPAREN) {
            next(p);
            emit_call(p, F, &id);
            return;
        }

        emit_load_ident(p, F, &id);
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

static void parse_and_emit_func(P* p, const Token* name_tok, bool is_global) {
    (void)is_global;

    char* fname = token_str(name_tok);

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
        if (p->cur.kind == TK_DEDENT) { next(p); break; }
        if (p->cur.kind == TK_NL) { next(p); continue; }

        // let
        if (p->cur.kind == TK_IDENT && tok_is(&p->cur, "let")) {
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
        if (p->cur.kind == TK_IDENT && tok_is(&p->cur, "ret")) {
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
            break;
        }

        // call statement
        if (p->cur.kind == TK_IDENT && tok_is(&p->cur, "call")) {
            next(p);
            if (p->cur.kind != TK_IDENT) die("expected function name after call");
            Token callee = p->cur; next(p);
            expect(p, TK_LPAREN, "expected '(' after call name");
            next(p);
            emit_call(p, &F, &callee);
            expect(p, TK_SEMI, "expected ';' after call");
            next(p);
            continue;
        }

        // inline asm
        if (p->cur.kind == TK_AT) {
            die("@asm nyi fuq u");
        }

        die("unsupported statement");
    }

    free(fname);
}

static void translate(const char* in_path, const char* out_path) {
    size_t len=0;
    char* src = read_file_all(in_path, &len);

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

    FILE* out = fopen(out_path, "wb");
    if (!out) die("cannot open output file");

    Out O = { out };
    P p = { &L, {0}, &O };
    next(&p);

    // NASM header
    outln(&O, "default rel");

    // default section
    outln(&O, "section .text");

    while (p.cur.kind != TK_EOF) {
        if (p.cur.kind == TK_NL) { next(&p); continue; }

        // #section
        if (p.cur.kind == TK_HASH) {
            next(&p);
            if (p.cur.kind != TK_IDENT) die("expected directive after #");
            if (tok_is(&p.cur, "section")) {
                next(&p);
                if (p.cur.kind != TK_IDENT) die("expected section name");
                if (tok_is(&p.cur,"program")) outln(&O, "section .text");
                else if (tok_is(&p.cur,"data")) outln(&O, "section .data");
                else if (tok_is(&p.cur,"rodata")) outln(&O, "section .rodata");
                else if (tok_is(&p.cur,"bss")) outln(&O, "section .bss");
                else die("unknown section");
                next(&p);
                continue;
            }
            die("unknown #directive");
        }

        // global label block: global main:
        if (p.cur.kind == TK_IDENT && tok_is(&p.cur, "global")) {
            next(&p);
            if (p.cur.kind != TK_IDENT) die("expected label name after global");
            Token name = p.cur;
            char* sname = token_str(&name);
            next(&p);
            expect(&p, TK_COLON, "expected ':' after global name");
            outfmt(&O, "global %s\n", sname);
            outfmt(&O, "%s:\n", sname);
            free(sname);
            next(&p);
            skip_nl(&p);
            expect(&p, TK_INDENT, "expected indented block after global label");
            while (p.cur.kind != TK_DEDENT && p.cur.kind != TK_EOF) next(&p);
            if (p.cur.kind == TK_DEDENT) next(&p);
            continue;
        }

        // func
        if (p.cur.kind == TK_IDENT && tok_is(&p.cur, "func")) {
            next(&p);
            if (p.cur.kind != TK_IDENT) die("expected function name");
            Token fname = p.cur; next(&p);
            parse_and_emit_func(&p, &fname, false);
            continue;
        }

        die("unexpected top-level token");
    }

    fclose(out);
    free(src);
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
