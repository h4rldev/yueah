#include <stdio.h>

#include <base64.h>
#include <jwt.h>
#include <log.h>

#include <h2o.h>
#include <sodium.h>
#include <yyjson.h>

static int generate_yueah_jwt_key(void) {
  FILE *key_file;

  mem_t key_len = crypto_auth_hmacsha512256_KEYBYTES;
  mem_t hex_key_len = crypto_auth_hmacsha512256_KEYBYTES * 2 + 1;

  unsigned char key[crypto_auth_hmacsha512256_KEYBYTES];
  char hex_key[crypto_auth_hmacsha512256_KEYBYTES * 2 + 1];

  crypto_auth_hmacsha512256_keygen(key);
  char *hex_key_str = sodium_bin2hex(hex_key, hex_key_len, key, key_len);
  if (!hex_key_str) {
    yueah_log(Error, false, "Failed to make key hex");
    return -1;
  }

  key_file = fopen("jwt_key.txt", "w");
  hex_key[strlen(hex_key)] = '\n';
  fwrite(hex_key, sizeof(hex_key), 1, key_file);
  fwrite("use this key instead ^", strlen("use this key instead ^"), 1,
         key_file);
  fclose(key_file);

  return 0;
}

static unsigned char *get_jwt_key(h2o_mem_pool_t *pool) {
  const char *key = getenv("YUEAH_JWT_KEY");
  int rc = 0;
  mem_t key_len = 0;
  unsigned char *key_buf;

  if (!key) {
    yueah_log(Error, true,
              "YUEAH_JWT_KEY not set, generating a new one, check "
              "jwt_key.txt");
    generate_yueah_jwt_key();
    return NULL;
  }

  key_buf = h2o_mem_alloc_pool(pool, unsigned char,
                               crypto_auth_hmacsha512256_KEYBYTES);

  char hex_key_end[64];

  rc = sodium_hex2bin(key_buf, crypto_auth_hmacsha512256_KEYBYTES, key,
                      strlen(key), NULL, &key_len, (const char **)&hex_key_end);
  if (rc != 0) {
    yueah_log(Error, false, "Failed to decode hex_key: code %d, hex_end %02x",
              rc, hex_key_end);
    return NULL;
  }

  if (key_len < crypto_auth_hmacsha512256_KEYBYTES) {
    yueah_log(Error, true, "Invalid key, too short");
    return NULL;
  }

  return key_buf;
}

static yueah_jwt_header_t *yueah_jwt_create_header(h2o_mem_pool_t *pool) {
  yueah_jwt_header_t *header = h2o_mem_alloc_pool(pool, yueah_jwt_header_t, 1);
  header->alg = h2o_mem_alloc_pool(pool, char, strlen("HS256") + 1);
  header->typ = h2o_mem_alloc_pool(pool, char, strlen("JWT") + 1);

  header->alg = "HS256";
  header->typ = "JWT";

  return header;
}

static char *yueah_jwt_encode_header(h2o_mem_pool_t *pool,
                                     yueah_jwt_header_t *header) {
  char *header_json = NULL;
  char *encoded_header = NULL;

  mem_t header_json_len = 0;
  yyjson_write_err err;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  yyjson_mut_val *alg_key = yyjson_mut_str(doc, "alg");
  yyjson_mut_val *alg_val = yyjson_mut_str(doc, header->alg);

  yyjson_mut_val *typ_key = yyjson_mut_str(doc, "typ");
  yyjson_mut_val *typ_val = yyjson_mut_str(doc, header->typ);

  yyjson_mut_obj_add(root, alg_key, alg_val);
  yyjson_mut_obj_add(root, typ_key, typ_val);

  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &header_json_len, &err);

  header_json = h2o_mem_alloc_pool(pool, char, header_json_len);
  header_json =
      yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, NULL, &err);
  if (!header_json) {
    yueah_log(Error, false, "Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  mem_t out_len = 0;
  encoded_header = yueah_base64_encode(pool, (unsigned char *)header_json,
                                       header_json_len, &out_len);
  if (!encoded_header) {
    yueah_log(Error, false, "Error encoding header");
    return NULL;
  }

  return encoded_header;
}

char *yueah_jwt_encode(h2o_mem_pool_t *pool, const char *payload) {
  const unsigned char *key = get_jwt_key(pool);
  if (!key)
    return NULL;

  yueah_jwt_header_t *header = yueah_jwt_create_header(pool);
  char *encoded_header = yueah_jwt_encode_header(pool, header);
  yueah_log(Debug, true, "header_json: %s", encoded_header);

  return NULL;
}
char *yueah_jwt_decode(h2o_mem_pool_t *pool, const char *token) {
  const unsigned char *key = get_jwt_key(pool);
  if (!key)
    return NULL;

  return NULL;
}
