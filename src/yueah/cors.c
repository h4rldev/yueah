#include <h2o.h>

#include <api/utils.h>
#include <yueah/cors.h>
#include <yueah/log.h>
#include <yueah/shared.h>
#include <yueah/string.h>

int yueah_add_cors_headers(h2o_req_t *req, const yueah_cors_config_t *config) {
  if (config->allow_origin) {
    h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN,
        "Access-Control-Allow-Origin", YUEAH_SSTRLIT(config->allow_origin));
  } else {
    int headers_index = h2o_find_header(&req->headers, H2O_TOKEN_ORIGIN, -1);
    if (headers_index != -1)
      h2o_add_header(&req->pool, &req->res.headers,
                     H2O_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN,
                     "Access-Control-Allow-Origin",
                     req->headers.entries[headers_index].value.base,
                     req->headers.entries[headers_index].value.len);
  }

  if (config->allow_methods)
    h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_METHODS,
        "Access-Control-Allow-Methods", YUEAH_SSTRLIT(config->allow_methods));

  if (config->allow_headers)
    h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_HEADERS,
        "Access-Control-Allow-Headers", YUEAH_SSTRLIT(config->allow_headers));

  if (config->expose_headers)
    h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_EXPOSE_HEADERS,
        "Access-Control-Expose-Headers", YUEAH_SSTRLIT(config->expose_headers));

  if (config->allow_credentials)
    h2o_add_header(&req->pool, &req->res.headers,
                   H2O_TOKEN_ACCESS_CONTROL_ALLOW_CREDENTIALS,
                   "Access-Control-Allow-Credentials", "true", 4);
  else if (config->allow_credentials == false)
    h2o_add_header(&req->pool, &req->res.headers,
                   H2O_TOKEN_ACCESS_CONTROL_ALLOW_CREDENTIALS,
                   "Access-Control-Allow-Credentials", "false", 5);

  return 0;
}

int yueah_handle_options(h2o_req_t *req, const yueah_cors_config_t *config) {
  if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return generic_response(
        req, 500, "Developer goofed up: Misuse of yueah_handle_options()");

  yueah_add_cors_headers(req, config);

  req->res.status = 204;
  req->res.reason = "No Content";

  h2o_send_inline(req, YUEAH_STRLIT(""));
  return 0;
}
