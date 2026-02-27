#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/cookie.h>
#include <yueah/log.h>
#include <yueah/shared.h>

#define YUEAH_COOKIE_CIPHER_LEN (4096 + crypto_secretbox_MACBYTES)

static int generate_yueah_key(void) {
  FILE *key_file;
  unsigned char key[crypto_secretbox_KEYBYTES];
  char hex_key[crypto_secretbox_KEYBYTES * 2 + 1];

  crypto_secretbox_keygen(key);
  char *hex_key_str = sodium_bin2hex(hex_key, crypto_secretbox_KEYBYTES * 2 + 1,
                                     key, crypto_secretbox_KEYBYTES);
  if (hex_key_str == NULL || hex_key_str != hex_key) {
    yueah_log(Error, false, "Failed to make key hex");
    return -1;
  }

  key_file = fopen("cookie_key.txt", "w");
  hex_key[strlen(hex_key)] = '\n';
  fwrite(hex_key, sizeof(hex_key), 1, key_file);
  fwrite("use this key instead ^", strlen("use this key instead ^"), 1,
         key_file);
  fclose(key_file);

  return 0;
}

unsigned char *yueah_cookie_encrypt(h2o_mem_pool_t *pool,
                                    unsigned char *content, mem_t content_len,
                                    mem_t *out_len) {
  if (!pool || !content)
    return NULL;

  unsigned char key[crypto_secretbox_KEYBYTES];
  unsigned char nonce[crypto_secretbox_NONCEBYTES];

  mem_t ciphertext_len =
      content_len + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES;
  unsigned char *ciphertext =
      h2o_mem_alloc_pool(pool, unsigned char *, ciphertext_len);

  char *key_buf = getenv("YUEAH_AES_KEY");
  if (!key_buf || strlen(key_buf) < crypto_secretbox_KEYBYTES) {
    yueah_log(Error, true,
              "Invalid YUEAH_AES_KEY, too short, generating one, check "
              "cookie_key.txt");
    if (generate_yueah_key() < 0)
      return NULL;

    return NULL;
  }

  memcpy(key, key_buf, crypto_secretbox_KEYBYTES);

  randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);

  memcpy(ciphertext, nonce, crypto_secretbox_NONCEBYTES);
  crypto_secretbox_easy(ciphertext + crypto_secretbox_NONCEBYTES, content,
                        content_len, nonce, key);

  *out_len = ciphertext_len;
  return ciphertext;
}

unsigned char *yueah_cookie_decrypt(h2o_mem_pool_t *pool, unsigned char *bin,
                                    mem_t bin_len, mem_t *out_len) {
  if (!pool || !bin)
    return NULL;

  int rc = 0;
  char *key_buf = getenv("YUEAH_AES_KEY");
  if (!key_buf || strlen(key_buf) < crypto_secretbox_KEYBYTES) {
    yueah_log(Error, true, "Invalid YUEAH_AES_KEY, nothing to decrypt");
    return NULL;
  }

  mem_t decrypted_len =
      bin_len - crypto_secretbox_MACBYTES - crypto_secretbox_NONCEBYTES;

  unsigned char key[crypto_secretbox_KEYBYTES];
  unsigned char nonce[crypto_secretbox_NONCEBYTES];
  unsigned char *ciphertext = h2o_mem_alloc_pool(
      pool, unsigned char *, bin_len - crypto_secretbox_NONCEBYTES);
  unsigned char *result =
      h2o_mem_alloc_pool(pool, unsigned char *, decrypted_len);

  memcpy(key, key_buf, crypto_secretbox_KEYBYTES);

  memmove(nonce, bin, crypto_secretbox_NONCEBYTES);
  memmove(ciphertext, bin + crypto_secretbox_NONCEBYTES,
          bin_len - crypto_secretbox_NONCEBYTES);

  rc = crypto_secretbox_open_easy(
      result, ciphertext, bin_len - crypto_secretbox_NONCEBYTES, nonce, key);
  if (rc != 0) {
    yueah_log(Error, true, "Failed to decrypt content, code %d", rc);
    return NULL;
  }

  decrypted_len -= 2;
  *out_len = decrypted_len;
  result[decrypted_len] = '\0';
  return result;
}

static char *unix_time_to_date_header(uint64_t unix_time) {
  static char date[1024] = {0};

  char wday[4] = {0};
  char month[4] = {0};

  time_t time_container = unix_time;
  struct tm *time_ = gmtime(&time_container);
  time_t now = time(NULL);

  double diff_seconds = difftime(time_container, now);
  if (diff_seconds < 0) {
    yueah_log(Error, true, "Time shouldn't be in the past");
    return NULL;
  }

  if (diff_seconds >= 34560000) {
    yueah_log(Error, true, "Time difference can't be more than 400 days");
    return NULL;
  }

  switch (time_->tm_wday) {
  case 0:
    strlcpy(wday, "Sun", 4);
    break;
  case 1:
    strlcpy(wday, "Mon", 4);
    break;
  case 2:
    strlcpy(wday, "Tue", 4);
    break;
  case 3:
    strlcpy(wday, "Wed", 4);
    break;
  case 4:
    strlcpy(wday, "Thu", 4);
    break;
  case 5:
    strlcpy(wday, "Fri", 4);
    break;
  case 6:
    strlcpy(wday, "Sat", 4);
    break;

  default:
    yueah_log(Error, true, "Invalid day");
    return NULL;
  }

  switch (time_->tm_mon) {
  case 0:
    strlcpy(month, "Jan", 4);
    break;
  case 1:
    strlcpy(month, "Feb", 4);
    break;
  case 2:
    strlcpy(month, "Mar", 4);
    break;
  case 3:
    strlcpy(month, "Apr", 4);
    break;
  case 4:
    strlcpy(month, "May", 4);
    break;
  case 5:
    strlcpy(month, "Jun", 4);
    break;
  case 6:
    strlcpy(month, "Jul", 4);
    break;
  case 7:
    strlcpy(month, "Aug", 4);
    break;
  case 8:
    strlcpy(month, "Sep", 4);
    break;
  case 9:
    strlcpy(month, "Oct", 4);
    break;
  case 10:
    strlcpy(month, "Nov", 4);
    break;
  case 11:
    strlcpy(month, "Dec", 4);
    break;

  default:
    yueah_log(Error, true, "Invalid month");
    return NULL;
  }

  snprintf(date, 1024, "%s, %02d %s %04d %02d:%02d:%02d GMT", wday,
           time_->tm_mday, month, time_->tm_year + 1900, time_->tm_hour,
           time_->tm_min, time_->tm_sec);

  return date;
}

unsigned char *yueah_cookie_new(h2o_mem_pool_t *pool, const char *cookie_name,
                                const char *content, mem_t *out_len,
                                yueah_cookie_mask mask, ...) {
  mem_t header_size = 4096 + 32;
  unsigned char *cookie_header =
      h2o_mem_alloc_pool(pool, unsigned char *, header_size);

  yueah_same_site_t same_site = -1;
  mem_t max_age = 0;
  char *expires = NULL;
  char *domain = NULL;
  char *path = NULL;

  bool secure = false;
  bool http_only = false;

  va_list va;
  va_start(va, mask);

  if (mask & SAME_SITE)
    same_site = va_arg(va, yueah_same_site_t);

  if (mask & HTTP_ONLY)
    http_only = true;

  if (mask & SECURE)
    secure = true;

  if (mask & PATH)
    path = va_arg(va, char *);

  if (mask & EXPIRES) {
    mem_t expire_time = va_arg(va, uint64_t);
    mem_t now = time(NULL);
    mem_t expires_time = now + expire_time;

    expires = unix_time_to_date_header(expires_time);
  }

  if (mask & MAX_AGE) {
    max_age = va_arg(va, uint64_t);
    if (max_age > 34560000) {
      yueah_log(Error, true, "Max-Age must be less than 34560000");
      return NULL;
    }
  }

  if (mask & DOMAIN)
    domain = va_arg(va, char *);

  // This expects the content array ends with NULL

  mem_t encrypted_out_len = 0;
  mem_t cookie_contents_encoded_len = 0;

  unsigned char *cookie_contents = (unsigned char *)content;

  unsigned char *cookie_contents_encrypted = yueah_cookie_encrypt(
      pool, cookie_contents, strlen((const char *)cookie_contents),
      &encrypted_out_len);
  char *cookie_contents_encoded =
      yueah_base64_encode(pool, cookie_contents_encrypted, encrypted_out_len,
                          &cookie_contents_encoded_len);

  memcpy(cookie_header, cookie_name, strlen(cookie_name));
  memcpy(cookie_header + strlen(cookie_name), "=", 1);
  memcpy(cookie_header + strlen(cookie_name) + 1, cookie_contents_encoded,
         cookie_contents_encoded_len);
  memcpy(cookie_header + strlen(cookie_name) + 1 + cookie_contents_encoded_len,
         ";", 1);

  mem_t cookie_header_offset =
      strlen(cookie_name) + 1 + cookie_contents_encoded_len + 1;

  if (same_site > -1) {
    memcpy(cookie_header + cookie_header_offset, " SameSite=", 10);
    cookie_header_offset += 10;
    switch (same_site) {
    case LAX:
      memcpy(cookie_header + cookie_header_offset, "Lax;", 4);
      cookie_header_offset += 4;
      break;
    case STRICT:
      memcpy(cookie_header + cookie_header_offset, "Strict;", 7);
      cookie_header_offset += 7;
      break;
    case NONE:
      memcpy(cookie_header + cookie_header_offset, "None;", 5);
      cookie_header_offset += 5;
      break;

    default:
      memcpy(cookie_header + cookie_header_offset, "Lax;", 4);
      cookie_header_offset += 4;
      break;
    }
  }

  if (secure) {
    memcpy(cookie_header + cookie_header_offset, " Secure;", 8);
    cookie_header_offset += 8;
  }

  if (http_only) {
    memcpy(cookie_header + cookie_header_offset, " HttpOnly;", 10);
    cookie_header_offset += 10;
  }

  if (expires) {
    memcpy(cookie_header + cookie_header_offset, " Expires=", 9);
    cookie_header_offset += 9;
    memcpy(cookie_header + cookie_header_offset, expires, strlen(expires));
    cookie_header_offset += strlen(expires);
    memcpy(cookie_header + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (max_age > 0) {
    char fmt_buf[64];
    snprintf(fmt_buf, 64, "%lu", max_age);
    memcpy(cookie_header + cookie_header_offset, " Max-Age=", 9);
    cookie_header_offset += 9;
    memcpy(cookie_header + cookie_header_offset, fmt_buf, strlen(fmt_buf));
    cookie_header_offset += strlen(fmt_buf);
    memcpy(cookie_header + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (domain) {
    memcpy(cookie_header + cookie_header_offset, " Domain=", 8);
    cookie_header_offset += 8;
    memcpy(cookie_header + cookie_header_offset, domain, strlen(domain));
    cookie_header_offset += strlen(domain);
    memcpy(cookie_header + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (path) {
    memcpy(cookie_header + cookie_header_offset, " Path=", 7);
    cookie_header_offset += 7;
    memcpy(cookie_header + cookie_header_offset, path, strlen(path));
    cookie_header_offset += strlen(path);
    memcpy(cookie_header + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  va_end(va);
  memset(cookie_header + cookie_header_offset, 0, 1);

  *out_len = strlen((const char *)cookie_header);
  return cookie_header;
}

static char *parse_cookie(h2o_mem_pool_t *pool,
                          const unsigned char *cookie_header,
                          const char *cookie_name) {
  char format[4096] = {0};
  snprintf(format, 4096, "%s=", cookie_name);
  yueah_log_debug("header: %s", cookie_header);

  char *res = strstr((char *)cookie_header, format);
  if (!res) {
    yueah_log_error("Invalid cookie header");
    return NULL;
  }

  yueah_log_debug("cookie_content: %s", res);

  char *cookie_content = h2o_mem_alloc_pool(pool, char *, strlen(res) + 1);
  memcpy(cookie_content, res + strlen(format),
         strlen(res) - strlen(format) + 1);

  return cookie_content;
}

char *yueah_get_cookie_name(h2o_mem_pool_t *pool, h2o_iovec_t cookie_header) {
  if (!cookie_header.base)
    return NULL;

  char *after_cookie_name = strstr(cookie_header.base, "=");
  mem_t len = after_cookie_name - cookie_header.base;

  char *cookie_name = h2o_mem_alloc_pool(pool, char *, len + 1);
  memcpy(cookie_name, cookie_header.base, len);
  cookie_name[len] = '\0';

  return cookie_name;
}

unsigned char *yueah_get_cookie_content(h2o_mem_pool_t *pool,
                                        unsigned char *cookie_header,
                                        char *cookie_name, mem_t *out_len) {
  unsigned char *decrypted_content = NULL;
  char *content = parse_cookie(pool, cookie_header, cookie_name);
  if (!content) {
    yueah_log_error("Failed to parse cookie");
    return NULL;
  }

  mem_t decoded_content_len = 0;
  unsigned char *decoded_content = yueah_base64_decode(
      pool, content, strlen(content), 4096, &decoded_content_len);

  mem_t decrypted_len = 0;
  decrypted_content = yueah_cookie_decrypt(pool, decoded_content,
                                           decoded_content_len, &decrypted_len);
  if (!decrypted_content || decrypted_len < 10) {
    yueah_log_error("Failed to decrypt cookie");
    return NULL;
  }

  char *end = strchr((char *)decrypted_content, ';');
  if (end)
    *end = '\0';

  decrypted_len = (unsigned char *)end - decrypted_content;
  *out_len = decrypted_len;

  return decrypted_content;
}
