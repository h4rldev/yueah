#include <api/blog.h>

int not_found(h2o_handler_t *handler, h2o_req_t *req) {
  TODO("not_found");

  h2o_generator_t generator = {NULL, NULL};

  // h2o_iovec_t body = h2o_strdup(&req->pool, html_buffer,
  // strlen(html_buffer));

  req->res.status = 404;
  req->res.reason = "Not Found";
  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL,
                 H2O_STRLIT("text/html; charset=utf-8"));
  h2o_start_response(req, &generator);
  // h2o_send(req, &body, 1, 1);

  return 0;
}
