#include <h2o.h>
#include <sodium.h>

#include <yueah/error.h>
#include <yueah/hash.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *hash_password(h2o_mem_pool_t *pool,
                              const yueah_string_t *password,
                              yueah_error_t *error) {
  cstr *hash = h2o_mem_alloc_pool(pool, cstr, crypto_pwhash_STRBYTES);
  if (crypto_pwhash_str(hash, (cstr *)password->data, password->len,
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    *error = yueah_throw_error(
        "Failed to hash password, did you run out of memory?");
    return NULL;
  }

  return yueah_string_new(pool, hash, crypto_pwhash_STRBYTES);
}

bool verify_password(h2o_mem_pool_t *pool, const yueah_string_t *hash,
                     const yueah_string_t *password) {
  cstr *hash_cstr = yueah_string_to_cstr(pool, hash);
  if (crypto_pwhash_str_verify(hash_cstr, (cstr *)password->data,
                               password->len) == 0)
    return true;

  return false;
}
