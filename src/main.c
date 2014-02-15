#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "shame.h"
#include "sl.h"

typedef struct f_s f_t;
typedef bool (*filter_t)(f_t*, const char*, uv_stat_t*);

struct f_s {
  bool prune_hidden_dirs;
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
  uv_timer_t timer;
  bool is_interactive;
};

void readdir_cb(uv_fs_t*);

static inline void check_oom(bool val) {
  if (!val) {
    fputs("out of memory", stderr);
    exit(1);
  }
}

const char* basename(const char* path) {
  const char* basename = strrchr(path, '/');
  if (basename == NULL) {
    basename = path;
  } else {
    basename++;
  }
  return basename;
}

bool filter_strstr_path(f_t* f, const char* path, uv_stat_t* buf) {
  (void)buf;
  return strstr(path, f->filter_arg.pattern);
}

bool filter_strstr_basename(f_t* f, const char* path, uv_stat_t* buf) {
  (void)buf;
  return strstr(basename(path), f->filter_arg.pattern);
}

bool is_hidden(const char* path) {
  return basename(path)[0] == '.';
}

bool should_walk(f_t* f, const char* path, uv_stat_t* buf) {
  if (!S_ISDIR(buf->st_mode)) {
    return false;
  }

  if (f->prune_hidden_dirs && is_hidden(path)) {
    return false;
  }

  return true;
}

static inline void clear_status() {
  fprintf(stderr, "\r"); // XXX not good
}

static inline void stop_timer_if_done(f_t* f) {
  if (f->stats.tasks_pending == 0) {
    uv_timer_stop(&f->timer);
  }
}

void lstat_cb(uv_fs_t* req) {
  bool reused_req = false;
  f_t* f = (f_t*)req->loop->data;
  const char* path = req->path;

  f->stats.tasks_pending--;
  f->stats.lstats++;

  if (req->result < 0) {
    f->stats.errors++;
  } else {
    uv_stat_t* buf = (uv_stat_t*)req->ptr;
    if (should_walk(f, path, buf)) {
      reused_req = true;
      f->stats.tasks_pending++;
      uv_fs_readdir(req->loop, req, req->path, O_RDONLY, readdir_cb);
    }
    if (f->filter == NULL || f->filter(f, path, buf)) {
      clear_status();
      puts(path);
    }
  }
  free((void*)path);
  if (!reused_req) {
    free(req);
  }

  stop_timer_if_done(f);
}

void set_to_path(sl_t sl, const char* path) {
  size_t path_len = strlen(path);
  check_oom(sl_ensure_capacity_f(sl, path_len + 1));
  sl_overwrite(sl, 0, path, path_len);
  if (sl_peek(sl, path_len - 1) != '/') {
    sl_set_length(sl, path_len + 1);
    sl_poke(sl, path_len, '/');
  } else {
    sl_set_length(sl, path_len);
  }
}

void readdir_cb(uv_fs_t* req) {
  bool reused_req = false;
  f_t* f = (f_t*)req->loop->data;
  const char* root = req->path;
  ssize_t file_count = req->result;
  void* req_ptr = req->ptr;

  f->stats.tasks_pending--;
  f->stats.readdirs++;

  if (file_count < 0) {
    f->stats.errors++;
  } else {
    set_to_path(f->buf, root);
    size_t root_len = sl_get_length(f->buf);
    char* child = req_ptr;
    while (file_count-- > 0) {
      size_t child_len = strlen(child);
      sl_set_length(f->buf, root_len);
      check_oom(sl_append_f(f->buf, child, child_len));
      check_oom(sl_null_terminate_f(f->buf));
      uv_fs_t* next_req = NULL;
      if (reused_req) {
        next_req = malloc(sizeof(uv_fs_t));
      } else {
        next_req = req;
        reused_req = true;
      }
      f->stats.tasks_pending++;
      uv_fs_lstat(req->loop, next_req, sl_get_buf(f->buf), lstat_cb);
      child += child_len + 1;
    }
  }

  free((void*)root);
  free(req_ptr);
  if (!reused_req) {
    free(req);
  }

  stop_timer_if_done(f);
}

void read_opts(f_t* f, int argc, char** argv) {
  int opt;
  f->root = strdup(".");
  f->filter = filter_strstr_basename;
  f->prune_hidden_dirs = true;
  while ((opt = getopt(argc, argv, "ad:")) != -1) {
    switch (opt) {
      case 'a':
        f->prune_hidden_dirs = false;
        break;
      case 'd':
        free(f->root);
        f->root = strdup(optarg);
        break;
      case 'p':
        f->filter = filter_strstr_path;
        break;
      default:
        fprintf(stderr, "Usage: %s [-d dir] [pattern]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  argc -= optind;
  argv += optind;
  if (argc >= 1) {
    if (argc > 1) {
      fprintf(stderr, "warning: trailing args ignored\n");
    }
    f->filter_arg.pattern = argv[0];
  } else {
    f->filter = NULL;
  }
}

void timer_cb(uv_timer_t* req, int status) {
  (void)status;
  f_t* f = (f_t*)req->loop->data;
  f->stats.ticks++;
  char* ticker="/-\\|";
  clear_status();
  fprintf(stderr,
      "%c lstats=%d readdirs=%d pending=%d errors=%d",
      ticker[f->stats.ticks%4],
      f->stats.lstats,
      f->stats.readdirs,
      f->stats.tasks_pending,
      f->stats.errors);
  fflush(stdout);
}

int run(f_t* f) {
  uv_loop_t* loop = uv_default_loop();
  loop->data = f;

  f->buf = sl_alloc_f(1);
  check_oom(f->buf);

  f->is_interactive = (uv_guess_handle(STDERR_FILENO) == UV_TTY);
  if (f->is_interactive) {
    uv_timer_init(loop, &f->timer);
    uv_timer_start(&f->timer, &timer_cb, 500, 500);
  }

  uv_fs_t* req = malloc(sizeof(uv_fs_t));
  f->stats.tasks_pending++;
  uv_fs_readdir(loop, req, f->root, O_RDONLY, readdir_cb);

  int r = uv_run(loop, UV_RUN_DEFAULT);

  clear_status();
  if (f->stats.errors == 1) {
    fprintf(stderr, "there was 1 error\n");
  } else if (f->stats.errors > 1) {
    fprintf(stderr, "there were %d errors\n", f->stats.errors);
  }

  free(f->root);
  sl_free(f->buf);

  return r;
}

int main(int argc, char** argv) {
  f_t f;
  memset(&f, 0, sizeof(f_t));
  read_opts(&f, argc, argv);
  return run(&f);
}
