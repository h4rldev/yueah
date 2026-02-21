
#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/log.h>
#include <yueah/shared.h>

char *yueah_base64_encode(h2o_mem_pool_t *pool, unsigned char *content,
                          mem_t content_len, mem_t *out_len) {

  size_t base64_len = sodium_base64_encoded_len(
      content_len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

  char *base64 = h2o_mem_alloc_pool(pool, char *, base64_len);
  char *result = sodium_bin2base64(base64, base64_len, content, content_len,
                                   sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

  if (result != base64) {
    yueah_log_error("sodium_bin2base64 failed");
    return NULL;
  }

  *out_len = base64_len - 1;
  return base64;
}

unsigned char *yueah_base64_decode(h2o_mem_pool_t *pool, char *base64,
                                   mem_t base64_len, mem_t max_out_len,
                                   mem_t *out_len) {
  mem_t buf_len = 0;
  mem_t max_len = max_out_len;
  char last[64];

  int rc = 0;
  unsigned char *buf = h2o_mem_alloc_pool(pool, char *, buf_len);

  rc = sodium_base642bin(buf, max_len, base64, base64_len, NULL, &buf_len,
                         (const char **)&last,
                         sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  if (rc != 0) {
    yueah_log_error("sodium_base642bin failed: code %d, last: %s", rc, last);
    return NULL;
  }

  *out_len = buf_len;
  return buf;
}
