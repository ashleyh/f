#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "shame.h"
#include "sl.h"
#include <dirent.h>

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
  uv_timer_t timer;
  bool is_interactive;
};

struct f_scandir_response_s {
  char* path;
  sl_t dents;
};

typedef struct f_scandir_response_s* f_scandir_response_t;

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

bool is_hidden(const char* path) {
  return basename(path)[0] == '.';
}

bool should_walk(f_t* f, const char* path) {
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

static inline bool is_dots(const char* name) {
  if (name[0] != '.') return false;
  if (name[1] == 0) return true;
  if (name[1] != '.') return false;
  if (name[2] == 0) return true;
  return false;
}

static inline bool readdir_wrapper(DIR* dir, struct dirent** dent, int* err) {
  errno = 0;
  *dent = readdir(dir);
  if (*dent == NULL) {
    *err = errno;
    return false;
  } else {
    *err = 0;
    return true;
  }
}

static inline bool f_scandir_impl(const char* path, f_scandir_response_t response) {
  DIR* dir = opendir(path);
  if (dir == NULL) {
    return false;
  }
  struct dirent* dent = NULL;
  int err = 0;
  while (readdir_wrapper(dir, &dent, &err)) {
    if (!is_dots(dent->d_name)) {
      check_oom(sl_append_f(response->dents, (const char*)dent, dent->d_reclen));
    }
  }
  bool ok = (err == 0);
  closedir(dir);
  return ok;
}

void f_scandir_work(uv_work_t* req) {
  char* path = req->data;
  f_scandir_response_t response = malloc(sizeof(struct f_scandir_response_s));
  check_oom(response);
  response->dents = sl_alloc_f(1);
  check_oom(response->dents);
  response->path = path;
  if (!f_scandir_impl(path, response)) {
    if (response->dents != NULL) {
      sl_free(response->dents);
    }
    response->dents = NULL;
  }
  req->data = response;
}

static inline void set_to_path(sl_t sl, const char* path) {
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

void f_scandir(uv_loop_t* loop, char* root);

static inline void visit(f_t* f) {
  if (f->filter == NULL || f->filter(f, f->buf->buf)) {
    clear_status();
    puts(f->buf->buf);
  }
}

void f_scandir_cb(uv_work_t* req, int status) {
  f_t* f = (f_t*)req->loop->data;
  f_scandir_response_t response = req->data;
  f->stats.tasks_pending--;
  if (response->dents == NULL) {
    f->stats.errors++;
  } else {
    set_to_path(f->buf, response->path);
    size_t root_len = sl_get_length(f->buf);
    size_t off = 0;
    while (off < response->dents->length) {
      struct dirent* dent = (struct dirent*)(response->dents->buf + off);
      sl_set_length(f->buf, root_len);
      check_oom(sl_append_f(f->buf, dent->d_name, get_namlen(dent)));
      check_oom(sl_null_terminate_f(f->buf));
      visit(f);
      switch (dent->d_type) {
        case DT_UNKNOWN:
        case DT_DIR:
        case DT_LNK:
          if (should_walk(f, f->buf->buf)) {
            f_scandir(req->loop, strdup(f->buf->buf));
          }
      }
      off += dent->d_reclen;
    }
    sl_free(response->dents);
  }
  (void)status;
  stop_timer_if_done(f);
  free(req);
  free(response->path);
  free(response);
}

void f_scandir(uv_loop_t* loop, char* root) {
  f_t* f = (f_t*)loop->data;
  uv_work_t* req = (uv_work_t*)malloc(sizeof(uv_work_t));
  req->data = root;
  f->stats.tasks_pending++;
  uv_queue_work(loop, req, f_scandir_work, f_scandir_cb);
}

void read_opts(f_t* f, int argc, char** argv) {
  int opt;
  f->root = strdup(".");
  f->filter = filter_strstr_basename;
  f->case_sensitive = false;
  f->prune_hidden_dirs = true;
  while ((opt = getopt(argc, argv, "acpd:")) != -1) {
    switch (opt) {
      case 'a':
        f->prune_hidden_dirs = false;
        break;
      case 'c':
        f->case_sensitive = true;
        break;
      case 'd':
        free(f->root);
        f->root = strdup(optarg);
        break;
      case 'p':
        f->filter = filter_strstr_path;
        break;
      default:
        fprintf(stderr,
            "Usage: %s [-a] [-c] [-d dir] [-p] [pattern]\n"
            "Options:\n"
            "  -a      don't prune hidden dirs\n"
            "  -c      respect case\n"
            "  -d dir  root in dir not .\n"
            "  -p      match in whole path\n"
            , argv[0]
            );
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

  f_scandir(loop, f->root);

  int r = uv_run(loop, UV_RUN_DEFAULT);

  clear_status();
  if (f->stats.errors == 1) {
    fprintf(stderr, "there was 1 error\n");
  } else if (f->stats.errors > 1) {
    fprintf(stderr, "there were %d errors\n", f->stats.errors);
  }

  sl_free(f->buf);

  return r;
}

int main(int argc, char** argv) {
  f_t f;
  memset(&f, 0, sizeof(f_t));
  read_opts(&f, argc, argv);
  return run(&f);
}
