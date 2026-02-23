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

/*
 * Construct a jwt payload
 *
 * [pool]  Memory pool to allocate from
 *
 * [iss]  Issuer
 *
 * [sub]  Subject (user/user id)
 *
 * [aud]  Audience (client id or something like that)
 *
 * [exp]  Expiration time (in 64 bit unix time)
 *
 * [nbf]  Not before time (in 64 bit unix time)
 *
 *
 * Returns a pointer to the assembled payload
 */

yueah_jwt_claims_t *yueah_jwt_create_claims(h2o_mem_pool_t *pool,
                                            const char *iss, const char *sub,
                                            const char *aud, const mem_t exp,
                                            const mem_t nbf);

/*
 * Encode a jwt token
 *
 * [pool]  Memory pool to allocate from
 *
 * [payload]  Payload to encode
 *
 * [key_type]  Type of key to use (Access or Refresh)
 *
 * [*out_len]  Length of the encoded token
 *
 *
 * Returns a pointer to the encoded token
 */
char *yueah_jwt_encode(h2o_mem_pool_t *pool, const yueah_jwt_claims_t *payload,
                       yueah_jwt_key_type_t key_type, mem_t *out_len);

/*
 * Verify a jwt token's signature
 *
 * [pool]  Memory pool to allocate from
 *
 * [token]  Token to verify
 *
 * [token_len]  Length of the token
 *
 * [aud]  Audience to verify
 *
 * [key_type]  Type of key to use (Access or Refresh)
 *
 *
 * Returns true if the token is valid, false otherwise
 */

bool yueah_jwt_verify(h2o_mem_pool_t *pool, const char *token, mem_t token_len,
                      const char *aud, yueah_jwt_key_type_t key_type);

#endif // !YUEAH_JWT_H
