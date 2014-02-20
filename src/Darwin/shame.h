#ifndef SHAME_H_PD85IYSU
#define SHAME_H_PD85IYSU

#include <dirent.h>
static inline size_t get_namlen(struct dirent* dent) {
  return dent->d_namlen;
}

#endif /* end of include guard: SHAME_H_PD85IYSU */

