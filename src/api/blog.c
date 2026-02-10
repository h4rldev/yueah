#include <api/blog.h>
#include <api/shared.h>

#include <db.h>
#include <h2o.h>
#include <log.h>
#include <sqlite3.h>
#include <state.h>

int blog_not_found(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;

  sqlite3 *db;

  if (db_connect(yueah_state->db_path, &db, READ | WRITE) != 0) {
    yueah_log(Error, true, "failed to init db");
    return -1;
  }

  char *html_buffer = h2o_mem_alloc_pool(&req->pool, char *, 1024);
  bool exists =
      sqlite3_table_column_metadata(db, NULL, "posts", NULL, NULL, NULL, NULL,
                                    NULL, NULL) == SQLITE_OK;

  snprintf(html_buffer, 1024, "Hello!! posts exists: %s",
           exists ? "true" : "false");

  db_disconnect(db);

  return error_response(req, 404, html_buffer);
}
