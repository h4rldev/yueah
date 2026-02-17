#include <string.h>

#include <h2o.h>

#include <yueah/shared.h>

char *yueah_strdup(h2o_mem_pool_t *pool, const char *str, mem_t size) {
  char *dup = h2o_mem_alloc_pool(pool, char *, size);
  memcpy(dup, str, size);
  return dup;
}

void print_hex(const char *label, const unsigned char *str) {
  printf("%s: ", label);
  while (*str) {
    printf("%02x ", *str);
    str++;
  }
  printf("\n");
}
