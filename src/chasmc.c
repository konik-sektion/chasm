#include <stdio.h>
#include <string.h>

#include "assembler.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: chasmc <input.chasm> -o <output.asm>\n");
        return 1;
    }

    const char* in_path  = argv[1];
    const char* out_path = "out.asm";

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[i + 1];
            i++;
        }
    }

    translate(in_path, out_path);
    printf("wrote %s\n", out_path);
    return 0;
}
