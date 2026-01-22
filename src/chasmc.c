#include <stdbool.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "assembler.h"
#include "util.h"

static int run_process(const char* cmd, char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) die("failed to fork");
    if (pid == 0) {
        execvp(cmd, argv);
        perror(cmd);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die("failed to wait for process");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
}

static char* strip_extension(const char* path) {
    const char* slash = strrchr(path, '/');
    const char* dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash)) return xstrdup(path);
    size_t len = (size_t)(dot - path);
    char* out = (char*)malloc(len + 1);
    if (!out) die("Out of Memory");
    memcpy(out, path, len);
    out[len] = 0;
    return out;
}

static char* append_ext(const char* base, const char* ext) {
    size_t blen = strlen(base);
    size_t elen = strlen(ext);
    char* out = (char*)malloc(blen + elen + 1);
    if (!out) die("Out of Memory");
    memcpy(out, base, blen);
    memcpy(out + blen, ext, elen);
    out[blen + elen] = 0;
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: chasmc <input.chasm> -o <output> [-A: expose asm | -O: expose object | -p: expose both]\n");
        return 1;
    }

    const char* in_path = argv[1];
    const char* out_path = "a.asm";
    bool keep_asm = false;
    bool keep_obj = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[i + 1];
            i++;
            continue;
        }
        if (strcmp(argv[i], "-A") == 0) {
            keep_asm = true;
            continue;
        }
        if (strcmp(argv[i], "-O") == 0) {
            keep_obj = true;
            continue;
        }
        if (strcmp(argv[i], "-p") == 0) {
            keep_asm = true;
            keep_obj = true;
            continue;
        }
    }
    
    char* base = strip_extension(out_path);
    char* asm_path = append_ext(base, ".asm");
    char* obj_path = append_ext(base, ".o");

    translate(in_path, out_path);

    char* nasm_argv[] = {"nasm", "-f", "elf64", "-o", obj_path, asm_path, NULL};
    if (run_process("nasm", nasm_argv) != 0) die("nasm failed");

    char* ld_argv[] = {"ld", "-o", (char*)out_path, obj_path, NULL};
    if (run_process("ld", ld_argv) != 0) die("ld failed");

    if (!keep_asm) remove(asm_path);
    if (!keep_obj) remove(obj_path);

    printf("wrote %s\n", out_path);

    free(base);
    free(asm_path);
    free(obj_path);

    return 0;
}
