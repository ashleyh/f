#ifndef F_H_3F2NI1S9
#define F_H_3F2NI1S9

#include <stdio.h>
#include "sl.h"

typedef struct f_s f_t;
typedef bool (*filter_t)(f_t*, const char*);

struct f_s {
  bool prune_hidden_dirs;
  bool case_sensitive;
  filter_t filter;
  union {
    const char* pattern;
  } filter_arg;
  char* root;
  sl_t buf;
  struct {
    unsigned int tasks_pending;
    unsigned int ticks;
    unsigned int lstats;
    unsigned int readdirs;
    unsigned int errors;
  } stats;
  bool is_interactive;
};

// XXX: move
static inline void check_oom(bool val) {
  if (!val) {
    fputs("out of memory", stderr);
    exit(1);
  }
}

#endif /* end of include guard: F_H_3F2NI1S9 */
