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
#include "f.h"
#include "scandir.h"
#include "filter.h"

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

int run(f_t* f) {
  uv_loop_t* loop = uv_default_loop();
  loop->data = f;

  f->buf = sl_alloc_f(1);
  check_oom(f->buf);

  f_scandir(loop, f->root);

  int r = uv_run(loop, UV_RUN_DEFAULT);

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
