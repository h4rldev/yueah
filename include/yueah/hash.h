#ifndef YUEAH_HASH_H
#define YUEAH_HASH_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/shared.h>

char *hash_password(h2o_mem_pool_t *pool, const char *password,
                    mem_t password_len);
bool verify_password(const char *hash, const char *password,
                     mem_t password_len);

#endif // !YUEAH_HASH_H
