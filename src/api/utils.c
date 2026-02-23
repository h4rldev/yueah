#include <ctype.h>
#include <string.h>

#include <h2o.h>
#include <yyjson.h>

#include <yueah/log.h>
#include <yueah/shared.h>

#include <api/utils.h>

char *get_res_reason(int status) {
  static char reason[1024] = {0};

  switch (status) {
  case 100:
    strlcpy(reason, "Continue", 1024);
    break;
  case 101:
    strlcpy(reason, "Switching Protocols", 1024);
    break;
  case 103:
    strlcpy(reason, "Early Hints", 1024);
    break;

  case 200:
    strlcpy(reason, "OK", 1024);
    break;
  case 201:
    strlcpy(reason, "Created", 1024);
    break;
  case 202:
    strlcpy(reason, "Accepted", 1024);
    break;
  case 203:
    strlcpy(reason, "Non-Authoritative Information", 1024);
    break;
  case 204:
    strlcpy(reason, "No Content", 1024);
    break;
  case 205:
    strlcpy(reason, "Reset Content", 1024);
    break;
  case 206:
    strlcpy(reason, "Partial Content", 1024);
    break;

  case 300:
    strlcpy(reason, "Multiple Choices", 1024);
    break;
  case 301:
    strlcpy(reason, "Moved Permanently", 1024);
    break;
  case 302:
    strlcpy(reason, "Found", 1024);
    break;
  case 303:
    strlcpy(reason, "See Other", 1024);
    break;
  case 304:
    strlcpy(reason, "Not Modified", 1024);
    break;
  case 307:
    strlcpy(reason, "Temporary Redirect", 1024);
    break;
  case 308:
    strlcpy(reason, "Permanent Redirect", 1024);
    break;

  case 400:
    strlcpy(reason, "Bad Request", 1024);
    break;
  case 401:
    strlcpy(reason, "Unauthorized", 1024);
    break;
  case 402:
    strlcpy(reason, "Payment Required", 1024);
    break;
  case 403:
    strlcpy(reason, "Forbidden", 1024);
    break;
  case 404:
    strlcpy(reason, "Not Found", 1024);
    break;
  case 405:
    strlcpy(reason, "Method Not Allowed", 1024);
    break;
  case 406:
    strlcpy(reason, "Not Acceptable", 1024);
    break;
  case 407:
    strlcpy(reason, "Proxy Authentication Required", 1024);
    break;
  case 408:
    strlcpy(reason, "Request Timeout", 1024);
    break;
  case 409:
    strlcpy(reason, "Conflict", 1024);
    break;
  case 410:
    strlcpy(reason, "Gone", 1024);
    break;
  case 411:
    strlcpy(reason, "Length Required", 1024);
    break;
  case 412:
    strlcpy(reason, "Precondition Failed", 1024);
    break;
  case 413:
    strlcpy(reason, "Payload Too Large", 1024);
    break;
  case 414:
    strlcpy(reason, "URI Too Long", 1024);
    break;
  case 415:
    strlcpy(reason, "Unsupported Media Type", 1024);
    break;
  case 416:
    strlcpy(reason, "Range Not Satisfiable", 1024);
    break;
  case 417:
    strlcpy(reason, "Expectation Failed", 1024);
    break;
  case 418:
    strlcpy(reason, "I'm a teapot", 1024);
    break;
  case 421:
    strlcpy(reason, "Misdirected Request", 1024);
    break;
  case 426:
    strlcpy(reason, "Upgrade Required", 1024);
    break;
  case 428:
    strlcpy(reason, "Precondition Required", 1024);
    break;
  case 429:
    strlcpy(reason, "Too Many Requests", 1024);
    break;
  case 431:
    strlcpy(reason, "Request Header Fields Too Large", 1024);
    break;
  case 451:
    strlcpy(reason, "Unavailable For Legal Reasons", 1024);
    break;

  case 500:
    strlcpy(reason, "Internal Server Error", 1024);
    break;
  case 501:
    strlcpy(reason, "Not Implemented", 1024);
    break;
  case 502:
    strlcpy(reason, "Bad Gateway", 1024);
    break;
  case 503:
    strlcpy(reason, "Service Unavailable", 1024);
    break;
  case 504:
    strlcpy(reason, "Gateway Timeout", 1024);
    break;
  case 505:
    strlcpy(reason, "HTTP Version Not Supported", 1024);
    break;
  case 506:
    strlcpy(reason, "Variant Also Negotiates", 1024);
    break;
  case 510:
    strlcpy(reason, "Not Extended", 1024);
    break;
  case 511:
    strlcpy(reason, "Network Authentication Required", 1024);
    break;
  }

  return reason;
}

static char *error_resp_to_json(h2o_mem_pool_t *pool, yyjson_mut_doc *doc,
                                yyjson_mut_val *root,
                                error_response_t *error_response) {
  yyjson_write_err err;
  char *json;

  yyjson_mut_val *status_key = yyjson_mut_str(doc, "status");
  yyjson_mut_val *message_key = yyjson_mut_str(doc, "message");

  yyjson_mut_val *status_val = yyjson_mut_int(doc, error_response->status);
  yyjson_mut_val *message_val = yyjson_mut_str(doc, error_response->message);

  yyjson_mut_obj_add(root, status_key, status_val);
  yyjson_mut_obj_add(root, message_key, message_val);

  yyjson_mut_doc_set_root(doc, root);

  mem_t out_len = 0;
  yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &out_len, &err);

  json = h2o_mem_alloc_pool(pool, char *, out_len);
  json = yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &out_len, &err);
  if (!json) {
    yueah_log(Error, true, "Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  return json;
}

char *get_form_val(h2o_mem_pool_t *pool, const char *key, char **input,
                   mem_t *val_len) {
  char *output = NULL;

  if (!key || !input || !val_len)
    return NULL;

  mem_t key_idx = 0;
  mem_t key_len = 0;

  for (mem_t i = 0; input[i] != NULL; i++) {
    key_len = strlen(key);
    if (strncmp(input[i], key, key_len) != 0)
      continue;

    key_idx = i;
  }

  char *val = input[key_idx] + key_len + 1;
  mem_t _val_len = strlen(val) + 1;
  output = h2o_mem_alloc_pool(pool, char, _val_len);
  memcpy(output, val, _val_len);

  *val_len = _val_len;
  return output;
}

void urldecode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if (*src == '%' && ((a = src[1]) && (b = src[2])) && isxdigit(a) &&
        isxdigit(b)) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';

      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';

      *dst++ = 16 * a + b;
      src += 3;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

/*
 * Parses the body of a post request
 *
 *
 * [pool]      Memory pool to allocate the array from
 *
 * [input]     The body of the post request
 *
 * [input_len] The length of the body
 *
 *
 * Returns an array of strings terminated with a null which is at the last byte
 * of the array
 */
char **parse_post_body(h2o_mem_pool_t *pool, const char *input,
                       const mem_t input_len) {

  if (!input)
    return NULL;

  if (input_len == 0)
    return NULL;

  char *decoded_input = h2o_mem_alloc_pool(pool, char, input_len + 1);
  urldecode(decoded_input, input);

  mem_t array_len = 1;
  int part_idx = 0;

  for (mem_t i = 0; i < input_len; i++)
    if (decoded_input[i] == '&')
      array_len = array_len + 2;

  char *ptr = decoded_input;
  char **res = h2o_mem_alloc_pool(pool, char *, array_len);
  char *next = strtok(ptr, "&");
  while (next != NULL) {
    mem_t part_len = strlen(next);

    res[part_idx] = h2o_mem_alloc_pool(pool, char, part_len + 1);
    memcpy(res[part_idx], next, part_len);
    res[part_idx][part_len] = '\0';

    part_idx++;
    next = strtok(NULL, "&");
  }

  res[array_len - 1] = NULL;
  return res;
}

int generic_response(h2o_req_t *req, int status, const char *message) {
  h2o_generator_t generator = {NULL, NULL};
  error_response_t *error_response =
      h2o_mem_alloc_pool(&req->pool, error_response_t, 1);

  error_response->status = status;
  error_response->message = h2o_mem_alloc_pool(&req->pool, char *, 1024);
  error_response->message = (char *)message;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  char *body = error_resp_to_json(&req->pool, doc, root, error_response);
  h2o_iovec_t body_iovec = h2o_strdup(&req->pool, body, strlen(body));

  req->res.status = status;
  req->res.reason = get_res_reason(status);

  h2o_start_response(req, &generator);
  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL,
                 H2O_STRLIT("application/json; charset=utf-8"));
  h2o_send(req, &body_iovec, 1, 1);

  return 0;
}
