
#include <h2o.h>
#include <sqlite3.h>

#include <yueah/db.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/response.h>
#include <yueah/shared.h>
#include <yueah/string.h>
#include <yueah/types.h>

#include <api/blog.h>

int blog_not_found(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  yueah_error_t error = yueah_success(NULL);
  h2o_mem_pool_t *pool = &req->pool;

  sqlite3 *db;

  error = yueah_db_connect(pool, yueah_state->db_path, &db, READ | WRITE);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to connect to db"));
  }

  yueah_string_t *html_buffer = yueah_string_new(&req->pool, NULL, 1024);
  bool exists =
      sqlite3_table_column_metadata(db, NULL, "posts", NULL, NULL, NULL, NULL,
                                    NULL, NULL) == SQLITE_OK;

  u64 html_buffer_len =
      snprintf((char *)html_buffer->data, 1024, "Hello!! posts exists: %s",
               exists ? "true" : "false");

  error = yueah_db_disconnect(db);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to disconnect from db"));
  }

  html_buffer->len = html_buffer_len;
  return yueah_generic_response(req, 404, html_buffer);
}
