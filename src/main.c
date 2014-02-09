#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "shame.h"

typedef struct f_s f_t;
typedef bool (*filter_t)(f_t*, const char*, uv_stat_t*);

struct f_s {
  bool prune_hidden_dirs;
  filter_t filter;
  union {
    const char* pattern;
  } filter_arg;
  char* root;
  char* buf;
  size_t buf_size;
};

void readdir_cb(uv_fs_t*);

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

void lstat_cb(uv_fs_t* req) {
  bool reused_req = false;
  const char* path = req->path;
  if (req->result < 0) {
    fprintf(stderr, "error: %s: %s\n", path, uv_strerror(req->result));
  } else {
    uv_stat_t* buf = (uv_stat_t*)req->ptr;
    f_t* f = (f_t*)req->loop->data;
    if (should_walk(f, path, buf)) {
      reused_req = true;
      uv_fs_readdir(req->loop, req, req->path, O_RDONLY, readdir_cb);
    }
    if (f->filter == NULL || f->filter(f, path, buf)) {
      puts(path);
    }
  }
  free((void*)path);
  if (!reused_req) {
    free(req);
  }
}

// true iff resized
static inline bool resize_buf(f_t* f, size_t required_size) {
  if (required_size > f->buf_size) {
    while (required_size > f->buf_size) f->buf_size *= 2;
    f->buf = realloc(f->buf, f->buf_size);
    return true;
  } else {
    return false;
  }
}

void readdir_cb(uv_fs_t* req) {
  bool reused_req = false;
  f_t* f = (f_t*)req->loop->data;
  const char* root = req->path;
  ssize_t file_count = req->result;
  void* req_ptr = req->ptr;

  if (file_count < 0) {
    fprintf(stderr, "error: %s: %s\n", root, uv_strerror(file_count));
  } else {
    size_t root_len = strlen(root);
    if (root[root_len - 1] != '/') {
      root_len++;
    }
    resize_buf(f, root_len + 1);
    stpcpy(f->buf, root);
    char* buf_end = f->buf + root_len;
    buf_end[-1] = '/';
    buf_end[0] = 0;

    char* child = req_ptr;
    while (file_count-- > 0) {
      size_t child_len = strlen(child);
      size_t required_size = root_len + child_len + 1;
      if (resize_buf(f, required_size)) {
        buf_end = f->buf + root_len;
        assert(buf_end[-1] == '/');
      }
      strlcpy(buf_end, child, child_len + 1);
      uv_fs_t* next_req = NULL;
      if (reused_req) {
        next_req = malloc(sizeof(uv_fs_t));
      } else {
        next_req = req;
        reused_req = true;
      }
      uv_fs_lstat(req->loop, next_req, f->buf, lstat_cb);
      child += child_len + 1;
    }
  }

  free((void*)root);
  free(req_ptr);
  if (!reused_req) {
    free(req);
  }
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

int run(f_t* f) {
  f->buf_size = 1;
  f->buf = malloc(f->buf_size);
  uv_loop_t* loop = uv_default_loop();
  loop->data = f;
  uv_fs_t* req = malloc(sizeof(uv_fs_t));
  uv_fs_readdir(loop, req, f->root, O_RDONLY, readdir_cb);
  int r = uv_run(loop, UV_RUN_DEFAULT);
  free(f->root);
  free(f->buf);
  return r;
}

int main(int argc, char** argv) {
  f_t f;
  read_opts(&f, argc, argv);
  return run(&f);
}
