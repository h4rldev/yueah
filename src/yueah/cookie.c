#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/cookie.h>
#include <yueah/log.h>
#include <yueah/shared.h>
#include <yueah/string.h>

static yueah_string_t *unix_time_to_date_header(h2o_mem_pool_t *pool,
                                                u64 unix_time) {
  yueah_string_t *date = yueah_string_new(pool, NULL, 1024);

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

  u64 len =
      snprintf((char *)date->data, 1024, "%s, %02d %s %04d %02d:%02d:%02d GMT",
               wday, time_->tm_mday, month, time_->tm_year + 1900,
               time_->tm_hour, time_->tm_min, time_->tm_sec);
  date->len = len;
  return date;
}

#define YUEAH_COOKIE_HEADER_LEN 4096
yueah_string_t *yueah_cookie_new(h2o_mem_pool_t *pool,
                                 const yueah_string_t *cookie_name,
                                 const yueah_string_t *content,
                                 yueah_cookie_mask mask, ...) {
  yueah_string_t *cookie_header =
      yueah_string_new(pool, NULL, YUEAH_COOKIE_HEADER_LEN);

  yueah_same_site_t same_site = -1;
  u64 max_age = 0;
  yueah_string_t *expires = NULL;
  yueah_string_t *domain = NULL;
  yueah_string_t *path = NULL;

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
    path = va_arg(va, yueah_string_t *);

  if (mask & EXPIRES) {
    u64 expire_time = va_arg(va, uint64_t);
    u64 now = time(NULL);
    u64 expires_time = now + expire_time;

    expires = unix_time_to_date_header(pool, expires_time);
  }

  if (mask & MAX_AGE) {
    max_age = va_arg(va, u64);
    if (max_age > 34560000) {
      yueah_log(Error, true, "Max-Age must be less than 34560000");
      return NULL;
    }
  }

  if (mask & DOMAIN)
    domain = va_arg(va, yueah_string_t *);

  const yueah_string_t *cookie_contents = content;

  yueah_string_t *cookie_contents_encoded =
      yueah_base64_encode(pool, cookie_contents);

  memcpy(cookie_header->data, cookie_name->data, cookie_name->len);
  memcpy(cookie_header->data + cookie_name->len, "=", 1);
  memcpy(cookie_header->data + cookie_name->len + 1,
         cookie_contents_encoded->data, cookie_contents_encoded->len);
  memcpy(cookie_header->data + cookie_name->len + 1 +
             cookie_contents_encoded->len,
         ";", 1);

  u64 cookie_header_offset =
      cookie_name->len + 1 + cookie_contents_encoded->len + 1;

  if (same_site > -1) {
    memcpy(cookie_header->data + cookie_header_offset, " SameSite=", 10);
    cookie_header_offset += 10;
    switch (same_site) {
    case LAX:
      memcpy(cookie_header->data + cookie_header_offset, "Lax;", 4);
      cookie_header_offset += 4;
      break;
    case STRICT:
      memcpy(cookie_header->data + cookie_header_offset, "Strict;", 7);
      cookie_header_offset += 7;
      break;
    case NONE:
      memcpy(cookie_header->data + cookie_header_offset, "None;", 5);
      cookie_header_offset += 5;
      break;

    default:
      memcpy(cookie_header->data + cookie_header_offset, "Lax;", 4);
      cookie_header_offset += 4;
      break;
    }
  }

  if (secure) {
    memcpy(cookie_header->data + cookie_header_offset, " Secure;", 8);
    cookie_header_offset += 8;
  }

  if (http_only) {
    memcpy(cookie_header->data + cookie_header_offset, " HttpOnly;", 10);
    cookie_header_offset += 10;
  }

  if (expires) {
    memcpy(cookie_header->data + cookie_header_offset, " Expires=", 9);
    cookie_header_offset += 9;
    memcpy(cookie_header->data + cookie_header_offset, expires->data,
           expires->len);
    cookie_header_offset += expires->len;
    memcpy(cookie_header->data + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (max_age > 0) {
    char fmt_buf[64];
    u64 fmt_buf_len = snprintf(fmt_buf, 64, "%lu", max_age);
    memcpy(cookie_header->data + cookie_header_offset, " Max-Age=", 9);
    cookie_header_offset += 9;
    memcpy(cookie_header->data + cookie_header_offset, fmt_buf, fmt_buf_len);
    cookie_header_offset += fmt_buf_len;
    memcpy(cookie_header->data + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (domain) {
    memcpy(cookie_header->data + cookie_header_offset, " Domain=", 8);
    cookie_header_offset += 8;
    memcpy(cookie_header->data + cookie_header_offset, domain->data,
           domain->len);
    cookie_header_offset += domain->len;
    memcpy(cookie_header->data + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  if (path) {
    memcpy(cookie_header->data + cookie_header_offset, " Path=", 7);
    cookie_header_offset += 7;
    memcpy(cookie_header->data + cookie_header_offset, path->data, path->len);
    cookie_header_offset += path->len;
    memcpy(cookie_header + cookie_header_offset, ";", 1);
    cookie_header_offset += 1;
  }

  va_end(va);
  memset(cookie_header + cookie_header_offset, 0, 1);

  cookie_header->len = cookie_header_offset;
  return cookie_header;
}

static yueah_string_t *parse_cookie(h2o_mem_pool_t *pool,
                                    const yueah_string_t *cookie_header,
                                    const yueah_string_t *cookie_name) {
  cstr format[4096] = {0};
  cstr *cookie_name_cstr = yueah_string_to_cstr(pool, cookie_name);
  u64 format_len = snprintf(format, 4096, "%s=", cookie_name_cstr);

  cstr *cookie_header_cstr = yueah_string_to_cstr(pool, cookie_header);
  cstr *res = strstr(cookie_header_cstr, format);
  if (!res) {
    yueah_log_error("Invalid cookie header");
    return NULL;
  }

  yueah_string_t *cookie_content =
      yueah_string_new(pool, NULL, strlen(res) + 1);
  memcpy(cookie_content->data, res + format_len, strlen(res) - format_len);

  return cookie_content;
}

bool yueah_cookie_name_exists(h2o_mem_pool_t *pool, h2o_iovec_t cookie_header,
                              const yueah_string_t *cookie_name) {
  if (!cookie_header.base)
    return NULL;

  cstr *cookie_name_cstr = yueah_string_to_cstr(pool, cookie_name);
  cstr needle[1024];

  snprintf(needle, 1024, "%s=", cookie_name_cstr);

  char *after_cookie_name = strstr(cookie_header.base, needle);
  if (after_cookie_name == NULL)
    return false;

  return true;
}

yueah_string_t *yueah_get_cookie_content(h2o_mem_pool_t *pool,
                                         const yueah_string_t *cookie_header,
                                         const yueah_string_t *cookie_name) {
  yueah_string_t *content = parse_cookie(pool, cookie_header, cookie_name);
  if (!content) {
    yueah_log_error("Failed to parse cookie");
    return NULL;
  }

  return yueah_base64_decode(pool, content, 4096);
}
