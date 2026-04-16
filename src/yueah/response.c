#include <h2o.h>
#include <yyjson.h>

#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/response.h>
#include <yueah/string.h>
#include <yueah/types.h>
#include <yueah/url.h>

static char *get_res_reason(int status) {
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

static yueah_string_t *
error_resp_to_json(h2o_mem_pool_t *pool, yyjson_mut_doc *doc,
                   yyjson_mut_val *root, yueah_error_response_t *error_response,
                   yueah_error_t *error) {
  yyjson_write_err err;
  char *json;

  yyjson_mut_val *status_key = yyjson_mut_str(doc, "status");
  yyjson_mut_val *message_key = yyjson_mut_str(doc, "message");

  yyjson_mut_val *status_val = yyjson_mut_int(doc, error_response->status);
  yyjson_mut_val *message_val = yyjson_mut_str(doc, error_response->message);

  yyjson_mut_obj_add(root, status_key, status_val);
  yyjson_mut_obj_add(root, message_key, message_val);

  yyjson_mut_doc_set_root(doc, root);

  u64 out_len = 0;
  yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &out_len, &err);

  json = h2o_mem_alloc_pool(pool, char *, out_len);
  json = yyjson_mut_write_opts(doc, YYJSON_WRITE_NOFLAG, NULL, &out_len, &err);
  if (!json) {
    *error = yueah_throw_error("Error writing json: %d:%s", err.code, err.msg);
    return NULL;
  }

  return yueah_string_new(pool, json, out_len);
}

yueah_string_array_t *yueah_parse_form_body(h2o_mem_pool_t *pool,
                                            const yueah_string_t *form_body,
                                            yueah_error_t *error) {
  if (!form_body) {
    *error = yueah_throw_error("Form body is NULL");
    return NULL;
  }

  cstr *decoded_input = h2o_mem_alloc_pool(pool, char, form_body->len + 1);
  cstr *input_cstr = yueah_string_to_cstr(pool, form_body);
  yueah_urldecode(decoded_input, input_cstr);

  u64 array_len = 1;
  int part_idx = 0;

  for (u64 i = 0; decoded_input[i] != '\0'; i++)
    if (decoded_input[i] == '&')
      array_len++;

  char *ptr = decoded_input;
  yueah_string_t **res = h2o_mem_alloc_pool(pool, yueah_string_t, array_len);
  char *next = strtok(ptr, "&");
  while (next != NULL) {
    u64 part_len = strlen(next);

    res[part_idx] = yueah_string_new(pool, next, part_len);

    part_idx++;
    next = strtok(NULL, "&");
  }

  yueah_string_array_t *string_array =
      h2o_mem_alloc_pool(pool, yueah_string_array_t, 1);
  string_array->strings = res;
  string_array->len = array_len;

  *error = yueah_success(NULL);
  return string_array;
}

yueah_string_t *yueah_get_form_val(h2o_mem_pool_t *pool,
                                   const yueah_string_t *key,
                                   const yueah_string_array_t *form_data,
                                   yueah_error_t *error) {
  if (!key) {
    *error = yueah_throw_error("Key is NULL");
    return NULL;
  }

  if (!form_data) {
    *error = yueah_throw_error("Form data is NULL");
    return NULL;
  }

  u64 key_idx = 0;
  for (u64 i = 0; i < form_data->len; i++) {
    if (memcmp(form_data->strings[i]->data, key->data, key->len) != 0)
      continue;

    key_idx = i;
  }

  cstr *val = (cstr *)form_data->strings[key_idx]->data + key->len + 1;
  u64 val_len = form_data->strings[key_idx]->len - key->len - 1;
  return yueah_string_new(pool, val, val_len);
}

int yueah_generic_response(h2o_req_t *req, u16 status,
                           const yueah_string_t *message) {
  h2o_generator_t generator = {NULL, NULL};
  h2o_mem_pool_t *pool = &req->pool;
  yueah_error_t error = yueah_success(NULL);

  yueah_error_response_t *error_response =
      h2o_mem_alloc_pool(&req->pool, yueah_error_response_t, 1);

  error_response->status = status;
  error_response->message = yueah_string_to_cstr(pool, message);

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yueah_string_t *body =
      error_resp_to_json(pool, doc, root, error_response, &error);
  if (error.status != OK) {
    yueah_print_error(error);
    return -1;
  }

  h2o_iovec_t *body_iovec = yueah_string_to_iovec(&req->pool, body);

  req->res.status = status;
  req->res.reason = get_res_reason(status);

  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL,
                 H2O_STRLIT("application/json; charset=utf-8"));

  h2o_start_response(req, &generator);
  h2o_send(req, body_iovec, 1, 1);

  return 0;
}
