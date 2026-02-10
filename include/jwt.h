#ifndef YUEAH_JWT_H
#define YUEAH_JWT_H

#include <stdint.h>

#include <h2o.h>

typedef uint64_t mem_t;

typedef struct {
  char *alg;
  char *typ;
} yueah_jwt_header_t;

typedef struct {
  const char *iss;
  const char *sub;
  const char *aud;
  const mem_t iat;
  const mem_t exp;
  const char *jti;
  const mem_t nbf;
} yueah_jwt_claims_t;

char *yueah_jwt_encode(h2o_mem_pool_t *pool, const char *payload);
char *yueah_jwt_decode(h2o_mem_pool_t *pool, const char *token);

#endif // !YUEAH_JWT_H
