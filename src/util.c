#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char* msg) {
    fprintf(stderr, "chasmc error: %s\n", msg);
    exit(1);
}

char* read_file_all(const char* path, size_t* out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open input file");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) die ("ftell failed");
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) die("Fatal: Out of Memory");
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    if (out_len) *out_len = got;
    return buf;
}

char* xstrdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* out = (char *)malloc(len + 1);
    if (!out) die("Fatal: Out of Memory");
    memcpy(out, src, len);
    out[len] = 0;
    return out;
}
