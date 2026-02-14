#include <sodium/crypto_auth_hmacsha512256.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

static unsigned char *yueah_get_jwt_key(h2o_mem_pool_t *pool) {
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

static char *yueah_jwt_header_to_json(h2o_mem_pool_t *pool,
                                      const yueah_jwt_header_t *header,
                                      mem_t *out_len) {
  char *header_json = NULL;

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

  *out_len = header_json_len;
  return header_json;
}

static char *yueah_jwt_encode_header(h2o_mem_pool_t *pool,
                                     yueah_jwt_header_t *header,
                                     mem_t *out_len) {
  char *encoded_header = NULL;
  char *header_json = NULL;
  mem_t header_json_len = 0;
  mem_t base64_out_len = 0;

  header_json = yueah_jwt_header_to_json(pool, header, &header_json_len);
  if (!header_json) {
    yueah_log(Error, false, "Error encoding header");
    return NULL;
  }

  encoded_header = yueah_base64_encode(pool, (unsigned char *)header_json,
                                       header_json_len, &base64_out_len);
  if (!encoded_header) {
    yueah_log(Error, false, "Error encoding header");
    return NULL;
  }

  *out_len = base64_out_len;
  return encoded_header;
}

static char *yueah_generate_jti(h2o_mem_pool_t *pool) {
  size_t bin_len = 32 / 2;
  unsigned char random_bytes[bin_len];
  char jti_hex[bin_len * 2 + 1];

  randombytes_buf(random_bytes, bin_len);
  char *jti_hex_str =
      sodium_bin2hex(jti_hex, bin_len * 2 + 1, random_bytes, bin_len);
  if (!jti_hex_str) {
    yueah_log(Error, false, "Failed to make jti hex");
    return NULL;
  }

  char *jti = h2o_mem_alloc_pool(pool, char, bin_len * 2 + 1);
  memcpy(jti, jti_hex, bin_len * 2 + 1);

  return jti;
}

static yueah_jwt_claims_t *
yueah_jwt_create_claims(h2o_mem_pool_t *pool, const char *iss, const char *sub,
                        const char *aud, const mem_t exp, const mem_t nbf) {
  yueah_jwt_claims_t *claims = h2o_mem_alloc_pool(pool, yueah_jwt_claims_t, 1);

  time_t now = time(NULL);
  mem_t exp_time = now + exp;
  mem_t iat_time = now;
  mem_t nbf_time = now + nbf;

  claims->iss = yueah_strdup(pool, iss, strlen(iss) + 1);
  claims->sub = yueah_strdup(pool, sub, strlen(sub) + 1);
  claims->aud = yueah_strdup(pool, aud, strlen(aud) + 1);
  claims->iat = iat_time;
  claims->exp = exp_time;
  claims->jti = yueah_generate_jti(pool);
  claims->nbf = nbf_time;

  return claims;
}

static char *yueah_payload_to_json(h2o_mem_pool_t *pool,
                                   const yueah_jwt_claims_t *claims,
                                   mem_t *out_len) {
  char *payload_json = NULL;

  mem_t payload_json_len = 0;
  yyjson_write_err err;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  yyjson_mut_val *iss_key = yyjson_mut_str(doc, "iss");
  yyjson_mut_val *iss_val = yyjson_mut_str(doc, claims->iss);

  yyjson_mut_val *sub_key = yyjson_mut_str(doc, "sub");
  yyjson_mut_val *sub_val = yyjson_mut_str(doc, claims->sub);

  yyjson_mut_val *aud_key = yyjson_mut_str(doc, "aud");
  yyjson_mut_val *aud_val = yyjson_mut_str(doc, claims->aud);

  yyjson_mut_val *iat_key = yyjson_mut_str(doc, "iat");
  yyjson_mut_val *iat_val = yyjson_mut_uint(doc, claims->iat);

  yyjson_mut_val *exp_key = yyjson_mut_str(doc, "exp");
  yyjson_mut_val *exp_val = yyjson_mut_uint(doc, claims->exp);

  yyjson_mut_val *jti_key = yyjson_mut_str(doc, "jti");
  yyjson_mut_val *jti_val = yyjson_mut_str(doc, claims->jti);

  yyjson_mut_val *nbf_key = yyjson_mut_str(doc, "nbf");
  yyjson_mut_val *nbf_val = yyjson_mut_uint(doc, claims->nbf);

  yyjson_mut_obj_add(root, iss_key, iss_val);
  yyjson_mut_obj_add(root, sub_key, sub_val);
  yyjson_mut_obj_add(root, aud_key, aud_val);
  yyjson_mut_obj_add(root, iat_key, iat_val);
  yyjson_mut_obj_add(root, exp_key, exp_val);
  yyjson_mut_obj_add(root, jti_key, jti_val);
  yyjson_mut_obj_add(root, nbf_key, nbf_val);

  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &payload_json_len,
                        &err);

  payload_json = h2o_mem_alloc_pool(pool, char, payload_json_len);
  payload_json =
      yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, NULL, &err);

  if (!payload_json) {
    yueah_log(Error, false, "Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  *out_len = payload_json_len;
  return payload_json;
}

static unsigned char *
yueah_jwt_create_signature(h2o_mem_pool_t *pool, const unsigned char *key,
                           const yueah_jwt_claims_t *claims,
                           const yueah_jwt_header_t *header) {
  int rc = 0;
  unsigned char *signature = NULL;
  unsigned char hash[crypto_auth_hmacsha512256_BYTES];
  unsigned char *key_ptr = (unsigned char *)key;

  mem_t header_json_len = 0;
  mem_t payload_out_len = 0;
  mem_t concat_len = 0;

  char *payload_json = yueah_payload_to_json(pool, claims, &payload_out_len);
  char *header_json = yueah_jwt_header_to_json(pool, header, &header_json_len);

  concat_len = header_json_len + payload_out_len + 2;

  char *concat = h2o_mem_alloc_pool(pool, char, concat_len);
  snprintf(concat, concat_len, "%s.%s", header_json, payload_json);

  rc = crypto_auth_hmacsha512256(hash, (unsigned char *)payload_json,
                                 concat_len, key_ptr);
  if (rc != 0) {
    yueah_log(Error, false, "Failed to init hmac, return code %d", rc);
    return NULL;
  }

  signature =
      h2o_mem_alloc_pool(pool, unsigned char, crypto_auth_hmacsha512256_BYTES);
  memcpy(signature, hash, crypto_auth_hmacsha512256_BYTES);

  return signature;
}

static char *yueah_jwt_encode_signature(h2o_mem_pool_t *pool,
                                        const unsigned char *key,
                                        const yueah_jwt_claims_t *claims,
                                        const yueah_jwt_header_t *header,
                                        mem_t *out_len) {
  char *encoded_signature = NULL;
  unsigned char *signature =
      yueah_jwt_create_signature(pool, key, claims, header);

  mem_t base64_out_len = 0;
  encoded_signature = yueah_base64_encode(
      pool, signature, crypto_auth_hmacsha512256_BYTES, &base64_out_len);
  if (!encoded_signature || base64_out_len < 10) {
    yueah_log(Error, false, "Failed to encode signature");
    return NULL;
  }

  *out_len = base64_out_len;
  return encoded_signature;
}

static char *yueah_jwt_encode_payload(h2o_mem_pool_t *pool,
                                      const unsigned char *key,
                                      const yueah_jwt_claims_t *payload,
                                      mem_t *out_len) {
  mem_t json_payload_len = 0;
  char *json_payload = yueah_payload_to_json(pool, payload, &json_payload_len);
  yueah_log(Debug, false, "json_payload: %s", json_payload);
  char *encoded_payload = NULL;

  mem_t base64_out_len = 0;
  encoded_payload = yueah_base64_encode(pool, (unsigned char *)json_payload,
                                        json_payload_len, &base64_out_len);
  if (!encoded_payload || base64_out_len < 10) {
    yueah_log(Error, false, "Failed to encode payload");
    return NULL;
  }

  *out_len = base64_out_len;
  return encoded_payload;
}

char *yueah_jwt_encode(h2o_mem_pool_t *pool, const char *payload) {
  const unsigned char *key = yueah_get_jwt_key(pool);
  if (!key)
    return NULL;

  yueah_jwt_header_t *header = yueah_jwt_create_header(pool);
  yueah_jwt_claims_t *claims =
      yueah_jwt_create_claims(pool, "yueah", "yueah", "yueah", 3600, 0);

  mem_t encoded_header_len = 0;
  mem_t encoded_payload_len = 0;
  mem_t encoded_signature_len = 0;
  mem_t concat_len = 0;

  char *encoded_header =
      yueah_jwt_encode_header(pool, header, &encoded_header_len);
  char *encoded_payload =
      yueah_jwt_encode_payload(pool, key, claims, &encoded_payload_len);
  char *encoded_signature = yueah_jwt_encode_signature(
      pool, key, claims, header, &encoded_signature_len);

  concat_len =
      encoded_header_len + encoded_payload_len + encoded_signature_len + 3;
  char *concat = h2o_mem_alloc_pool(pool, char, concat_len);
  snprintf(concat, concat_len, "%s.%s.%s", encoded_header, encoded_payload,
           encoded_signature);

  yueah_log(Debug, true, "header: %s", encoded_header);
  yueah_log(Debug, true, "payload: %s", encoded_payload);
  yueah_log(Debug, true, "signature: %s", encoded_signature);
  yueah_log(Debug, true, "concat: %s", concat);

  return concat;
}
char *yueah_jwt_decode(h2o_mem_pool_t *pool, const char *token) {
  const unsigned char *key = yueah_get_jwt_key(pool);
  if (!key)
    return NULL;

  return NULL;
}
