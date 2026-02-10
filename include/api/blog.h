#ifndef YUEAH_API_BLOG_H
#define YUEAH_API_BLOG_H

#include <h2o.h>

#define TODO(message)                                                          \
  do {                                                                         \
    fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message);         \
    abort();                                                                   \
  } while (0)

int blog_not_found(h2o_handler_t *handler, h2o_req_t *req);

#endif // !YUEAH_API_BLOG_H
