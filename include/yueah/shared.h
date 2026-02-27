#ifndef YUEAH_SHARED_H
#define YUEAH_SHARED_H

#include <stdbool.h>
#include <stdint.h>

#include <h2o.h>

typedef uint64_t mem_t;

#define KiB(num) ((mem_t)(num) << 10)
#define MiB(num) ((mem_t)(num) << 20)
#define GiB(num) ((mem_t)(num) << 30)

typedef struct {
  const char *allow_origin;
  const char *allow_methods;
  const char *allow_headers;
  const char *expose_headers;

  bool allow_credentials : 1;
  mem_t max_age;
} yueah_cors_config_t;

typedef struct {
  yueah_cors_config_t *public;
  yueah_cors_config_t *private;
} yueah_cors_configs_t;

typedef struct {
  char *db_path;
  yueah_cors_configs_t *cors;
} yueah_state_t;

typedef struct {
  h2o_handler_t super;
  yueah_state_t *state;
} yueah_handler_t;

char *yueah_strdup(h2o_mem_pool_t *pool, const char *str, mem_t size);
void print_hex_unsigned(const char *label, const unsigned char *str,
                        size_t size); // to not depend on null terminator
void print_hex(const char *label, const char *str,
               size_t size); // to not depend on null terminator

char *yueah_iovec_to_str(h2o_mem_pool_t *pool, h2o_iovec_t *iovec);

#endif // !YUEAH_SHARED_H
