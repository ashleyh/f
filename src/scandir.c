#include "scandir.h"

#include <stdbool.h>
#include <dirent.h>
#include "sl.h"
#include "f.h"
#include "filter.h"
#include "shame.h"

struct f_scandir_response_s {
  char* path;
  sl_t dents;
};

typedef struct f_scandir_response_s* f_scandir_response_t;

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

static void f_scandir_work(uv_work_t* req) {
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

static inline void visit(f_t* f) {
  if (f->filter == NULL || f->filter(f, f->buf->buf)) {
    puts(f->buf->buf);
  }
}

static void f_scandir_cb(uv_work_t* req, int status) {
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
