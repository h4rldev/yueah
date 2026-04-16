#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *yueah_base64_encode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *content,
                                    yueah_error_t *error) {
  u64 base64_len = sodium_base64_encoded_len(
      content->len, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  cstr *base64 = h2o_mem_alloc_pool(pool, cstr, base64_len);

  cstr *result =
      sodium_bin2base64(base64, base64_len, content->data, content->len,
                        sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

  if (memcmp(result, base64, base64_len) != 0) {
    *error = yueah_throw_error("sodium_bin2base64 failed");
    return NULL;
  }

  yueah_string_t *base64_str = yueah_string_new(pool, base64, base64_len - 1);

  *error = yueah_success(NULL);
  return base64_str;
}

yueah_string_t *yueah_base64_decode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *base64,
                                    u64 max_out_len, yueah_error_t *error) {
  int rc = 0;
  u64 len = base64->len / 4 * 3;
  u64 real_len = 0;

  ucstr *bin_ucstr = h2o_mem_alloc_pool(pool, ucstr, len);
  rc = sodium_base642bin(bin_ucstr, len * 64, (char *)base64->data, base64->len,
                         NULL, &real_len, NULL,
                         sodium_base64_VARIANT_ORIGINAL_NO_PADDING);
  if (rc != 0) {
    *error = yueah_throw_error("sodium_base642bin failed: code %d", rc);
    return NULL;
  }

  yueah_string_t *bin_str = yueah_string_new(pool, (cstr *)bin_ucstr, real_len);
  *error = yueah_success(NULL);
  return bin_str;
}
