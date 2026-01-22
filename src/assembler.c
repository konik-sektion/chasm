#include "assembler.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "util.h"

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
} Symbol;

typedef struct {
    Symbol* items;
    size_t count;
    size_t cap;
} SymbolTable;

static void add_symbol(SymbolTable* table, const char* name, const char* qualified) {
    if (table->count + 1 > table->cap) {
        table->cap = (table->cap == 0) ? 16 : table->cap*  2;
        table->items = (Symbol* )realloc(table->items, table->cap*  sizeof(Symbol));
        if (!table->items) die("oom");
    }
    table->items[table->count++] = (Symbol){xstrdup(name), xstrdup(qualified)};
}

static const char* lookup_symbol(SymbolTable* table, const char* name) {
    const char* hit = NULL;
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            if (hit) die("ambiguous name; use namespace qualifier");
            hit = table->items[i].qualified;
        }
    }
    return hit;
}

static void free_symbol_table(SymbolTable* table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].name);
        free(table->items[i].qualified);
    }
    free(table->items);
}

typedef struct {
    char* name;
    int arity;
    char* body;
} Macro;

typedef struct {
    Macro* items;
    size_t count;
    size_t cap;
    SymbolTable symbols;
} MacroTable;

static void add_macro(MacroTable* table, const char* name, int arity, const char* body) {
    if (table->count + 1 > table->cap) {
        table->cap = (table->cap == 0) ? 8 : table->cap*  2;
        table->items = (Macro* )realloc(table->items, table->cap*  sizeof(Macro));
        if (!table->items) die("oom");
    }
    table->items[table->count++] = (Macro){xstrdup(name), arity, xstrdup(body)};
}

static Macro* find_macro(MacroTable* table, const char* name) {
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) return &table->items[i];
    }
    return NULL;
}

static void free_macro_table(MacroTable* table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].name);
        free(table->items[i].body);
    }
    free(table->items);
    free_symbol_table(&table->symbols);
}

typedef enum {
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    TY_I8,
    TY_I16,
    TY_I32,
    TY_I64,
    TY_NULL,
    TY_UNKNOWN
} TypeKind;

typedef struct {
    TypeKind kind;
} Type;

static Type parse_type_name(const Token* t) {
    if (token_is(t, "u8")) return (Type){TY_U8};
    if (token_is(t, "u16")) return (Type){TY_U16};
    if (token_is(t, "u32")) return (Type){TY_U32};
    if (token_is(t, "u64")) return (Type){TY_U64};
    if (token_is(t, "i8")) return (Type){TY_I8};
    if (token_is(t, "i16")) return (Type){TY_I16};
    if (token_is(t, "i32")) return (Type){TY_I32};
    if (token_is(t, "i64")) return (Type){TY_I64};
    if (token_is(t, "Null") || token_is(t, "null")) return (Type){TY_NULL};
    return (Type){TY_UNKNOWN};
}

static int type_size(Type ty) {
    switch (ty.kind) {
        case TY_U8:
        case TY_I8:
            return 1;
        case TY_U16:
        case TY_I16:
            return 2;
        case TY_U32:
        case TY_I32:
            return 4;
        case TY_U64:
        case TY_I64:
            return 8;
        default:
            return 0;
    }
}

static const char* nasm_size(Type ty) {
    switch (ty.kind) {
        case TY_U8:
        case TY_I8:
            return "byte";
        case TY_U16:
        case TY_I16:
            return "word";
        case TY_U32:
        case TY_I32:
            return "dword";
        case TY_U64:
        case TY_I64:
            return "qword";
        default:
            return "qword";
    }
}

static const char* nasm_data_directive(Type ty) {
    switch (ty.kind) {
        case TY_U8:
        case TY_I8:
            return "db";
        case TY_U16:
        case TY_I16:
            return "dw";
        case TY_U32:
        case TY_I32:
            return "dd";
        case TY_U64:
        case TY_I64:
            return "dq";
        default:
            return "dq";
    }
}

typedef struct {
    char* name;
    Type ty;
    int rbp_off;
} Local;

typedef struct {
    Local* locals;
    size_t nlocals, cap;
    int stack_used;
} FrameLayout;

static void add_local(FrameLayout* F, const char* name, Type ty) {
    if (F->nlocals + 1 > F->cap) {
        F->cap = (F->cap == 0) ? 16 : F->cap*  2;
        F->locals = (Local* )realloc(F->locals, F->cap*  sizeof(Local));
        if (!F->locals) die("oom");
    }
    int sz = type_size(ty);
    F->stack_used += sz ? sz : 8;
    if (F->stack_used % 8) F->stack_used += (8 - (F->stack_used % 8));

    Local L;
    L.name = xstrdup(name);
    L.ty = ty;
    L.rbp_off = -F->stack_used;
    F->locals[F->nlocals++] = L;
}

static Local* find_local(FrameLayout* F, const char* name) {
    for (size_t i = 0; i < F->nlocals; i++) {
        if (strcmp(F->locals[i].name, name) == 0) return &F->locals[i];
    }
    return NULL;
}

typedef enum {
    SEC_NONE,
    SEC_TEXT,
    SEC_DATA,
    SEC_BSS,
    SEC_RODATA,
    SEC_MACROS
} Section;

typedef struct {
    char* name;
    Type ty;
    int reserve_count;
} GlobalVar;

typedef struct {
    GlobalVar* items;
    size_t count;
    size_t cap;
    SymbolTable symbols;
} GlobalTable;

static void add_global(GlobalTable* table,
                       const char* raw_name,
                       const char* qualified_name,
                       Type ty,
                       int reserve_count) {
    if (table->count + 1 > table->cap) {
        table->cap = (table->cap == 0) ? 16 : table->cap*  2;
        table->items = (GlobalVar* )realloc(table->items, table->cap*  sizeof(GlobalVar));
        if (!table->items) die("oom");
    }
    table->items[table->count++] = (GlobalVar){xstrdup(qualified_name), ty, reserve_count};
    add_symbol(&table->symbols, raw_name, qualified_name);
}

static GlobalVar* find_global(GlobalTable* table, const char* name) {
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) return &table->items[i];
    }
    return NULL;
}

static void free_global_table(GlobalTable* table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].name);
    }
    free(table->items);
    free_symbol_table(&table->symbols);
}

typedef struct {
    char* *paths;
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
        set->cap = (set->cap == 0) ? 16 : set->cap*  2;
        set->paths = (char* *)realloc(set->paths, set->cap*  sizeof(char* ));
        if (!set->paths) die("oom");
    }
    set->paths[set->count++] = xstrdup(path);
}

static char* resolve_import_path(const char* from_path, const char* import_path) {
    if (import_path[0] == '/') return xstrdup(import_path);
    const char* slash = strrchr(from_path, '/');
    if (!slash) return xstrdup(import_path);
    size_t dir_len = (size_t)(slash - from_path + 1);
    size_t rel_len = strlen(import_path);
    char* out = (char* )malloc(dir_len + rel_len + 1);
    if (!out) die("oom");
    memcpy(out, from_path, dir_len);
    memcpy(out + dir_len, import_path, rel_len);
    out[dir_len + rel_len] = 0;
    return out;
}

typedef struct {
    SymbolTable funcs;
    GlobalTable globals;
    MacroTable macros;
    ImportSet scanned;
} CompileContext;

static char* join_namespace(const char* ns, const char* name) {
    size_t nlen = strlen(ns);
    size_t mlen = strlen(name);
    char* out = (char* )malloc(nlen + 2 + mlen + 1);
    if (!out) die("oom");
    memcpy(out, ns, nlen);
    out[nlen] = '_';
    out[nlen + 1] = '_';
    memcpy(out + nlen + 2, name, mlen);
    out[nlen + 2 + mlen] = 0;
    return out;
}

static char* resolve_definition_name(const char* current_ns, const char* name) {
    if (current_ns) return join_namespace(current_ns, name);
    return xstrdup(name);
}

static char* resolve_reference_name(const char* current_ns,
                                    const char* name,
                                    const char* explicit_ns,
                                    const char* *using_namespaces,
                                    size_t using_count,
                                    SymbolTable* table) {
    if (explicit_ns) return join_namespace(explicit_ns, name);
    const char* qualified = lookup_symbol(table, name);
    if (qualified) return xstrdup(qualified);
    if (current_ns) return join_namespace(current_ns, name);
    if (using_count == 1) return join_namespace(using_namespaces[0], name);
    if (using_count > 1) die("ambiguous namespace reference; use <ns>::<name>");
    return xstrdup(name);
}

static void scan_file_for_symbols(CompileContext* ctx, const char* path);

static void scan_imports_in_file(CompileContext* ctx, const char* path, const char* src, size_t len) {
    Lexer L;
    lexer_init(&L, src, len);
    for (;;) {
        Token t = next_token(&L);
        if (t.kind == TK_EOF) break;
        if (t.kind == TK_HASH) {
            Token dir = next_token(&L);
            if (dir.kind == TK_IDENT && token_is(&dir, "import")) {
                Token path_tok = next_token(&L);
                if (path_tok.kind != TK_IDENT && path_tok.kind != TK_STRING && path_tok.kind != TK_PATH) {
                    die("expected path after #import");
                }
                char* import_token = token_str(&path_tok);
                char* resolved = resolve_import_path(path, import_token);
                free(import_token);
                scan_file_for_symbols(ctx, resolved);
                free(resolved);
            }
        }
    }
}

static bool is_reserve_directive(const Token* t) {
    return token_is(t, "resb") || token_is(t, "resw") || token_is(t, "resd") || token_is(t, "resq");
}

static Type type_for_reserve(const Token* t) {
    if (token_is(t, "resb")) return (Type){TY_U8};
    if (token_is(t, "resw")) return (Type){TY_U16};
    if (token_is(t, "resd")) return (Type){TY_U32};
    if (token_is(t, "resq")) return (Type){TY_U64};
    return (Type){TY_UNKNOWN};
}

static void scan_file_for_symbols(CompileContext* ctx, const char* path) {
    if (import_seen(&ctx->scanned, path)) return;
    add_import(&ctx->scanned, path);

    size_t len = 0;
    char* src = read_file_all(path, &len);
    scan_imports_in_file(ctx, path, src, len);

    Lexer L;
    lexer_init(&L, src, len);

    char* current_namespace = NULL;
    Section section = SEC_NONE;

    for (;;) {
        Token t = next_token(&L);
        if (t.kind == TK_EOF) break;
        if (t.kind == TK_HASH) {
            Token dir = next_token(&L);
            if (dir.kind != TK_IDENT) continue;
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
            if (token_is(&dir, "section")) {
                Token name = next_token(&L);
                if (name.kind != TK_IDENT) die("expected section name");
                if (token_is(&name, "program")) section = SEC_TEXT;
                else if (token_is(&name, "data")) section = SEC_DATA;
                else if (token_is(&name, "bss")) section = SEC_BSS;
                else if (token_is(&name, "readonly")) section = SEC_RODATA;
                else if (token_is(&name, "macros")) section = SEC_MACROS;
                else section = SEC_NONE;
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
            char* qualified = current_namespace ? join_namespace(current_namespace, raw) : xstrdup(raw);
            add_symbol(&ctx->funcs, raw, qualified);
            free(raw);
            free(qualified);
            continue;
        }

        if (section == SEC_DATA || section == SEC_BSS || section == SEC_RODATA) {
            if (t.kind == TK_IDENT && token_is(&t, "let")) {
                Token name = next_token(&L);
                if (name.kind != TK_IDENT && name.kind != TK_STAR) die("expected variable name after let");
                bool pointer_name = false;
                if (name.kind == TK_STAR) {
                    pointer_name = true;
                    name = next_token(&L);
                }
                if (name.kind != TK_IDENT) die("expected variable name after let");
                char* raw = token_str(&name);
                char* qualified = resolve_definition_name(current_namespace, raw);

                Token maybe_colon = next_token(&L);
                Type ty = (Type){TY_UNKNOWN};
                int reserve_count = 1;
                if (maybe_colon.kind == TK_COLON) {
                    Token type_token = next_token(&L);
                    ty = parse_type_name(&type_token);
                    if (ty.kind == TY_UNKNOWN && is_reserve_directive(&type_token)) {
                        ty = type_for_reserve(&type_token);
                        Token count_tok = next_token(&L);
                        if (count_tok.kind != TK_INT) die("expected reserve count");
                        char* count_str = token_str(&count_tok);
                        reserve_count = atoi(count_str);
                        free(count_str);
                    }
                }

                if (ty.kind == TY_UNKNOWN && pointer_name) ty.kind = TY_U64;
                if (ty.kind == TY_UNKNOWN) ty.kind = TY_U64;

                add_global(&ctx->globals, raw, qualified, ty, reserve_count);
                free(raw);
                free(qualified);
                continue;
            }
        }

        if (section == SEC_MACROS) {
            if (t.kind == TK_IDENT && token_is(&t, "def")) {
                Token name = next_token(&L);
                if (name.kind != TK_IDENT) die("expected macro name");
                char* raw = token_str(&name);
                char* qualified = resolve_definition_name(current_namespace, raw);
                Token maybe_comma = next_token(&L);
                if (maybe_comma.kind == TK_COMMA) {
                    Token count_tok = next_token(&L);
                    if (count_tok.kind != TK_INT) die("expected macro arity");
                    char* count_str = token_str(&count_tok);
                    (void)count_str;
                    free(count_str);
                }
                add_symbol(&ctx->macros.symbols, raw, qualified);
                free(raw);
                free(qualified);
                continue;
            }
        }
    }

    free(current_namespace);
    free(src);
}

typedef struct {
    Lexer* L;
    Token cur;
    Out* O;
    char* current_namespace;
    char* *using_namespaces;
    size_t using_count;
    size_t using_cap;
    SymbolTable* func_table;
    SymbolTable* global_symbols;
    MacroTable* macro_table;
    GlobalTable* globals;
    Section current_section;
} Parser;

static void next(Parser* p) { p->cur = next_token(p->L); }
static void expect(Parser* p, TokenKind k, const char* msg) {
    if (p->cur.kind != k) die(msg);
    next(p);
}

static void skip_nl(Parser* p) {
    while (p->cur.kind == TK_NL) next(p);
}

typedef struct {
    char* name;
    char* ns;
} QualifiedName;

static QualifiedName parse_qualified_name(Parser* p) {
    if (p->cur.kind != TK_IDENT) die("expected identifier");
    char* first = token_str(&p->cur);
    next(p);
    if (p->cur.kind == TK_SCOPE) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected identifier after '::'");
        char* second = token_str(&p->cur);
        next(p);
        return (QualifiedName){second, first};
    }
    return (QualifiedName){first, NULL};
}

static void add_using_namespace(Parser* p, const char* name) {
    if (p->using_count + 1 > p->using_cap) {
        p->using_cap = (p->using_cap == 0) ? 8 : p->using_cap*  2;
        p->using_namespaces = (char* *)realloc(p->using_namespaces, p->using_cap*  sizeof(char* ));
        if (!p->using_namespaces) die("oom");
    }
    p->using_namespaces[p->using_count++] = xstrdup(name);
}

static char* trim_ws(const char* start, const char* end) {
    while (start < end && (*start == ' ' ||* start == '\t' ||* start == '\n' ||* start == '\r')) start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    size_t len = (size_t)(end - start);
    char* out = (char* )malloc(len + 1);
    if (!out) die("oom");
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}

static char* substring(const char* start, const char* end) {
    size_t len = (size_t)(end - start);
    char* out = (char* )malloc(len + 1);
    if (!out) die("oom");
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}

static void emit_raw_block(Out* O, const char* text) {
    if (!text || !*text) return;
    const char* cursor = text;
    while (*cursor) {
        const char* line_end = strchr(cursor, '\n');
        if (!line_end) {
            outfmt(O, "%s\n", cursor);
            break;
        }
        outfmt(O, "%.*s\n", (int)(line_end - cursor), cursor);
        cursor = line_end + 1;
    }
}

static void emit_asm_from_text(Out* O, const char* text) {
    const char* cursor = text;
    while (*cursor) {
        const char* asm_pos = strstr(cursor, "@asm");
        if (!asm_pos) {
            emit_raw_block(O, cursor);
            break;
        }
        if (asm_pos > cursor) {
            char* prefix = substring(cursor, asm_pos);
            emit_raw_block(O, prefix);
            free(prefix);
        }
        const char* brace = strchr(asm_pos, '{');
        if (!brace) die("expected '{' after @asm");
        int depth = 1;
        const char* block_start = brace + 1;
        const char* scan = block_start;
        while (*scan && depth > 0) {
            if (*scan == '{') depth++;
            else if (*scan == '}') depth--;
            scan++;
        }
        if (depth != 0) die("unterminated @asm block");
        const char* block_end = scan - 1;
        char* block = substring(block_start, block_end);
        emit_raw_block(O, block);
        free(block);
        cursor = scan;
    }
}

static char* parse_inline_block(Parser* p) {
    if (p->cur.kind != TK_AT) die("expected @asm");
    next(p);
    if (p->cur.kind != TK_IDENT || !token_is(&p->cur, "asm")) die("expected asm after @");
    next(p);
    if (p->cur.kind != TK_LBRACE) die("expected '{' after @asm");

    size_t start = p->L->i;
    int depth = 1;
    size_t i = p->L->i;
    while (i < p->L->len && depth > 0) {
        char c = p->L->src[i];
        if (c == '{') depth++;
        else if (c == '}') depth--;
        i++;
    }
    if (depth != 0) die("unterminated @asm block");

    size_t end = i - 1;
    size_t j = start;
    while (j < end) {
        if (p->L->src[j] == '\n') {
            p->L->line++;
            p->L->col = 1;
        } else {
            p->L->col++;
        }
        j++;
    }
    p->L->i = i;
    p->cur = next_token(p->L);

    return substring(p->L->src + start, p->L->src + end);
}

static char* capture_until_enddef(Parser* p) {
    const char* body_start = p->cur.start;
    while (p->cur.kind != TK_EOF) {
        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "enddef")) {
            const char* body_end = p->cur.start;
            char* body = substring(body_start, body_end);
            next(p);
            return body;
        }
        next(p);
    }
    die("unterminated macro definition");
    return NULL;
}

static char* replace_placeholder(const char* text, const char* placeholder, const char* value) {
    size_t tlen = strlen(text);
    size_t plen = strlen(placeholder);
    size_t vlen = strlen(value);
    size_t count = 0;
    const char* scan = text;
    while ((scan = strstr(scan, placeholder)) != NULL) {
        count++;
        scan += plen;
    }
    size_t new_len = tlen + count*  (vlen - plen);
    char* out = (char* )malloc(new_len + 1);
    if (!out) die("oom");
    char* dst = out;
    scan = text;
    const char* hit;
    while ((hit = strstr(scan, placeholder)) != NULL) {
        size_t chunk = (size_t)(hit - scan);
        memcpy(dst, scan, chunk);
        dst += chunk;
        memcpy(dst, value, vlen);
        dst += vlen;
        scan = hit + plen;
    }
    strcpy(dst, scan);
    return out;
}

static char* expand_macro_body(const char* body, char* *args, int argc) {
    char* result = xstrdup(body);
    for (int i = 0; i < argc; i++) {
        char placeholder[16];
        snprintf(placeholder, sizeof(placeholder), "%%%d", i + 1);
        char* next = replace_placeholder(result, placeholder, args[i]);
        free(result);
        result = next;
    }
    return result;
}

static void emit_load_local(Out* O, FrameLayout* F, const char* name) {
    Local* L = find_local(F, name);
    if (!L) die("unknown identifier (local not found)");
    const char* sz = nasm_size(L->ty);
    if (type_size(L->ty) == 8) {
        outfmt(O, "    mov rax, %s [rbp%+d]\n", sz, L->rbp_off);
    } else if (L->ty.kind == TY_I8 || L->ty.kind == TY_I16 || L->ty.kind == TY_I32) {
        outfmt(O, "    movsx rax, %s [rbp%+d]\n", sz, L->rbp_off);
    } else {
        outfmt(O, "    movzx rax, %s [rbp%+d]\n", sz, L->rbp_off);
    }
}

static void emit_store_local(Out* O, FrameLayout* F, const char* name) {
    Local* L = find_local(F, name);
    if (!L) die("unknown identifier (local not found)");
    const char* sz = nasm_size(L->ty);
    if (L->ty.kind == TY_U8 || L->ty.kind == TY_I8) outfmt(O, "    mov %s [rbp%+d], al\n", sz, L->rbp_off);
    else if (L->ty.kind == TY_U16 || L->ty.kind == TY_I16) outfmt(O, "    mov %s [rbp%+d], ax\n", sz, L->rbp_off);
    else if (L->ty.kind == TY_U32 || L->ty.kind == TY_I32) outfmt(O, "    mov %s [rbp%+d], eax\n", sz, L->rbp_off);
    else outfmt(O, "    mov %s [rbp%+d], rax\n", sz, L->rbp_off);
}

static void emit_load_global(Out* O, GlobalTable* globals, const char* name) {
    GlobalVar* G = find_global(globals, name);
    if (!G) die("unknown identifier (global not found)");
    const char* sz = nasm_size(G->ty);
    if (type_size(G->ty) == 8) {
        outfmt(O, "    mov rax, %s [rel %s]\n", sz, name);
    } else if (G->ty.kind == TY_I8 || G->ty.kind == TY_I16 || G->ty.kind == TY_I32) {
        outfmt(O, "    movsx rax, %s [rel %s]\n", sz, name);
    } else {
        outfmt(O, "    movzx rax, %s [rel %s]\n", sz, name);
    }
}

static void emit_store_global(Out* O, GlobalTable* globals, const char* name) {
    GlobalVar* G = find_global(globals, name);
    if (!G) die("unknown identifier (global not found)");
    const char* sz = nasm_size(G->ty);
    if (G->ty.kind == TY_U8 || G->ty.kind == TY_I8) outfmt(O, "    mov %s [rel %s], al\n", sz, name);
    else if (G->ty.kind == TY_U16 || G->ty.kind == TY_I16) outfmt(O, "    mov %s [rel %s], ax\n", sz, name);
    else if (G->ty.kind == TY_U32 || G->ty.kind == TY_I32) outfmt(O, "    mov %s [rel %s], eax\n", sz, name);
    else outfmt(O, "    mov %s [rel %s], rax\n", sz, name);
}

static void emit_expr(Parser* p, FrameLayout* F);

static void emit_call(Parser* p, FrameLayout* F, const char* callee) {
    static const char* argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int argc = 0;

    if (p->cur.kind != TK_RPAREN) {
        for (;;) {
            emit_expr(p, F);
            if (argc >= 6) die("too many args (supports 6)");
            outfmt(p->O, "    mov %s, rax\n", argregs[argc]);
            argc++;
            if (p->cur.kind == TK_COMMA) {
                next(p);
                continue;
            }
            break;
        }
    }
    expect(p, TK_RPAREN, "expected ')' after call args");
    outfmt(p->O, "    call %s\n", callee);
}

static void emit_factor(Parser* p, FrameLayout* F) {
    if (p->cur.kind == TK_MINUS) {
        next(p);
        emit_factor(p, F);
        outln(p->O, "    neg rax");
        return;
    }
    if (p->cur.kind == TK_INT) {
        char* n = token_str(&p->cur);
        outfmt(p->O, "    mov rax, %s\n", n);
        free(n);
        next(p);
        return;
    }
    if (p->cur.kind == TK_AMP) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected identifier after &");
        QualifiedName qn = parse_qualified_name(p);
        char* name = resolve_reference_name(p->current_namespace,
                                            qn.name,
                                            qn.ns,
                                            (const char* *)p->using_namespaces,
                                            p->using_count,
                                            p->global_symbols);
        outfmt(p->O, "    lea rax, [rel %s]\n", name);
        free(name);
        free(qn.name);
        free(qn.ns);
        return;
    }
    if (p->cur.kind == TK_STAR) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected identifier after '*'");
        QualifiedName qn = parse_qualified_name(p);
        Local* local = find_local(F, qn.name);
        if (local) {
            emit_load_local(p->O, F, qn.name);
        } else {
            char* name = resolve_reference_name(p->current_namespace,
                                                qn.name,
                                                qn.ns,
                                                (const char* *)p->using_namespaces,
                                                p->using_count,
                                                p->global_symbols);
            emit_load_global(p->O, p->globals, name);
            free(name);
        }
        outln(p->O, "    mov rbx, rax");
        outln(p->O, "    mov rax, [rbx]");
        free(qn.name);
        free(qn.ns);
        return;
    }
    if (p->cur.kind == TK_IDENT) {
        QualifiedName qn = parse_qualified_name(p);

        if (qn.ns) {
            if (p->cur.kind != TK_LPAREN) die("namespaced identifier must be a call");
            next(p);
            char* fname = resolve_reference_name(p->current_namespace,
                                                 qn.name,
                                                 qn.ns,
                                                 (const char* *)p->using_namespaces,
                                                 p->using_count,
                                                 p->func_table);
            emit_call(p, F, fname);
            free(fname);
            free(qn.name);
            free(qn.ns);
            return;
        }

        if (p->cur.kind == TK_LPAREN) {
            next(p);
            char* fname = resolve_reference_name(p->current_namespace,
                                                 qn.name,
                                                 NULL,
                                                 (const char* *)p->using_namespaces,
                                                 p->using_count,
                                                 p->func_table);
            emit_call(p, F, fname);
            free(fname);
            free(qn.name);
            return;
        }

        Local* local = find_local(F, qn.name);
        if (local) {
            emit_load_local(p->O, F, qn.name);
        } else {
            char* name = resolve_reference_name(p->current_namespace,
                                                qn.name,
                                                NULL,
                                                (const char* *)p->using_namespaces,
                                                p->using_count,
                                                p->global_symbols);
            emit_load_global(p->O, p->globals, name);
            free(name);
        }
        free(qn.name);
        return;
    }
    if (p->cur.kind == TK_LPAREN) {
        next(p);
        emit_expr(p, F);
        expect(p, TK_RPAREN, "expected ')'");
        return;
    }
    die("expected expression atom");
}

static void emit_expr(Parser* p, FrameLayout* F) {
    emit_factor(p, F);
    while (p->cur.kind == TK_PLUS || p->cur.kind == TK_MINUS) {
        TokenKind op = p->cur.kind;
        next(p);
        outln(p->O, "    mov rbx, rax");
        emit_factor(p, F);
        if (op == TK_PLUS) outln(p->O, "    add rax, rbx");
        else outln(p->O, "    sub rbx, rax\n    mov rax, rbx");
    }
}

static void emit_macro_invocation(Parser* p) {
    if (p->cur.kind != TK_IDENT) die("expected macro name after '$'");
    QualifiedName qn = parse_qualified_name(p);
    char* macro_name = resolve_reference_name(p->current_namespace,
                                              qn.name,
                                              qn.ns,
                                              (const char* *)p->using_namespaces,
                                              p->using_count,
                                              &p->macro_table->symbols);
    free(qn.name);
    free(qn.ns);

    char* args[16];
    int argc = 0;
    if (p->cur.kind == TK_COMMA) {
        next(p);
        if (p->cur.kind != TK_SEMI) {
            const char* arg_start = p->cur.start;
            const char* arg_end = p->cur.end;
            for (;;) {
                if (p->cur.kind == TK_SEMI) {
                    char* arg = trim_ws(arg_start, arg_end);
                    if (*arg) args[argc++] = arg;
                    else free(arg);
                    break;
                }
                if (p->cur.kind == TK_COMMA) {
                    char* arg = trim_ws(arg_start, arg_end);
                    if (*arg) args[argc++] = arg;
                    else free(arg);
                    next(p);
                    arg_start = p->cur.start;
                    arg_end = p->cur.end;
                    continue;
                }
                arg_end = p->cur.end;
                next(p);
            }
        } else {
            next(p);
        }
    } else {
        expect(p, TK_SEMI, "expected ';' after macro invocation");
    }

    Macro* macro = find_macro(p->macro_table, macro_name);
    if (macro) {
        char* expanded = expand_macro_body(macro->body, args, argc);
        emit_asm_from_text(p->O, expanded);
        free(expanded);
    } else {
        outfmt(p->O, "    %s", macro_name);
        if (argc > 0) {
            outfmt(p->O, " ");
            for (int i = 0; i < argc; i++) {
                outfmt(p->O, "%s%s", args[i], (i + 1 < argc) ? ", " : "");
            }
        }
        outln(p->O, "");
    }

    for (int i = 0; i < argc; i++) free(args[i]);
    free(macro_name);
    if (p->cur.kind == TK_SEMI) next(p);
}

static void parse_and_emit_func(Parser* p, const char* raw_name, bool is_global, bool is_inline) {
    (void)is_inline;

    char* fname = resolve_definition_name(p->current_namespace, raw_name);

    expect(p, TK_LPAREN, "expected '(' after func name");

    typedef struct {
        char* name;
        Type ty;
    } Param;
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
            if (ty.kind == TY_UNKNOWN) die("unknown type name");
            next(p);

            params[nparams++] = (Param){pn, ty};

            if (p->cur.kind == TK_COMMA) {
                next(p);
                continue;
            }
            break;
        }
    }
    expect(p, TK_RPAREN, "expected ')' after params");

    expect(p, TK_RARROW, "expected '>>' return type");
    if (p->cur.kind != TK_IDENT) die("expected return type name");
    Type ret_ty = parse_type_name(&p->cur);
    (void)ret_ty;
    next(p);

    expect(p, TK_COLON, "expected ':' after function header");
    skip_nl(p);
    expect(p, TK_INDENT, "expected indented function body");

    if (is_global) outfmt(p->O, "global %s\n", fname);
    outfmt(p->O, "%s:\n", fname);
    outln(p->O, "    push rbp");
    outln(p->O, "    mov rbp, rsp");

    FrameLayout F = {0};

    static const char* argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    for (int i = 0; i < nparams; i++) {
        add_local(&F, params[i].name, params[i].ty);
    }

    if (F.stack_used > 0) {
        outfmt(p->O, "    sub rsp, %d\n", F.stack_used);
    }

    for (int i = 0; i < nparams; i++) {
        Local* Lc = find_local(&F, params[i].name);
        const char* sz = nasm_size(Lc->ty);
        if (i >= 6) die("too many params (phase1 supports 6)");
        if (Lc->ty.kind == TY_U8 || Lc->ty.kind == TY_I8)
            outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "dil");
        else if (Lc->ty.kind == TY_U16 || Lc->ty.kind == TY_I16)
            outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "di");
        else if (Lc->ty.kind == TY_U32 || Lc->ty.kind == TY_I32)
            outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, "edi");
        else
            outfmt(p->O, "    mov %s [rbp%+d], %s\n", sz, Lc->rbp_off, argregs[i]);
    }

    for (;;) {
        if (p->cur.kind == TK_DEDENT) {
            next(p);
            if (p->cur.kind == TK_IDENT && token_is(&p->cur, "end")) {
                next(p);
            }
            break;
        }
        if (p->cur.kind == TK_NL) {
            next(p);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "let")) {
            next(p);
            bool pointer_name = false;
            if (p->cur.kind == TK_STAR) {
                pointer_name = true;
                next(p);
            }
            if (p->cur.kind != TK_IDENT) die("expected local name after let");
            Token lname = p->cur;
            next(p);

            Type ty = (Type){TY_UNKNOWN};
            if (p->cur.kind == TK_COLON) {
                next(p);
                if (p->cur.kind != TK_IDENT) die("expected type name");
                ty = parse_type_name(&p->cur);
                if (ty.kind == TY_UNKNOWN) die("unknown type name");
                next(p);
            }
            if (ty.kind == TY_UNKNOWN && pointer_name) ty.kind = TY_U64;
            if (ty.kind == TY_UNKNOWN) ty.kind = TY_U64;

            if (p->cur.kind == TK_EQ) {
                next(p);
                emit_expr(p, &F);
            } else {
                outln(p->O, "    xor rax, rax");
            }
            expect(p, TK_SEMI, "expected ';' after let");
            next(p);

            char* lname_str = token_str(&lname);
            add_local(&F, lname_str, ty);
            emit_store_local(p->O, &F, lname_str);
            free(lname_str);
            continue;
        }

        if (p->cur.kind == TK_IDENT && (token_is(&p->cur, "ret") || token_is(&p->cur, "return"))) {
            next(p);
            if (p->cur.kind != TK_SEMI) {
                emit_expr(p, &F);
            } else {
                outln(p->O, "    xor rax, rax");
            }
            expect(p, TK_SEMI, "expected ';' after return");
            next(p);

            outln(p->O, "    leave");
            outln(p->O, "    ret");
            while (p->cur.kind != TK_DEDENT && p->cur.kind != TK_EOF) next(p);
            if (p->cur.kind == TK_DEDENT) next(p);
            if (p->cur.kind == TK_IDENT && token_is(&p->cur, "end")) {
                next(p);
            }
            break;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "set")) {
            next(p);
            bool deref = false;
            if (p->cur.kind == TK_STAR) {
                deref = true;
                next(p);
            }
            if (p->cur.kind != TK_IDENT) die("expected name after set");
            QualifiedName qn = parse_qualified_name(p);
            if (p->cur.kind == TK_COLON) {
                next(p);
                if (p->cur.kind != TK_IDENT) die("expected type after ':'");
                next(p);
            }
            expect(p, TK_EQ, "expected '=' after set target");
            emit_expr(p, &F);
            expect(p, TK_SEMI, "expected ';' after set");
            next(p);

            if (deref) {
                outln(p->O, "    mov rcx, rax");
                Local* local = find_local(&F, qn.name);
                if (local) {
                    emit_load_local(p->O, &F, qn.name);
                } else {
                    char* name = resolve_reference_name(p->current_namespace,
                                                        qn.name,
                                                        qn.ns,
                                                        (const char* *)p->using_namespaces,
                                                        p->using_count,
                                                        p->global_symbols);
                    emit_load_global(p->O, p->globals, name);
                    free(name);
                }
                outln(p->O, "    mov rbx, rax");
                outln(p->O, "    mov [rbx], rcx");
            } else {
                Local* local = find_local(&F, qn.name);
                if (local) {
                    emit_store_local(p->O, &F, qn.name);
                } else {
                    char* name = resolve_reference_name(p->current_namespace,
                                                        qn.name,
                                                        qn.ns,
                                                        (const char* *)p->using_namespaces,
                                                        p->using_count,
                                                        p->global_symbols);
                    emit_store_global(p->O, p->globals, name);
                    free(name);
                }
            }
            free(qn.name);
            free(qn.ns);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "push")) {
            next(p);
            for (;;) {
                emit_expr(p, &F);
                outln(p->O, "    push rax");
                if (p->cur.kind == TK_COMMA) {
                    next(p);
                    continue;
                }
                break;
            }
            expect(p, TK_SEMI, "expected ';' after push");
            next(p);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "pop")) {
            next(p);
            for (;;) {
                bool deref = false;
                if (p->cur.kind == TK_STAR) {
                    deref = true;
                    next(p);
                }
                if (p->cur.kind != TK_IDENT) die("expected identifier after pop");
                QualifiedName qn = parse_qualified_name(p);
                if (p->cur.kind == TK_COLON) {
                    next(p);
                    if (p->cur.kind == TK_IDENT) next(p);
                }
                outln(p->O, "    pop rax");
                if (deref) {
                    outln(p->O, "    mov rcx, rax");
                    Local* local = find_local(&F, qn.name);
                    if (local) {
                        emit_load_local(p->O, &F, qn.name);
                    } else {
                        char* name = resolve_reference_name(p->current_namespace,
                                                            qn.name,
                                                            qn.ns,
                                                            (const char* *)p->using_namespaces,
                                                            p->using_count,
                                                            p->global_symbols);
                        emit_load_global(p->O, p->globals, name);
                        free(name);
                    }
                    outln(p->O, "    mov rbx, rax");
                    outln(p->O, "    mov [rbx], rcx");
                } else {
                    Local* local = find_local(&F, qn.name);
                    if (local) {
                        emit_store_local(p->O, &F, qn.name);
                    } else {
                        char* name = resolve_reference_name(p->current_namespace,
                                                            qn.name,
                                                            qn.ns,
                                                            (const char* *)p->using_namespaces,
                                                            p->using_count,
                                                            p->global_symbols);
                        emit_store_global(p->O, p->globals, name);
                        free(name);
                    }
                }
                free(qn.name);
                free(qn.ns);
                if (p->cur.kind == TK_COMMA) {
                    next(p);
                    continue;
                }
                break;
            }
            expect(p, TK_SEMI, "expected ';' after pop");
            next(p);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "void")) {
            next(p);
            while (p->cur.kind != TK_SEMI && p->cur.kind != TK_EOF) next(p);
            expect(p, TK_SEMI, "expected ';' after void");
            next(p);
            continue;
        }

        if (p->cur.kind == TK_IDENT && token_is(&p->cur, "call")) {
            next(p);
            if (p->cur.kind != TK_IDENT) die("expected function name after call");
            QualifiedName qn = parse_qualified_name(p);
            expect(p, TK_LPAREN, "expected '(' after call name");
            next(p);
            char* fname = resolve_reference_name(p->current_namespace,
                                                 qn.name,
                                                 qn.ns,
                                                 (const char* *)p->using_namespaces,
                                                 p->using_count,
                                                 p->func_table);
            emit_call(p, &F, fname);
            free(fname);
            free(qn.name);
            free(qn.ns);
            expect(p, TK_SEMI, "expected ';' after call");
            next(p);
            continue;
        }

        if (p->cur.kind == TK_AT) {
            char* block = parse_inline_block(p);
            emit_raw_block(p->O, block);
            free(block);
            continue;
        }

        if (p->cur.kind == TK_DOLLAR) {
            next(p);
            emit_macro_invocation(p);
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

static void parse_global_let(Parser* p) {
    next(p);
    bool pointer_name = false;
    if (p->cur.kind == TK_STAR) {
        pointer_name = true;
        next(p);
    }
    if (p->cur.kind != TK_IDENT) die("expected variable name after let");
    char* raw = token_str(&p->cur);
    next(p);
    Type ty = (Type){TY_UNKNOWN};
    int reserve_count = 1;

    if (p->cur.kind == TK_COLON) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected type name after ':'");
        ty = parse_type_name(&p->cur);
        if (ty.kind == TY_UNKNOWN && is_reserve_directive(&p->cur)) {
            ty = type_for_reserve(&p->cur);
            next(p);
            if (p->cur.kind != TK_INT) die("expected reserve count");
            char* count_str = token_str(&p->cur);
            reserve_count = atoi(count_str);
            free(count_str);
        }
        next(p);
    }
    if (ty.kind == TY_UNKNOWN && pointer_name) ty.kind = TY_U64;
    if (ty.kind == TY_UNKNOWN) ty.kind = TY_U64;

    char* qualified = resolve_definition_name(p->current_namespace, raw);
    add_global(p->globals, raw, qualified, ty, reserve_count);

    if (p->current_section == SEC_BSS) {
        if (reserve_count <= 0) reserve_count = 1;
        const char* directive = "resb";
        if (ty.kind == TY_U16 || ty.kind == TY_I16) directive = "resw";
        else if (ty.kind == TY_U32 || ty.kind == TY_I32) directive = "resd";
        else if (ty.kind == TY_U64 || ty.kind == TY_I64) directive = "resq";
        outfmt(p->O, "%s: %s %d\n", qualified, directive, reserve_count);
        expect(p, TK_SEMI, "expected ';' after let");
        next(p);
        free(raw);
        free(qualified);
        return;
    }

    if (p->cur.kind == TK_EQ) {
        next(p);
        const char* start = p->cur.start;
        const char* end = p->cur.end;
        while (p->cur.kind != TK_SEMI) {
            if (p->cur.kind == TK_EOF || p->cur.kind == TK_NL) die("expected ';' after let");
            end = p->cur.end;
            next(p);
        }
        char* value = trim_ws(start, end);
        if (!*value) {
            free(value);
            value = xstrdup("0");
        }
        outfmt(p->O, "%s: %s %s\n", qualified, nasm_data_directive(ty), value);
        free(value);
        next(p);
    } else {
        outfmt(p->O, "%s: %s 0\n", qualified, nasm_data_directive(ty));
        expect(p, TK_SEMI, "expected ';' after let");
        next(p);
    }

    free(raw);
    free(qualified);
}

static void parse_macro_definition(Parser* p) {
    next(p);
    if (p->cur.kind != TK_IDENT) die("expected macro name");
    char* raw = token_str(&p->cur);
    next(p);

    int arity = 0;
    if (p->cur.kind == TK_COMMA) {
        next(p);
        if (p->cur.kind != TK_INT) die("expected macro arity");
        char* count_str = token_str(&p->cur);
        arity = atoi(count_str);
        free(count_str);
        next(p);
    }

    expect(p, TK_COLON, "expected ':' after macro header");
    char* qualified = resolve_definition_name(p->current_namespace, raw);
    char* body = capture_until_enddef(p);
    add_macro(p->macro_table, qualified, arity, body);
    free(body);
    free(raw);
    free(qualified);
}

static void compile_path(const char* path, Out* O, CompileContext* ctx, ImportSet* imports, bool emit_header);

static void handle_directive(Parser* p,
                             const char* path,
                             Out* O,
                             CompileContext* ctx,
                             ImportSet* imports) {
    if (p->cur.kind != TK_IDENT) die("expected directive after #");

    if (token_is(&p->cur, "section")) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected section name");
        if (token_is(&p->cur, "program")) {
            outln(p->O, "section .text");
            p->current_section = SEC_TEXT;
        } else if (token_is(&p->cur, "data")) {
            outln(p->O, "section .data");
            p->current_section = SEC_DATA;
        } else if (token_is(&p->cur, "readonly")) {
            outln(p->O, "section .rodata");
            p->current_section = SEC_RODATA;
        } else if (token_is(&p->cur, "bss")) {
            outln(p->O, "section .bss");
            p->current_section = SEC_BSS;
        } else if (token_is(&p->cur, "macros")) {
            p->current_section = SEC_MACROS;
        } else {
            die("unknown section");
        }
        next(p);
        return;
    }
    if (token_is(&p->cur, "module")) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected module name after #module");
        free(p->current_namespace);
        p->current_namespace = token_str(&p->cur);
        next(p);
        return;
    }
    if (token_is(&p->cur, "endmodule")) {
        if (!p->current_namespace) die("#endmodule without active module");
        free(p->current_namespace);
        p->current_namespace = NULL;
        next(p);
        return;
    }
    if (token_is(&p->cur, "import")) {
        next(p);
        if (p->cur.kind != TK_IDENT && p->cur.kind != TK_STRING && p->cur.kind != TK_PATH) {
            die("expected path after #import");
        }
        char* import_token = token_str(&p->cur);
        char* resolved = resolve_import_path(path, import_token);
        free(import_token);
        next(p);
        compile_path(resolved, O, ctx, imports, false);
        free(resolved);
        return;
    }
    if (token_is(&p->cur, "uns")) {
        next(p);
        if (p->cur.kind != TK_IDENT) die("expected namespace after #uns");
        char* ns = token_str(&p->cur);
        add_using_namespace(p, ns);
        free(ns);
        next(p);
        return;
    }
    die("unknown #directive");
}

static void compile_path(const char* path, Out* O, CompileContext* ctx, ImportSet* imports, bool emit_header) {
    if (import_seen(imports, path)) return;
    add_import(imports, path);

    size_t len = 0;
    char* src = read_file_all(path, &len);
    Lexer L;
    lexer_init(&L, src, len);

    Parser p = {0};
    p.L = &L;
    p.O = O;
    p.current_namespace = NULL;
    p.using_namespaces = NULL;
    p.using_count = 0;
    p.using_cap = 0;
    p.func_table = &ctx->funcs;
    p.global_symbols = &ctx->globals.symbols;
    p.macro_table = &ctx->macros;
    p.globals = &ctx->globals;
    p.current_section = SEC_NONE;

    next(&p);

    if (emit_header) {
        outln(O, "default rel");
        outln(O, "section .text");
    }

    while (p.cur.kind != TK_EOF) {
        if (p.cur.kind == TK_NL) {
            next(&p);
            continue;
        }

        if (p.cur.kind == TK_HASH) {
            next(&p);
            handle_directive(&p, path, O, ctx, imports);
            continue;
        }

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

        if (p.cur.kind == TK_IDENT && token_is(&p.cur, "let")) {
            if (p.current_section != SEC_DATA && p.current_section != SEC_BSS
                && p.current_section != SEC_RODATA) {
                die("let statements must be in data/bss/readonly sections");
            }
            parse_global_let(&p);
            continue;
        }

        if (p.cur.kind == TK_IDENT && token_is(&p.cur, "def")) {
            if (p.current_section != SEC_MACROS) {
                die("macro definitions must be in macros section");
            }
            parse_macro_definition(&p);
            continue;
        }

        if (p.cur.kind == TK_AT) {
            char* block = parse_inline_block(&p);
            emit_raw_block(O, block);
            free(block);
            continue;
        }

        die("unexpected top-level token");
    }

    for (size_t i = 0; i < p.using_count; i++) free(p.using_namespaces[i]);
    free(p.using_namespaces);
    free(p.current_namespace);
    free(src);
}

void translate(const char* in_path, const char* out_path) {
    CompileContext ctx = {0};
    scan_file_for_symbols(&ctx, in_path);

    FILE* out = fopen(out_path, "wb");
    if (!out) die("cannot open output file");
    Out O = {out};

    ImportSet imports = {0};
    compile_path(in_path, &O, &ctx, &imports, true);

    for (size_t i = 0; i < imports.count; i++) free(imports.paths[i]);
    free(imports.paths);
    fclose(out);

    for (size_t i = 0; i < ctx.scanned.count; i++) free(ctx.scanned.paths[i]);
    free(ctx.scanned.paths);
    free_symbol_table(&ctx.funcs);
    free_global_table(&ctx.globals);
    free_macro_table(&ctx.macros);
}
