#ifndef SL_H_WBLWNQJT
#define SL_H_WBLWNQJT

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct sl_s {
  size_t length, capacity;
  char* buf;
};

typedef struct sl_s* sl_t;

static inline sl_t sl_alloc_f(size_t capacity) {
  sl_t sl = (sl_t)malloc(sizeof(struct sl_s));
  if (sl == NULL) {
    return sl;
  }
  sl->length = 0;
  sl->capacity = capacity;
  sl->buf = malloc(capacity);
  return sl;
}

static inline void sl_free(sl_t sl) {
  free(sl->buf);
  free(sl);
}

static inline bool sl_ensure_capacity_f(sl_t sl, size_t new_size) {
  size_t cap = sl->capacity;
  if (new_size <= cap) {
    return true;
  }
  if (cap == 0) {
    cap = 1;
  }
  while (new_size > cap) {
    cap *= 2;
  }
  void* new_buf = realloc(sl->buf, cap);
  if (new_buf == NULL) {
    return false;
  }
  sl->capacity = cap;
  sl->buf = new_buf;
  return true;
}

static inline void sl_set_length(sl_t sl, size_t new_length) {
  assert(new_length <= sl->capacity);
  sl->length = new_length;
}

static inline void sl_overwrite(sl_t sl, size_t start, const char* src, size_t src_len) {
  assert(start <= sl->length);
  assert(start + src_len <= sl->capacity);
  memcpy(sl->buf + start, src, src_len);
  sl->length = start + src_len;
}

static inline bool sl_overwrite_f(sl_t sl, size_t start, const char* src, size_t src_len) {
  if (!sl_ensure_capacity_f(sl, start + src_len)) {
    return false;
  }
  sl_overwrite(sl, start, src, src_len);
  return true;
}

static inline char sl_peek(sl_t sl, size_t index) {
  assert(index < sl->length);
  return sl->buf[index];
}

static inline void sl_poke(sl_t sl, size_t index, char c) {
  assert(index < sl->length);
  sl->buf[index] = c;
}

static inline bool sl_append_f(sl_t sl, const char* src, size_t src_len) {
  if (!sl_ensure_capacity_f(sl, sl->length + src_len)) {
    return false;
  }
  sl_overwrite(sl, sl->length, src, src_len);
  return true;
}

static inline bool sl_null_terminate_f(sl_t sl) {
  size_t old_length = sl->length;
  if (!sl_ensure_capacity_f(sl, old_length + 1)) {
    return false;
  }
  sl_set_length(sl, old_length + 1);
  sl_poke(sl, old_length, 0);
  return true;
}

static inline size_t sl_get_length(sl_t sl) {
  return sl->length;
}

static inline char* sl_get_buf(sl_t sl) {
  return sl->buf;
}

#endif /* end of include guard: SL_H_WBLWNQJT */
