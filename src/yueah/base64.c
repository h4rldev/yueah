#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *yueah_base64_encode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *content) {

  u64 base64_len = sodium_base64_encoded_len(
      content->len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

  yueah_string_t *base64 = yueah_string_new(pool, NULL, base64_len);
  char *result = sodium_bin2base64((char *)base64->data, base64_len * 64,
                                   content->data, content->len,
                                   sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

  if (result != (char *)base64->data) {
    yueah_log_error("sodium_bin2base64 failed");
    return NULL;
  }

  return base64;
}

yueah_string_t *yueah_base64_decode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *base64,
                                    u64 max_out_len) {
  int rc = 0;
  u64 len = base64->len / 4 * 3;
  u64 real_len = 0;

  yueah_string_t *buf = yueah_string_new(pool, NULL, len);

  if (sodium_base642bin(buf->data, len * 64, (char *)base64->data, base64->len,
                        NULL, &real_len, NULL,
                        sodium_base64_VARIANT_ORIGINAL_NO_PADDING) != 0) {
    yueah_log_error("sodium_base642bin failed: code %d", rc);
    return NULL;
  }

  buf->len = real_len;
  return buf;
}
