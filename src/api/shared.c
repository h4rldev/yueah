#include <string.h>

#include <api/shared.h>
#include <log.h>
#include <mem.h>

#include <h2o.h>
#include <yyjson.h>

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

int error_response(h2o_req_t *req, int status, const char *message) {
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
