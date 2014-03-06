#ifndef FILTER_H_UDBEVYWL
#define FILTER_H_UDBEVYWL

#include <stdbool.h>
#include "f.h"

bool filter_strstr_path(f_t* f, const char* path);
bool filter_strstr_basename(f_t* f, const char* path);
bool should_walk(f_t* f, const char* path);


#endif /* end of include guard: FILTER_H_UDBEVYWL */

