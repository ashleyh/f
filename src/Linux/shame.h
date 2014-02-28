#ifndef SHAME_H_JXGSANQL
#define SHAME_H_JXGSANQL

#include <dirent.h>

static inline size_t get_namlen(struct dirent* dent) {
  return strlen(dent->d_name);
}

extern char* strcasestr(const char* haystack, const char* needle);

#endif /* end of include guard: SHAME_H_JXGSANQL */
