#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <h2o.h>
#include <sodium.h>

#include <base64.h>
#include <cookie.h>
#include <log.h>
#include <shared.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YUEAH_COOKIE_CIPHER_LEN (4096 + crypto_secretbox_MACBYTES)

static char *collect_contents(char **contents) {
  static char collection[4096] = {0};
  char intermediate[4096] = {0};

  while (*contents) {
    snprintf(intermediate, 4096, "%s; ", *contents);
    strlcat(collection, intermediate, 4096);
    contents++;
  }

  return collection;
}

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
    yueah_log(
        Error, true,
        "Invalid YUEAH_AES_KEY, too short, generating one, check key.txt");
    if (generate_yueah_key() < 0)
      return NULL;

    return NULL;
  }

  memcpy(key, key_buf, crypto_secretbox_KEYBYTES);

  randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);
  /*
    yueah_log(Debug, true, "nonce:");
    #ifdef YUEAH_DEBUG
      for (mem_t i = 0; i < crypto_secretbox_NONCEBYTES; i++)
        printf("%02x ", nonce[i]);
      puts("");
    #endif
  */

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

  /*
    yueah_log(Debug, true, "pulled nonce:");
  #ifdef YUEAH_DEBUG
    for (mem_t i = 0; i < crypto_secretbox_NONCEBYTES; i++)
      printf("%02x ", nonce[i]);
    puts("");
  #endif

    yueah_log(Debug, true, "pulled ciphertext:");
  #ifdef YUEAH_DEBUG
    for (mem_t i = 0; i < bin_len - crypto_secretbox_NONCEBYTES; i++)
      printf("%02x ", ciphertext[i]);
    puts("");
  #endif
  */
  rc = crypto_secretbox_open_easy(
      result, ciphertext, bin_len - crypto_secretbox_NONCEBYTES, nonce, key);
  if (rc != 0) {
    yueah_log(Error, true, "Failed to decrypt content, code %d", rc);
    return NULL;
  }

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

char *yueah_cookie_new(h2o_mem_pool_t *pool, const char *cookie_name,
                       char **contents, yueah_cookie_mask mask, ...) {
  mem_t header_size = 4096 + 32;
  char *cookie_header = h2o_mem_alloc_pool(pool, char *, header_size);

  yueah_same_site_t same_site = -1;
  mem_t max_age = -1;
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

  if (mask & EXPIRES)
    expires = unix_time_to_date_header(va_arg(va, uint64_t));

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
  char *cookie_contents = collect_contents(contents);

  mem_t encrypted_out_len = 0;
  mem_t base64_encode_out_len = 0;

  unsigned char *cookie_contents_encrypted =
      yueah_cookie_encrypt(pool, (unsigned char *)cookie_contents,
                           strlen(cookie_contents), &encrypted_out_len);
  char *cookie_contents_encoded =
      yueah_base64_encode(pool, cookie_contents_encrypted, encrypted_out_len,
                          &base64_encode_out_len);

  snprintf(cookie_header, header_size, "Set-Cookie: %s=%s;", cookie_name,
           cookie_contents_encoded);

  if (same_site > -1) {
    strlcat(cookie_header, " SameSite=", header_size);
    switch (same_site) {
    case LAX:
      strlcat(cookie_header, "Lax;", header_size);
      break;
    case STRICT:
      strlcat(cookie_header, "Strict;", header_size);
      break;
    case NONE:
      strlcat(cookie_header, "None;", header_size);
      break;

    default:
      strlcat(cookie_header, "Lax;", header_size);
      break;
    }
  }

  if (secure)
    strlcat(cookie_header, " Secure;", header_size);

  if (http_only)
    strlcat(cookie_header, " HttpOnly;", header_size);

  if (expires) {
    strlcat(cookie_header, " Expires=", header_size);
    strlcat(cookie_header, expires, header_size);
    strlcat(cookie_header, ";", header_size);
  }

  if (max_age) {
    char fmt_buf[64];
    snprintf(fmt_buf, 64, "%lu", max_age);
    strlcat(cookie_header, " Max-Age=", header_size);
    strlcat(cookie_header, fmt_buf, header_size);
    strlcat(cookie_header, ";", header_size);
  }

  if (domain) {
    strlcat(cookie_header, " Domain=", header_size);
    strlcat(cookie_header, domain, header_size);
    strlcat(cookie_header, ";", header_size);
  }

  if (path) {
    strlcat(cookie_header, " Path=", header_size);
    strlcat(cookie_header, path, header_size);
    strlcat(cookie_header, ";", header_size);
  }

  va_end(va);
  return cookie_header;
}
