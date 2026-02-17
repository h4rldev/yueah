#ifndef YUEAH_JWT_H
#define YUEAH_JWT_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/shared.h>

typedef struct {
  char *alg;
  char *typ;
} yueah_jwt_header_t;

typedef struct {
  const char *iss;
  const char *sub;
  const char *aud;
  mem_t iat;
  mem_t exp;
  const char *jti;
  mem_t nbf;
} yueah_jwt_claims_t;

typedef enum {
  Access,
  Refresh,
} yueah_jwt_key_type_t;

char *yueah_jwt_encode(h2o_mem_pool_t *pool, const char *payload,
                       yueah_jwt_key_type_t key_type, mem_t *out_len);
bool yueah_jwt_verify(h2o_mem_pool_t *pool, const char *token, mem_t token_len,
                      const char *aud, yueah_jwt_key_type_t key_type);

#endif // !YUEAH_JWT_H
