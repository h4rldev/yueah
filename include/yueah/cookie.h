#ifndef YUEAH_COOKIE_H
#define YUEAH_COOKIE_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/shared.h>

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
 * [cookie_name]  Name of the cookie
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
unsigned char *yueah_cookie_new(h2o_mem_pool_t *pool, const char *cookie_name,
                                const char *content, mem_t *out_len,
                                yueah_cookie_mask mask, ...);

char *yueah_get_cookie_name(h2o_mem_pool_t *pool, h2o_iovec_t cookie_header);

// will get one cookie content at a time, will work on the same header but with
// different cookie names
unsigned char *yueah_get_cookie_content(h2o_mem_pool_t *pool,
                                        unsigned char *cookie_header,
                                        char *cookie_name, mem_t *out_len);

#endif // !YUEAH_COOKIE_H
