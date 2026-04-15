#ifndef YUEAH_HASH_H
#define YUEAH_HASH_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Hash a password with argon2.
 *
 * @param pool Memory pool to allocate the string.
 * @param password The password to hash.
 *
 * @return The password hash.
 */
yueah_string_t *hash_password(h2o_mem_pool_t *pool,
                              const yueah_string_t *password);

/*
 * @brief Verify a password with a hash.
 *
 * @param pool Memory pool for conversion.
 * @param password The password to verify.
 * @param hash The hash to verify.
 *
 * @return true if the password matches the hash, false otherwise.
 */
bool verify_password(h2o_mem_pool_t *pool, const yueah_string_t *password,
                     const yueah_string_t *hash);

#endif // !YUEAH_HASH_H
