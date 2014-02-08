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
  const char* path = req->path;
  if (req->result < 0) {
    fprintf(stderr, "error: %s: %s\n", path, uv_strerror(req->result));
  } else {
    uv_stat_t* buf = (uv_stat_t*)req->ptr;
    f_t* f = (f_t*)req->loop->data;
    if (should_walk(f, path, buf)) {
      // reusing req
      uv_fs_readdir(req->loop, req, req->path, O_RDONLY, readdir_cb);
    } else {
      free(req);
    }
    if (f->filter == NULL || f->filter(f, path, buf)) {
      puts(path);
    }
  }
  free((void*)path);
}

void readdir_cb(uv_fs_t* req) {
  const char* root = req->path;
  ssize_t file_count = req->result;

  if (file_count < 0) {
    fprintf(stderr, "error: %s: %s\n", root, uv_strerror(file_count));
  } else {
    size_t root_len = strlen(root);
    assert(root[root_len - 1] != '/');
    char* child = req->ptr;
    while (file_count-- > 0) {
      size_t child_len = strlen(child);
      size_t next_root_size = root_len + child_len + 2;
      char* next_root = malloc(next_root_size);
      strlcpy(next_root, root, next_root_size);
      strlcat(next_root, "/", next_root_size);
      strlcat(next_root, child, next_root_size);
      uv_fs_t* next_req = malloc(sizeof(uv_fs_t));
      uv_fs_lstat(req->loop, next_req, next_root, lstat_cb);
      // uv strdups the path >->
      free(next_root);
      child += child_len + 1;
    }
  }

  free(req->ptr);
  free((void*)req->path);
  free(req);
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
  uv_loop_t* loop = uv_default_loop();
  loop->data = f;
  uv_fs_t* req = malloc(sizeof(uv_fs_t));
  uv_fs_readdir(loop, req, f->root, O_RDONLY, readdir_cb);
  int r = uv_run(loop, UV_RUN_DEFAULT);
  free(f->root);
  return r;
}

int main(int argc, char** argv) {
  f_t f;
  read_opts(&f, argc, argv);
  return run(&f);
}
