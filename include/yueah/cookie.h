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

typedef struct {
  char *name;
  char *content; // Encrypted by default
  char *domain;  // is NULL if not set
  yueah_same_site_t same_site;
  bool http_only : 1;
  bool secure : 1;
  mem_t max_age; // is 0 if not set
  mem_t expires; // is 0 if not set
} yueah_cookie_t;

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
char *yueah_set_cookie_new(h2o_mem_pool_t *pool, const char *cookie_name,
                           char **contents, yueah_cookie_mask mask, ...);

// will get one cookie content at a time, will work on the same header but with
// different cookie names
yueah_cookie_t *yueah_get_cookie(h2o_mem_pool_t *pool, char *cookie_header,
                                 char *cookie_name, mem_t type);

#endif // !YUEAH_COOKIE_H
