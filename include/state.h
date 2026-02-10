#ifndef YUEAH_STATE_H
#define YUEAH_STATE_H

#include <h2o.h>

typedef struct {
  char *db_path;
} yueah_state_t;

typedef struct {
  h2o_handler_t super;
  yueah_state_t *state;
} yueah_handler_t;

#endif // !YUEAH_STATE_H
