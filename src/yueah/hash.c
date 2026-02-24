#include <h2o.h>
#include <sodium.h>

#include <yueah/hash.h>
#include <yueah/log.h>
#include <yueah/shared.h>

char *hash_password(h2o_mem_pool_t *pool, const char *password,
                    mem_t password_len) {
  char *hash = h2o_mem_alloc_pool(pool, char, crypto_pwhash_STRBYTES);
  if (crypto_pwhash_str(hash, password, password_len,
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    yueah_log_error("Failed to hash password, did memory run out?");
    return NULL;
  }

  return hash;
}

bool verify_password(const char *hash, const char *password,
                     mem_t password_len) {
  if (crypto_pwhash_str_verify(hash, password, password_len) == 0)
    return true;

  return false;
}
