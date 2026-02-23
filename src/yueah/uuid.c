#include <h2o.h>
#include <sodium.h>

#include <yueah/uuid.h>

char *yueah_uuid_new(h2o_mem_pool_t *pool) {
  char *uuid = h2o_mem_alloc_pool(pool, char, 37);
  unsigned char uuid_bytes[16];
  randombytes_buf(uuid_bytes, 16);

  uuid_bytes[6] = (uuid_bytes[6] & 0x0f) | 0x40;
  uuid_bytes[8] = (uuid_bytes[8] & 0x3f) | 0x80;

  snprintf(
      uuid, 37,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3], uuid_bytes[4],
      uuid_bytes[5], uuid_bytes[6], uuid_bytes[7], uuid_bytes[8], uuid_bytes[9],
      uuid_bytes[10], uuid_bytes[11], uuid_bytes[12], uuid_bytes[13],
      uuid_bytes[14], uuid_bytes[15]);

  return uuid;
}
