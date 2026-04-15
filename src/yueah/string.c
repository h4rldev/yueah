#include <ctype.h>

#include <h2o.h>

#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *yueah_string_new(h2o_mem_pool_t *pool, const cstr_nullable *str,
                                 u64 size) {
  yueah_string_t *new_str = h2o_mem_alloc_pool(pool, yueah_string_t, 1);
  new_str->data = h2o_mem_alloc_pool(pool, u8, size);
  new_str->len = size;

  if (str)
    memcpy(new_str->data, str, size);

  return new_str;
}

cstr *yueah_string_to_cstr(h2o_mem_pool_t *pool, const yueah_string_t *str) {
  cstr *new_str = h2o_mem_alloc_pool(pool, cstr, str->len + 1);

  memcpy(new_str, str->data, str->len);
  memset(new_str + str->len, 0, 1);

  return new_str;
}

yueah_string_t *yueah_string_from_iovec(h2o_mem_pool_t *pool,
                                        const h2o_iovec_t *iovec) {
  return yueah_string_new(pool, iovec->base, iovec->len);
}

h2o_iovec_t *yueah_string_to_iovec(h2o_mem_pool_t *pool,
                                   const yueah_string_t *str) {
  h2o_iovec_t *iovec = h2o_mem_alloc_pool(pool, h2o_iovec_t, 1);
  iovec->base = (char *)str->data;
  iovec->len = str->len;

  return iovec;
}

void yueah_string_lower(yueah_string_t *str) {
  yueah_string_t *lowercase = str;

  for (u64 i = 0; i < lowercase->len; i++) {
    lowercase->data[i] = tolower(lowercase->data[i]);
  }
}

void yueah_string_upper(yueah_string_t *str) {
  yueah_string_t *uppercase = str;

  for (u64 i = 0; i < uppercase->len; i++) {
    uppercase->data[i] = toupper(uppercase->data[i]);
  }
}
