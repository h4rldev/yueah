#ifndef YUEAH_SHARED_H
#define YUEAH_SHARED_H

#include <h2o.h>

typedef uint64_t mem_t;

typedef struct {
  char *db_path;
} yueah_state_t;

typedef struct {
  h2o_handler_t super;
  yueah_state_t *state;
} yueah_handler_t;

#define KiB(num) ((mem_t)(num) << 10)
#define MiB(num) ((mem_t)(num) << 20)
#define GiB(num) ((mem_t)(num) << 30)

char *yueah_strdup(h2o_mem_pool_t *pool, const char *str, mem_t size);
void print_hex_unsigned(const char *label, const unsigned char *str,
                        size_t size); // to not depend on null terminator
void print_hex(const char *label, const char *str,
               size_t size); // to not depend on null terminator

#endif // !YUEAH_SHARED_H
