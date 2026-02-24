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
  char *userid;
  char *password_hash;
  char *role;
} yueah_user_t;

typedef enum {
  ConstraintSuccess,
  UserID,
  Username,
} yueah_constraint_err_t;

typedef enum {
  SqlSuccess,
  StmtPrepare,
  ConstraintErr,
} yueah_sql_err_type_t;

typedef struct {
  yueah_sql_err_type_t err_type;
  yueah_constraint_err_t constraint_err;
} yueah_sql_err_t;

static yueah_constraint_err_t get_constraint_err(const char *error_msg) {
  if (strstr(error_msg, "UNIQUE constraint failed: users.username") != NULL)
    return Username;

  if (strstr(error_msg, "UNIQUE constraint failed: users.userid") != NULL)
    return UserID;

  return ConstraintSuccess;
}

static int insert_user(sqlite3 *conn, yueah_user_t *user,
                       yueah_sql_err_t *err) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO users (userid, username, password_hash, role) "
                    "VALUES (?, ?, ?, ?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = StmtPrepare, 0};
    return -1;
  }

  sqlite3_bind_text(stmt, 1, user->userid, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, user->username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, user->password_hash, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, user->role, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to insert user: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    if (rc == SQLITE_CONSTRAINT) {
      yueah_constraint_err_t constraint_err =
          get_constraint_err(sqlite3_errmsg(conn));
      *err = (yueah_sql_err_t){.err_type = ConstraintErr,
                               .constraint_err = constraint_err};
    }

    return -1;
  }

  sqlite3_finalize(stmt);
  yueah_log_info("Inserted user: %s:%s", user->username, user->userid);

  return 0;
}

int post_register_form(h2o_handler_t *handler, h2o_req_t *req) {
  if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("POST")))
    return generic_response(req, 405, "Method not allowed");

  int header_index = h2o_find_header(&req->headers, H2O_TOKEN_CONTENT_TYPE, -1);
  if (header_index == -1) {
    yueah_log_error("Failed to find header for Content-Type");
    return generic_response(req, 400, "Failed to find header for Content-Type");
  }

  if (!h2o_memis(req->headers.entries[header_index].value.base,
                 req->headers.entries[header_index].value.len,
                 H2O_STRLIT("application/x-www-form-urlencoded"))) {
    yueah_log_error("Content-Type is not application/x-www-form-urlencoded");
    return generic_response(
        req, 400, "Content-Type is not application/x-www-form-urlencoded");
  }

  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  yueah_user_t *user = h2o_mem_alloc_pool(&req->pool, yueah_user_t, 1);
  sqlite3 *db;

  char *form_body = yueah_iovec_to_str(&req->pool, &req->entity);
  char **body = parse_post_body(&req->pool, form_body, req->entity.len);

  user->username = get_form_val(&req->pool, "username", body);
  if (!user->username) {
    yueah_log_error("Failed to get username");
    return generic_response(req, 400, "Failed to get username");
  }

  char *password = get_form_val(&req->pool, "password", body);
  if (!password) {
    yueah_log_error("Failed to get password");
    return generic_response(req, 400, "Failed to get password");
  }

  user->password_hash = hash_password(&req->pool, password, strlen(password));
  if (!user->password_hash) {
    yueah_log_error("Failed to hash password");
    return generic_response(req, 400, "Failed to hash password");
  }

  char *uuid = yueah_uuid_new(&req->pool);
  if (!uuid) {
    yueah_log_error("Failed to generate uuid");
    return generic_response(req, 400, "Failed to generate uuid");
  }

  user->userid = uuid;
  user->role = yueah_strdup(&req->pool, "poster", 7);

  if (yueah_db_connect(yueah_state->db_path, &db, READ | WRITE) != 0) {
    yueah_log_error("Failed to connect to db");
    return generic_response(req, 500, "Failed to connect to db");
  }

  yueah_sql_err_t err = {0};
  if (insert_user(db, user, &err) != 0) {
    char *err_msg = h2o_mem_alloc_pool(&req->pool, char, 1024);
    int status = 500;

    switch (err.err_type) {
    case StmtPrepare:
      memcpy(err_msg, "Failed to prepare SQL statement", 25);
      break;
    case ConstraintErr:
      switch (err.constraint_err) {
      case Username:
        snprintf(err_msg, 1024, "User %s already exists", user->username);
        status = 400;
        break;
      case UserID:
        memcpy(err_msg, "UserID already exists (this should never happen)", 48);
        break;
      default:
        memcpy(err_msg, "Unknown error", 13);
        break;
      }
      break;
    default:
      memcpy(err_msg, "Unknown error", 13);
      break;
    }

    return generic_response(req, status, err_msg);
  }

  if (yueah_db_disconnect(db) != 0) {
    yueah_log_error("Failed to disconnect from db");
    return generic_response(req, 500, "Failed to disconnect from db");
  }

  char *message = h2o_mem_alloc_pool(&req->pool, char, 1024);
  snprintf(message, 1024, "Register of %s:%s successful", user->username,
           user->userid);

  return generic_response(req, 200, message);
}
