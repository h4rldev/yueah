#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <h2o.h>
#include <sodium.h>

#include <yueah/base64.h>
#include <yueah/cookie.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/shared.h>
#include <yueah/string.h>

static yueah_string_t *unix_time_to_date_header(h2o_mem_pool_t *pool,
                                                u64 unix_time,
                                                yueah_error_t *error) {
  yueah_string_t *date = yueah_string_new(pool, NULL, 1024);

  char wday[4] = {0};
  char month[4] = {0};

  time_t time_container = unix_time;
  struct tm *time_ = gmtime(&time_container);
  time_t now = time(NULL);

  double diff_seconds = difftime(time_container, now);
  if (diff_seconds < 0) {
    *error = yueah_throw_error("Time shouldn't be in the past");
    return NULL;
  }

  if (diff_seconds >= 34560000) {
    *error = yueah_throw_error("Time difference can't be more than 400 days");
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
    *error = yueah_throw_error("Invalid day");
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
    *error = yueah_throw_error("Invalid month");
    return NULL;
  }

  u64 len =
      snprintf((char *)date->data, 1024, "%s, %02d %s %04d %02d:%02d:%02d GMT",
               wday, time_->tm_mday, month, time_->tm_year + 1900,
               time_->tm_hour, time_->tm_min, time_->tm_sec);
  date->len = len;

  *error = yueah_success(NULL);
  return date;
}

#define YUEAH_COOKIE_HEADER_LEN 4096
yueah_string_t *yueah_cookie_new(h2o_mem_pool_t *pool,
                                 const yueah_string_t *cookie_name,
                                 const yueah_string_t *content,
                                 yueah_error_t *error, yueah_cookie_mask mask,
                                 ...) {
  yueah_string_t *cookie_header =
      yueah_string_new(pool, NULL, YUEAH_COOKIE_HEADER_LEN);

  yueah_same_site_t same_site = -1;
  u64 max_age = 0;
  yueah_string_t *expires = NULL;
  yueah_string_t *domain = NULL;
  yueah_string_t *path = NULL;
  yueah_error_t cookie_error = yueah_success(NULL);

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
    u64 expire_time = va_arg(va, u64);
    u64 now = time(NULL);
    u64 expires_time = now + expire_time;

    expires = unix_time_to_date_header(pool, expires_time, &cookie_error);
    if (cookie_error.status != OK) {
      *error = cookie_error;
      return NULL;
    }
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

  va_end(va);

  const yueah_string_t *cookie_contents = content;
  yueah_string_t *cookie_contents_encoded =
      yueah_base64_encode(pool, cookie_contents, &cookie_error);
  if (cookie_error.status != OK) {
    *error = cookie_error;
    return NULL;
  }

  cookie_header = yueah_string_append(pool, cookie_name, YUEAH_STR("="));
  cookie_header =
      yueah_string_append(pool, cookie_header, cookie_contents_encoded);
  cookie_header = yueah_string_append(pool, cookie_header, YUEAH_STR(";"));

  if (same_site > -1) {
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" SameSite="));
    switch (same_site) {
    case LAX:
      cookie_header =
          yueah_string_append(pool, cookie_header, YUEAH_STR("Lax;"));
      break;
    case STRICT:
      cookie_header =
          yueah_string_append(pool, cookie_header, YUEAH_STR("Strict;"));
      break;
    case NONE:
      cookie_header =
          yueah_string_append(pool, cookie_header, YUEAH_STR("None;"));
      break;

    default:
      cookie_header =
          yueah_string_append(pool, cookie_header, YUEAH_STR("Lax;"));
      break;
    }
  }

  if (secure)
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" Secure;"));

  if (http_only)
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" HttpOnly;"));

  if (expires) {
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" Expires="));
    cookie_header = yueah_string_append(pool, cookie_header, expires);
    cookie_header = yueah_string_append(pool, cookie_header, YUEAH_STR(";"));
  }

  if (max_age > 0) {
    char fmt_buf[64];
    u64 fmt_buf_len = snprintf(fmt_buf, 64, "%lu;", max_age);
    yueah_string_t *max_age_str = yueah_string_new(pool, fmt_buf, fmt_buf_len);

    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" Max-Age="));
    cookie_header = yueah_string_append(pool, cookie_header, max_age_str);
  }

  if (domain) {
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" Domain="));
    cookie_header = yueah_string_append(pool, cookie_header, domain);
    cookie_header = yueah_string_append(pool, cookie_header, YUEAH_STR(";"));
  }

  if (path) {
    cookie_header =
        yueah_string_append(pool, cookie_header, YUEAH_STR(" Path="));
    cookie_header = yueah_string_append(pool, cookie_header, path);
    cookie_header = yueah_string_append(pool, cookie_header, YUEAH_STR(";"));
  }

  *error = yueah_success(NULL);
  return cookie_header;
}

static yueah_string_t *parse_cookie(h2o_mem_pool_t *pool,
                                    const yueah_string_t *cookie_header,
                                    const yueah_string_t *cookie_name,
                                    yueah_error_t *error) {
  cstr format[4096] = {0};
  cstr *cookie_name_cstr = yueah_string_to_cstr(pool, cookie_name);
  u64 format_len = snprintf(format, 4096, "%s=", cookie_name_cstr);

  cstr *cookie_header_cstr = yueah_string_to_cstr(pool, cookie_header);
  cstr *res = strstr(cookie_header_cstr, format);
  if (!res) {
    *error = yueah_throw_error("Invalid cookie header");
    return NULL;
  }

  yueah_string_t *cookie_content =
      yueah_string_new(pool, NULL, strlen(res) + 1);
  memcpy(cookie_content->data, res + format_len, strlen(res) - format_len);

  *error = yueah_success(NULL);
  return cookie_content;
}

bool yueah_cookie_name_exists(h2o_mem_pool_t *pool, h2o_iovec_t cookie_header,
                              const yueah_string_t *cookie_name,
                              yueah_error_t *error) {
  if (!cookie_header.base) {
    *error = yueah_throw_error("Cookie header is NULL");
    return NULL;
  }

  cstr *cookie_name_cstr = yueah_string_to_cstr(pool, cookie_name);
  cstr needle[1024];

  snprintf(needle, 1024, "%s=", cookie_name_cstr);

  char *after_cookie_name = strstr(cookie_header.base, needle);
  if (after_cookie_name == NULL) {
    *error = yueah_success(NULL);
    return false;
  }

  *error = yueah_success(NULL);
  return true;
}

yueah_string_t *yueah_get_cookie_content(h2o_mem_pool_t *pool,
                                         const yueah_string_t *cookie_header,
                                         const yueah_string_t *cookie_name,
                                         yueah_error_t *error) {
  yueah_error_t cookie_error = yueah_success(NULL);

  yueah_string_t *content =
      parse_cookie(pool, cookie_header, cookie_name, &cookie_error);
  if (!content || cookie_error.status != OK) {
    *error = cookie_error;
    return NULL;
  }

  return yueah_base64_decode(pool, content, 4096, error);
}

static int get_cookie_index(h2o_req_t *req, const yueah_string_t *cookie_name,
                            yueah_error_t *error) {
  int cookie_cursor = -1;
  int wanted_cookie_index = 0;
  yueah_error_t cookie_error = yueah_success(NULL);

  do {
    int cookie_index =
        h2o_find_header(&req->headers, H2O_TOKEN_COOKIE, cookie_cursor);
    if (cookie_index == -1) {
      yueah_log_debug("Didn't find cookie with the name %s", cookie_name);
      return -1;
    }

    h2o_header_t cookie_header = req->headers.entries[cookie_index];
    if (yueah_cookie_name_exists(&req->pool, cookie_header.value, cookie_name,
                                 &cookie_error)) {
      yueah_log_debug("Found cookie with the name %s at index %d, cursor %d",
                      cookie_name, cookie_index, cookie_cursor);
      wanted_cookie_index = cookie_index;
    } else if (cookie_error.status != OK) {
      *error = cookie_error;
      return -1;
    } else
      cookie_cursor++;

  } while (wanted_cookie_index == 0);

  return wanted_cookie_index;
}

yueah_string_t *yueah_req_get_cookie_content(h2o_req_t *req,
                                             const yueah_string_t *cookie_name,
                                             yueah_error_t *error) {
  yueah_error_t cookie_error = yueah_success(NULL);
  if (!cookie_name) {
    *error = yueah_throw_error("Cookie name is NULL");
    return NULL;
  }

  cstr cookie_content_buf[4096];
  cstr *cookie_name_cstr = yueah_string_to_cstr(&req->pool, cookie_name);
  u64 cookie_content_len = 0;

  int wanted_cookie_index = get_cookie_index(req, cookie_name, &cookie_error);
  if (cookie_error.status != OK) {
    *error = cookie_error;
    return NULL;
  }

  h2o_header_t cookie_header = req->headers.entries[wanted_cookie_index];
  cstr needle[1024];
  snprintf(needle, 1024, "%s=", cookie_name_cstr);

  size_t cookie_name_offset =
      h2o_strstr(cookie_header.value.base, cookie_header.value.len, needle,
                 strlen(needle));

  if (cookie_name_offset == SIZE_MAX) {
    yueah_log_error(
        "Failed to find cookie name in header despite being found previously");
    return NULL;
  }

  memcpy(cookie_content_buf, cookie_header.value.base + cookie_name_offset,
         cookie_header.value.len - cookie_name_offset);
  cookie_content_len = cookie_header.value.len - cookie_name_offset;

  size_t semi_colon_offset = 2;
  size_t semi_colon_end_offset =
      h2o_strstr(cookie_content_buf, cookie_content_len, ";", 1);
  if (semi_colon_end_offset == SIZE_MAX) {
    semi_colon_end_offset = 0;
    semi_colon_offset = 0;
  }

  cookie_content_len =
      cookie_content_len - semi_colon_end_offset - semi_colon_offset;

  *error = yueah_success(NULL);
  return yueah_string_new(&req->pool, cookie_content_buf, cookie_content_len);
}

yueah_error_t yueah_req_delete_cookie(h2o_req_t *req) {
  int cookie_index = h2o_find_header(&req->headers, H2O_TOKEN_COOKIE, -1);
  if (cookie_index == -1)
    return yueah_throw_error("Didn't find cookie");

  int new = h2o_delete_header(&req->headers, cookie_index);
  if (new < 0)
    return yueah_throw_error("Failed to delete cookie");

  return yueah_success(NULL);
}
