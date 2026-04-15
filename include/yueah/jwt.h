#ifndef YUEAH_JWT_H
#define YUEAH_JWT_H

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Construct a jwt payload
 *
 * @param pool Memory pool to allocate from
 * @param iss Token issuer
 * @param sub Subject (user/user id)
 * @param aud Audience (client id or something like that)
 * @param exp Expiration time (in 64 bit unix time)
 * @param nbf Not before time (in 64 bit unix time)
 *
 * @return A pointer to the assembled payload
 */

yueah_jwt_claims_t *yueah_jwt_create_claims(h2o_mem_pool_t *pool,
                                            const yueah_string_t *iss,
                                            const yueah_string_t *sub,
                                            const yueah_string_t *aud,
                                            const u64 exp, const u64 nbf);

/*
 * @brief Encode a jwt token
 *
 * @param pool Memory pool to allocate from
 * @param payload Payload to encode
 * @param key_type Type of key to use (Access or Refresh)
 *
 * @return a pointer to the encoded token as a string.
 */
yueah_string_t *yueah_jwt_encode(h2o_mem_pool_t *pool,
                                 const yueah_jwt_claims_t *payload,
                                 yueah_jwt_key_type_t key_type);

/*
 * @brief Verify a jwt token's signature
 *
 * @param pool Memory pool to allocate from
 * @param token Token to verify
 * @param aud Audience to verify
 * @param key_type Type of key to use (Access or Refresh)
 *
 * @return true if the token is valid, false otherwise
 */

bool yueah_jwt_verify(h2o_mem_pool_t *pool, const yueah_string_t *token,
                      const yueah_string_t *aud, yueah_jwt_key_type_t key_type);

/*
 * @brief Get the token's subject
 *
 * @param pool Memory pool to allocate from
 * @param token Token to get the subject from
 */
char *yueah_jwt_get_sub(h2o_mem_pool_t *pool, const yueah_string_t *token);

#endif // !YUEAH_JWT_H
