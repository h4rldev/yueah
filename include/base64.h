#ifndef YUEAH_BASE64_H
#define YUEAH_BASE64_H

#include <shared.h>

#include <h2o.h>

char *yueah_base64_encode(h2o_mem_pool_t *pool, unsigned char *content,
                          mem_t content_len, mem_t *out_len);
unsigned char *yueah_base64_decode(h2o_mem_pool_t *pool, char *base64,
                                   mem_t base64_len, mem_t *out_len);

#endif // !YUEAH_BASE64_H
