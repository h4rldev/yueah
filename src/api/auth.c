#include <string.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/db.h>
#include <yueah/hash.h>
#include <yueah/log.h>
#include <yueah/shared.h>
#include <yueah/uuid.h>

#include <api/auth.h>
#include <api/utils.h>

typedef enum {
  Poster,
  Admin,
  Banned,
} yueah_user_role_t;

typedef struct {
  char *username;
  mem_t username_len;
  char *userid;
  char *password_hash;
  mem_t password_hash_len;
  yueah_user_role_t role;
} yueah_user_t;

char *get_user_role(yueah_user_role_t role) {
  static char role_str[32];
  switch (role) {
  case Poster:
    memcpy(role_str, "poster", 6);
    role_str[6] = '\0';
    break;

  case Admin:
    memcpy(role_str, "admin", 5);
    role_str[5] = '\0';
    break;

  case Banned:
    memcpy(role_str, "banned", 6);
    role_str[6] = '\0';
    break;

  default:
    memcpy(role_str, "poster", 6);
    role_str[6] = '\0';
    break;
  }

  return role_str;
}

int insert_user(sqlite3 *conn, yueah_user_t *user) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO users (userid, username, password_hash, role) "
                    "VALUES (?, ?, ?, ?);";
  char *role = get_user_role(user->role);

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    return -1;
  }

  sqlite3_bind_text(stmt, 1, user->userid, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, user->username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, user->password_hash, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, role, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to insert user: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    return -1;
  }

  sqlite3_finalize(stmt);
  yueah_log_info("Inserted user: %s:%s", user->username, user->userid);

  return 0;
}

int post_register_form(h2o_handler_t *handler, h2o_req_t *req) {
  if (strcmp(req->method.base, "POST") != 0) {
    return generic_response(req, 405, "Method not allowed");
  }

  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  yueah_user_t *user = h2o_mem_alloc_pool(&req->pool, yueah_user_t, 1);
  sqlite3 *db;

  char *form_body = yueah_iovec_to_str(&req->pool, &req->entity);
  char **body = parse_post_body(&req->pool, form_body, req->entity.len);

  mem_t username_len = 0;
  mem_t password_len = 0;
  char *username = get_form_val(&req->pool, "username", body, &username_len);
  if (!username) {
    yueah_log_error("Failed to get username");
    return generic_response(req, 400, "Failed to get username");
  }

  char *password = get_form_val(&req->pool, "password", body, &password_len);
  if (!password) {
    yueah_log_error("Failed to get password");
    return generic_response(req, 400, "Failed to get password");
  }

  char *hash = hash_password(&req->pool, password, password_len);
  if (!hash) {
    yueah_log_error("Failed to hash password");
    return generic_response(req, 400, "Failed to hash password");
  }

  char *uuid = yueah_uuid_new(&req->pool);
  if (!uuid) {
    yueah_log_error("Failed to generate uuid");
    return generic_response(req, 400, "Failed to generate uuid");
  }

  user->username = username;
  user->username_len = username_len;
  user->userid = uuid;
  user->password_hash = hash;
  user->password_hash_len = password_len;
  user->role = Poster;

  if (yueah_db_connect(yueah_state->db_path, &db, READ | WRITE) != 0) {
    yueah_log_error("Failed to connect to db");
    return generic_response(req, 500, "Failed to connect to db");
  }

  if (insert_user(db, user) != 0)
    return generic_response(req, 500, "Failed to insert user");

  yueah_db_disconnect(db);

  return generic_response(req, 200, "Register successful!");
}
