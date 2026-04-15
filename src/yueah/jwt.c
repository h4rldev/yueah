#include <stdio.h>
#include <string.h>
#include <time.h>

#include <h2o.h>
#include <sodium.h>
#include <yyjson.h>

#include <yueah/base64.h>
#include <yueah/file.h>
#include <yueah/jwt.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

static int yueah_generate_jwt_key(h2o_mem_pool_t *pool,
                                  const yueah_string_t *key_path) {
  if (!key_path) {
    yueah_log_error("Invalid key path");
    return -1;
  }

  FILE *key_file;

  u64 key_len = crypto_auth_hmacsha512_KEYBYTES;
  u64 hex_key_len = crypto_auth_hmacsha512_KEYBYTES * 2 + 1;

  ucstr key[crypto_auth_hmacsha512_KEYBYTES];
  cstr hex_key[crypto_auth_hmacsha512_KEYBYTES * 2 + 1];

  crypto_auth_hmacsha512_keygen(key);
  cstr *hex_key_str = sodium_bin2hex(hex_key, hex_key_len, key, key_len);
  if (!hex_key_str) {
    yueah_log_error("Failed to make key hex");
    return -1;
  }

  cstr *key_path_cstr = yueah_string_to_cstr(pool, key_path);
  key_file = fopen(key_path_cstr, "w");
  hex_key[strlen(hex_key)] = '\n';
  fwrite(hex_key, sizeof(hex_key), 1, key_file);
  fwrite("use this key instead ^", strlen("use this key instead ^"), 1,
         key_file);
  fclose(key_file);

  return 0;
}

static int yueah_generate_access_jwt_key(h2o_mem_pool_t *pool) {
  return yueah_generate_jwt_key(pool, YUEAH_STR("access_jwt_key.txt"));
}

static int yueah_generate_refresh_jwt_key(h2o_mem_pool_t *pool) {
  return yueah_generate_jwt_key(pool, YUEAH_STR("refresh_jwt_key.txt"));
}

static yueah_string_t *yueah_get_jwt_key(h2o_mem_pool_t *pool,
                                         yueah_jwt_key_type_t key_type) {
  cstr key_path[1024];
  cstr key_var[1024];

  switch (key_type) {
  case Access:
    yueah_log_debug("Using access key");
    strlcpy(key_var, "YUEAH_ACCESS_JWT_KEY", sizeof(key_var));
    strlcpy(key_path, "access_jwt_key.txt", sizeof(key_path));
    break;
  case Refresh:
    yueah_log_debug("Using refresh key");
    strlcpy(key_var, "YUEAH_REFRESH_JWT_KEY", sizeof(key_var));
    strlcpy(key_path, "refresh_jwt_key.txt", sizeof(key_path));
    break;
  default:
    yueah_log_error("Invalid key type");
    return NULL;
  }

  const cstr *key = getenv(key_var);
  int rc = 0;
  u64 key_len = 0;
  static unsigned char key_buf[crypto_auth_hmacsha512_KEYBYTES];

  if (!key) {
    yueah_log_error("%s not set, generating a new one, check %s", key_var,
                    key_path);
    switch (key_type) {
    case Access:
      yueah_generate_access_jwt_key(pool);
      break;
    case Refresh:
      yueah_generate_refresh_jwt_key(pool);
      break;
    default:
      yueah_log_error("Invalid key type");
      break;
    }
    return NULL;
  }

  cstr hex_key_end[64];

  rc = sodium_hex2bin(key_buf, crypto_auth_hmacsha512_KEYBYTES, key,
                      strlen(key), NULL, &key_len, (const char **)&hex_key_end);
  if (rc != 0) {
    yueah_log_error("Failed to decode hex_key: code %d, hex_end %02x", rc,
                    hex_key_end);
    return NULL;
  }

  if (key_len < crypto_auth_hmacsha512_KEYBYTES) {
    yueah_log_error("Invalid key, too short, generating a new one, check %s",
                    key_var, key_path);
    switch (key_type) {
    case Access:
      yueah_generate_access_jwt_key(pool);
      break;
    case Refresh:
      yueah_generate_refresh_jwt_key(pool);
      break;
    default:
      yueah_log_error("Invalid key type");
      break;
    }
    return NULL;
  }

  return yueah_string_new(pool, (cstr *)key_buf, key_len);
}

static yueah_jwt_header_t *yueah_jwt_create_header(h2o_mem_pool_t *pool) {
  yueah_jwt_header_t *header = h2o_mem_alloc_pool(pool, yueah_jwt_header_t, 1);
  header->alg = YUEAH_STR("HS512");
  header->typ = YUEAH_STR("JWT");

  return header;
}

static yueah_string_t *
yueah_jwt_header_to_json(h2o_mem_pool_t *pool,
                         const yueah_jwt_header_t *header) {
  cstr *header_json = NULL;

  u64 header_json_len = 0;
  yyjson_write_err err;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  yyjson_mut_val *alg_key = yyjson_mut_str(doc, "alg");
  cstr *alg_val_cstr = yueah_string_to_cstr(pool, header->alg);
  yyjson_mut_val *alg_val = yyjson_mut_str(doc, alg_val_cstr);

  yyjson_mut_val *typ_key = yyjson_mut_str(doc, "typ");
  cstr *typ_val_cstr = yueah_string_to_cstr(pool, header->typ);
  yyjson_mut_val *typ_val = yyjson_mut_str(doc, typ_val_cstr);

  yyjson_mut_obj_add(root, alg_key, alg_val);
  yyjson_mut_obj_add(root, typ_key, typ_val);

  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &header_json_len, &err);

  header_json = h2o_mem_alloc_pool(pool, char, header_json_len);
  header_json =
      yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, NULL, &err);
  if (!header_json) {
    yueah_log_error("Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  return yueah_string_new(pool, header_json, header_json_len);
}

static yueah_string_t *yueah_jwt_encode_header(h2o_mem_pool_t *pool,
                                               yueah_jwt_header_t *header) {
  yueah_string_t *encoded_header = NULL;
  yueah_string_t *header_json = NULL;
  header_json = yueah_jwt_header_to_json(pool, header);
  if (!header_json) {
    yueah_log_error("Error encoding header");
    return NULL;
  }

  encoded_header = yueah_base64_encode(pool, header_json);
  if (!encoded_header) {
    yueah_log_error("Error encoding header");
    return NULL;
  }

  return encoded_header;
}

static yueah_string_t *yueah_generate_jti(h2o_mem_pool_t *pool) {
  u64 bin_len = 32 / 2;
  ucstr random_bytes[bin_len];
  cstr jti_hex[bin_len * 2 + 1];

  randombytes_buf(random_bytes, bin_len);
  cstr *jti_hex_str =
      sodium_bin2hex(jti_hex, bin_len * 2 + 1, random_bytes, bin_len);
  if (!jti_hex_str) {
    yueah_log_error("Failed to make jti hex");
    return NULL;
  }

  cstr *jti = h2o_mem_alloc_pool(pool, char, bin_len * 2 + 1);
  memcpy(jti, jti_hex, bin_len * 2 + 1);

  return yueah_string_new(pool, jti, bin_len * 2 + 1);
}

yueah_jwt_claims_t *yueah_jwt_create_claims(h2o_mem_pool_t *pool,
                                            const yueah_string_t *iss,
                                            const yueah_string_t *sub,
                                            const yueah_string_t *aud,
                                            const u64 exp, const u64 nbf) {
  yueah_jwt_claims_t *claims = h2o_mem_alloc_pool(pool, yueah_jwt_claims_t, 1);

  time_t now = time(NULL);
  u64 exp_time = now + exp;
  u64 iat_time = now;
  u64 nbf_time = now + nbf;

  claims->iss = iss;
  claims->sub = sub;
  claims->aud = aud;
  claims->iat = iat_time;
  claims->exp = exp_time;
  claims->jti = yueah_generate_jti(pool);
  claims->nbf = nbf_time;

  return claims;
}

static yueah_string_t *yueah_payload_to_json(h2o_mem_pool_t *pool,
                                             const yueah_jwt_claims_t *claims) {
  cstr *payload_json = NULL;

  u64 payload_json_len = 0;
  yyjson_write_err err;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);

  cstr *iss_cstr = yueah_string_to_cstr(pool, claims->iss);
  cstr *sub_cstr = yueah_string_to_cstr(pool, claims->sub);
  cstr *aud_cstr = yueah_string_to_cstr(pool, claims->aud);
  cstr *jti_cstr = yueah_string_to_cstr(pool, claims->jti);

  yyjson_mut_val *iss_key = yyjson_mut_str(doc, "iss");
  yyjson_mut_val *iss_val = yyjson_mut_str(doc, iss_cstr);

  yyjson_mut_val *sub_key = yyjson_mut_str(doc, "sub");
  yyjson_mut_val *sub_val = yyjson_mut_str(doc, sub_cstr);

  yyjson_mut_val *aud_key = yyjson_mut_str(doc, "aud");
  yyjson_mut_val *aud_val = yyjson_mut_str(doc, aud_cstr);

  yyjson_mut_val *iat_key = yyjson_mut_str(doc, "iat");
  yyjson_mut_val *iat_val = yyjson_mut_uint(doc, claims->iat);

  yyjson_mut_val *exp_key = yyjson_mut_str(doc, "exp");
  yyjson_mut_val *exp_val = yyjson_mut_uint(doc, claims->exp);

  yyjson_mut_val *jti_key = yyjson_mut_str(doc, "jti");
  yyjson_mut_val *jti_val = yyjson_mut_str(doc, jti_cstr);

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
    yueah_log_error("Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  return yueah_string_new(pool, payload_json, payload_json_len);
}

static yueah_string_t *
yueah_jwt_create_signature_from_str(h2o_mem_pool_t *pool,
                                    const yueah_string_t *key,
                                    const yueah_string_t *body) {
  int rc = 0;
  ucstr *signature = NULL;
  ucstr hash[crypto_auth_hmacsha512_BYTES];

  ucstr *key_cstr = (ucstr *)yueah_string_to_cstr(pool, key);
  if (crypto_auth_hmacsha512(hash, body->data, body->len, key_cstr) != 0) {
    yueah_log_error("Failed to hash hmac, return code %d", rc);
    return NULL;
  }

  signature =
      h2o_mem_alloc_pool(pool, unsigned char, crypto_auth_hmacsha512_BYTES);
  memcpy(signature, hash, crypto_auth_hmacsha512_BYTES);

  return yueah_string_new(pool, (cstr *)signature,
                          crypto_auth_hmacsha512_BYTES);
}

static yueah_string_t *
yueah_jwt_create_signature(h2o_mem_pool_t *pool, const yueah_string_t *key,
                           const yueah_jwt_claims_t *claims,
                           const yueah_jwt_header_t *header) {

  u64 concat_len = 0;

  yueah_string_t *header_json = yueah_jwt_header_to_json(pool, header);
  yueah_string_t *payload_json = yueah_payload_to_json(pool, claims);

  concat_len = header_json->len + payload_json->len + 1;

  yueah_string_t *concat = yueah_string_new(pool, NULL, concat_len);
  memcpy(concat->data, header_json->data, header_json->len);
  memcpy(concat->data + header_json->len, ".", 1);
  memcpy(concat->data + header_json->len + 1, payload_json->data,
         payload_json->len);

  return yueah_jwt_create_signature_from_str(pool, key, concat);
}

static yueah_string_t *
yueah_jwt_encode_signature(h2o_mem_pool_t *pool, const yueah_string_t *key,
                           const yueah_jwt_claims_t *claims,
                           const yueah_jwt_header_t *header) {
  yueah_string_t *encoded_signature = NULL;
  yueah_string_t *signature =
      yueah_jwt_create_signature(pool, key, claims, header);

  encoded_signature = yueah_base64_encode(pool, signature);
  if (!encoded_signature || encoded_signature->len < 10) {
    yueah_log_error("Failed to encode signature");
    return NULL;
  }

  return encoded_signature;
}

static yueah_string_t *
yueah_jwt_encode_payload(h2o_mem_pool_t *pool, const yueah_string_t *key,
                         const yueah_jwt_claims_t *payload) {
  yueah_string_t *json_payload = yueah_payload_to_json(pool, payload);
  yueah_string_t *encoded_payload = NULL;

  encoded_payload = yueah_base64_encode(pool, json_payload);
  if (!encoded_payload || encoded_payload->len < 10) {
    yueah_log_error("Failed to encode payload");
    return NULL;
  }

  return encoded_payload;
}

yueah_string_t *yueah_jwt_encode(h2o_mem_pool_t *pool,
                                 const yueah_jwt_claims_t *payload,
                                 yueah_jwt_key_type_t key_type) {
  const yueah_string_t *key = yueah_get_jwt_key(pool, key_type);
  if (!key)
    return NULL;

  yueah_jwt_header_t *header = yueah_jwt_create_header(pool);
  const yueah_jwt_claims_t *claims = payload;

  u64 concat_len = 0;

  yueah_string_t *encoded_header = yueah_jwt_encode_header(pool, header);
  yueah_string_t *encoded_payload = yueah_jwt_encode_payload(pool, key, claims);
  yueah_string_t *encoded_signature =
      yueah_jwt_encode_signature(pool, key, claims, header);

  concat_len =
      encoded_header->len + encoded_payload->len + encoded_signature->len + 2;
  yueah_string_t *concat = yueah_string_new(pool, NULL, concat_len);

  memcpy(concat->data, encoded_header->data, encoded_header->len);
  memcpy(concat->data + encoded_header->len, ".", 1);
  memcpy(concat->data + encoded_header->len + 1, encoded_payload->data,
         encoded_payload->len);
  memcpy(concat->data + encoded_header->len + 1 + encoded_payload->len, ".", 1);
  memcpy(concat->data + encoded_header->len + 1 + encoded_payload->len + 1,
         encoded_signature->data, encoded_signature->len);

  return concat;
}

bool yueah_jwt_verify(h2o_mem_pool_t *pool, const yueah_string_t *token,
                      const yueah_string_t *aud,
                      yueah_jwt_key_type_t key_type) {
  yueah_string_t *key = yueah_get_jwt_key(pool, key_type);
  if (!key)
    return false;

  u64 header_len = 0;
  u64 payload_len = 0;
  u64 signature_len = 0;

  cstr header_cstr[1024] = {0};
  yueah_string_t *header = {0};
  yueah_string_t *header_json = {0};

  cstr payload_cstr[2048] = {0};
  yueah_string_t *payload = {0};
  yueah_string_t *payload_json = {0};

  cstr signature_cstr[1024] = {0};
  yueah_string_t *signature = {0};
  yueah_string_t *decoded_signature = {0};

  yueah_string_t *concat = {0};

  cstr *token_cstr = yueah_string_to_cstr(pool, token);
  header_len = strchr(token_cstr, '.') - token_cstr;
  payload_len =
      strchr(token_cstr + header_len + 1, '.') - token_cstr - header_len - 1;
  signature_len = strlen(token_cstr) - header_len - payload_len - 2;

  sscanf(token_cstr, "%[^.].%[^.].%s", header_cstr, payload_cstr,
         signature_cstr);

  header = yueah_string_new(pool, header_cstr, header_len);
  payload = yueah_string_new(pool, payload_cstr, payload_len);

  header_json = yueah_base64_decode(pool, header, header_len);
  payload_json = yueah_base64_decode(pool, payload, payload_len);

  concat =
      yueah_string_new(pool, NULL, header_json->len + payload_json->len + 1);

  memcpy(concat->data, header_json->data, header_json->len);
  memcpy(concat->data + header_json->len, ".", 1);
  memcpy(concat->data + header_json->len + 1, payload_json->data,
         payload_json->len);

  yueah_string_t *generated_signature =
      yueah_jwt_create_signature_from_str(pool, key, concat);

  decoded_signature = yueah_base64_decode(
      pool, signature, signature_len + crypto_auth_hmacsha512_BYTES);

  if (sodium_memcmp(generated_signature->data, decoded_signature->data,
                    decoded_signature->len) != 0)
    return false;

  return true;
}

yueah_string_t *yueah_jwt_get_sub(h2o_mem_pool_t *pool,
                                  const yueah_string_t *token) {
  time_t now_local = time(NULL);
  struct tm *time_buf = localtime(&now_local);
  time_t now = timegm(time_buf);

  yyjson_read_err err;

  yyjson_doc *doc = NULL;
  yyjson_val *root = NULL;

  yyjson_val *iss_val = NULL;
  yyjson_val *sub_val = NULL;
  yyjson_val *aud_val = NULL;
  yyjson_val *iat_val = NULL;
  yyjson_val *exp_val = NULL;
  yyjson_val *jti_val = NULL;
  yyjson_val *nbf_val = NULL;

  u64 header_len = 0;
  u64 payload_len = 0;

  cstr header_cstr[1024] = {0};
  cstr payload_cstr[2048] = {0};
  yueah_string_t *payload = {0};

  yueah_string_t *payload_json = {0};
  cstr signature_cstr[1024] = {0};

  cstr *token_cstr = yueah_string_to_cstr(pool, token);

  header_len = strchr(token_cstr, '.') - token_cstr;
  payload_len =
      strchr(token_cstr + header_len + 1, '.') - token_cstr - header_len - 1;

  sscanf(token_cstr, "%[^.].%[^.].%s", header_cstr, payload_cstr,
         signature_cstr);

  payload_json = yueah_base64_decode(pool, payload, payload_len);

  payload_json = yueah_base64_decode(pool, payload, 2048);
  if (!payload_json || payload_json->len < 10) {
    yueah_log_error("Failed to decode payload");
    return NULL;
  }

  doc = yyjson_read_opts((char *)payload_json->data, payload_json->len,
                         YYJSON_READ_NOFLAG, NULL, &err);
  if (!doc) {
    yueah_log_error("Error parsing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  root = yyjson_doc_get_root(doc);

  iss_val = yyjson_obj_get(root, "iss");
  sub_val = yyjson_obj_get(root, "sub");
  aud_val = yyjson_obj_get(root, "aud");
  iat_val = yyjson_obj_get(root, "iat");
  exp_val = yyjson_obj_get(root, "exp");
  jti_val = yyjson_obj_get(root, "jti");
  nbf_val = yyjson_obj_get(root, "nbf");

  if (!iss_val || !yyjson_is_str(iss_val)) {
    yueah_log_error("Failed to get iss");
    return NULL;
  }

  if (!sub_val || !yyjson_is_str(sub_val)) {
    yueah_log_error("Failed to get sub");
    return NULL;
  }

  if (!aud_val || !yyjson_is_str(aud_val)) {
    yueah_log_error("Failed to get aud");
    return NULL;
  }

  if (!iat_val || !yyjson_is_uint(iat_val)) {
    yueah_log_error("Failed to get iat");
    return NULL;
  }

  if (!exp_val || !yyjson_is_uint(exp_val)) {
    yueah_log_error("Failed to get exp");
    return NULL;
  }

  if (!jti_val || !yyjson_is_str(jti_val)) {
    yueah_log_error("Failed to get jti");
    return NULL;
  }

  if (!nbf_val || !yyjson_is_uint(nbf_val)) {
    yueah_log_error("Failed to get nbf");
    return NULL;
  }

  if (strcasecmp(yyjson_get_str(iss_val), "yueah") != 0) {
    yueah_log_error("Invalid iss");
    return NULL;
  }

  if (strcasecmp(yyjson_get_str(aud_val), "blog") != 0) {
    yueah_log_error("Invalid aud");
    return NULL;
  }

  if ((long)yyjson_get_uint(exp_val) < now) {
    yueah_log_error("Token expired");
    return NULL;
  }

  if ((long)yyjson_get_uint(nbf_val) > now) {
    yueah_log_error("Token not valid yet");
    return NULL;
  }

  u64 sub_val_len = strlen(yyjson_get_str(sub_val));
  yueah_string_t *sub =
      yueah_string_new(pool, yyjson_get_str(sub_val), sub_val_len);
  if (sub == NULL || sub_val_len < 10) {
    yueah_log_error("Failed to get sub");
    return NULL;
  }

  return sub;
}
