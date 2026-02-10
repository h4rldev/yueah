#ifndef YUEAH_API_SHARED_H
#define YUEAH_API_SHARED_H

#include <h2o.h>

typedef struct {
  char *message;
  int status;
} error_response_t;

int json_response(h2o_req_t *req, void *data, size_t len);
int error_response(h2o_req_t *req, int status, const char *message);

#endif // !YUEAH_API_SHARED_H
