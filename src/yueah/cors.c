#include <h2o.h>

#include <yueah/cors.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/response.h>
#include <yueah/shared.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_error_t yueah_add_cors_headers(h2o_req_t *req,
                                     const yueah_cors_config_t *config) {
  if (config->allow_origin) {
    ssize_t idx = h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN,
        "Access-Control-Allow-Origin", YUEAH_SSTRLIT(config->allow_origin));
    if (idx < 0)
      return yueah_throw_error("Failed to add header for allow origin");
  } else {
    int headers_index = h2o_find_header(&req->headers, H2O_TOKEN_ORIGIN, -1);
    if (headers_index != -1) {
      ssize_t idx = h2o_add_header(
          &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN,
          "Access-Control-Allow-Origin",
          req->headers.entries[headers_index].value.base,
          req->headers.entries[headers_index].value.len);
      if (idx < 0)
        return yueah_throw_error("Failed to add header for allow origin");
    }
  }

  if (config->allow_methods)
    h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_METHODS,
        "Access-Control-Allow-Methods", YUEAH_SSTRLIT(config->allow_methods));

  if (config->allow_headers) {
    ssize_t idx = h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_ALLOW_HEADERS,
        "Access-Control-Allow-Headers", YUEAH_SSTRLIT(config->allow_headers));
    if (idx < 0)
      return yueah_throw_error("Failed to add header for allow headers");
  }

  if (config->expose_headers) {
    ssize_t idx = h2o_add_header(
        &req->pool, &req->res.headers, H2O_TOKEN_ACCESS_CONTROL_EXPOSE_HEADERS,
        "Access-Control-Expose-Headers", YUEAH_SSTRLIT(config->expose_headers));
    if (idx < 0)
      return yueah_throw_error("Failed to add header for expose headers");
  }

  if (config->allow_credentials) {
    ssize_t idx = h2o_add_header(&req->pool, &req->res.headers,
                                 H2O_TOKEN_ACCESS_CONTROL_ALLOW_CREDENTIALS,
                                 "Access-Control-Allow-Credentials", "true", 4);
    if (idx < 0)
      return yueah_throw_error("Failed to add header for allow credentials");
  } else if (config->allow_credentials == false) {
    ssize_t idx =
        h2o_add_header(&req->pool, &req->res.headers,
                       H2O_TOKEN_ACCESS_CONTROL_ALLOW_CREDENTIALS,
                       "Access-Control-Allow-Credentials", "false", 5);
    if (idx < 0)
      return yueah_throw_error("Failed to add header for allow credentials");
  }

  return yueah_success(NULL);
}

int yueah_handle_options(h2o_req_t *req, const yueah_cors_config_t *config) {
  h2o_mem_pool_t *pool = &req->pool;

  if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_generic_response(
        req, 500,
        YUEAH_STR("Developer goofed up: Misuse of yueah_handle_options()"));

  yueah_add_cors_headers(req, config);

  req->res.status = 204;
  req->res.reason = "No Content";

  h2o_send_inline(req, YUEAH_STRLIT(""));
  return 0;
}
