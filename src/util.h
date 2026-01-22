#ifndef CHASMC_UTIL_H
#define CHASMC_UTIL_H

#include <stddef.h>

void die(const char* msg);
char *read_file_all(const char* path, size_t* out_len);
char *xstrdup(const char* src);

#endif
