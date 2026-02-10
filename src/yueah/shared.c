#include <string.h>

#include <h2o.h>

#include <shared.h>

char *yueah_strdup(h2o_mem_pool_t *pool, const char *str, mem_t size) {
  char *dup = h2o_mem_alloc_pool(pool, char *, size);
  memcpy(dup, str, size);
  return dup;
}
