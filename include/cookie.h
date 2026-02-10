#ifndef YUEAH_COOKIE_H
#define YUEAH_COOKIE_H

#include <h2o.h>

typedef enum {
  SAME_SITE = 1 << 0,
  HTTP_ONLY = 1 << 1,
  SECURE = 1 << 2,
  PATH = 1 << 3,
  EXPIRES = 1 << 4,
  MAX_AGE = 1 << 5,
  DOMAIN = 1 << 6,
} yueah_cookie_bitmask_t;

typedef enum { INVALID = -1, LAX, STRICT, NONE } yueah_same_site_t;

typedef int yueah_cookie_mask;

/*
 * Construct a Set-Cookie header with given arguments
 *
 * [pool]  Memory pool to allocate from
 *
 * [contents]  Array of strings to be set and encrypted for cookie
 *
 * [mask]  Bitmask to set cookie attributes, see
 * [yueah_cookie_bitmask_t]
 *
 * [...]  Additional arguments for bitmask, e.g.
 * yueah_cookie_new(pool, contents, SAME_SITE, LAX)
 *
 *
 * Returns a yueah_cookie_t struct with the proper header.
 */
char *yueah_cookie_new(h2o_mem_pool_t *pool, const char *cookie_name,
                       char **contents, yueah_cookie_mask mask, ...);

#endif // !YUEAH_COOKIE_H
