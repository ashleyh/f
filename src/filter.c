#include "filter.h"

#include <string.h>
#include "shame.h"

static inline const char* basename(const char* path) {
  const char* basename = strrchr(path, '/');
  if (basename == NULL) {
    basename = path;
  } else {
    basename++;
  }
  return basename;
}

static inline bool strxstr(const char* haystack, const char* needle,
    bool case_sensitive) {
  return (case_sensitive? strstr: strcasestr)(haystack, needle);
}

bool filter_strstr_path(f_t* f, const char* path) {
  return strxstr(path, f->filter_arg.pattern, f->case_sensitive);
}

bool filter_strstr_basename(f_t* f, const char* path) {
  return strxstr(basename(path), f->filter_arg.pattern, f->case_sensitive);
}

static inline bool is_hidden(const char* path) {
  return basename(path)[0] == '.';
}

bool should_walk(f_t* f, const char* path) {
  if (f->prune_hidden_dirs && is_hidden(path)) {
    return false;
  }
  return true;
}
