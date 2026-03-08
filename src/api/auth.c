#include "h2o/header.h"
#include "h2o/memory.h"
#include <string.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/cookie.h>
#include <yueah/cors.h>
#include <yueah/db.h>
#include <yueah/hash.h>
#include <yueah/jwt.h>
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
  Fetch,
  IncompleteTable,
  NotFound,
  Insert,
  StmtPrepare,
  Bind,
  ConstraintErr,
  TokenIsBlacklisted,
  SqlSuccess,
} yueah_sql_err_type_t;

typedef struct {
  yueah_sql_err_type_t err_type;
  yueah_constraint_err_t constraint_err;
} yueah_sql_err_t;

typedef enum {
  MethodNotAllowed,
  FailedToFindContentType,
  WrongContentType,
  Success,
} yueah_verify_err_t;
static yueah_verify_err_t verify_headers(h2o_req_t *req, char *method,
                                         mem_t method_len, char *content_type,
                                         mem_t content_type_len) {
  if (!h2o_memis(req->method.base, req->method.len, method, method_len))
    return MethodNotAllowed;

  if (content_type != NULL) {
    int header_index =
        h2o_find_header(&req->headers, H2O_TOKEN_CONTENT_TYPE, -1);
    if (header_index == -1)
      return FailedToFindContentType;

    if (!h2o_memis(req->headers.entries[header_index].value.base,
                   req->headers.entries[header_index].value.len, content_type,
                   content_type_len))
      return WrongContentType;
  }

  return Success;
}

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

static int get_user_userid(h2o_mem_pool_t *pool, sqlite3 *conn,
                           const char *userid, yueah_user_t **user,
                           yueah_sql_err_t *err) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT * FROM users WHERE userid = ?;";
  yueah_user_t *user_buf = h2o_mem_alloc_pool(pool, yueah_user_t, 1);

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = StmtPrepare, 0};
    return -1;
  }

  if (sqlite3_column_count(stmt) != 6) {
    yueah_log_error("Failed to get user: wrong number of columns");
    yueah_log_error("SQL: %s", sql);
    yueah_log_error("SQLITE_COLUMN_COUNT: %d", sqlite3_column_count(stmt));
    *err = (yueah_sql_err_t){IncompleteTable, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, userid, -1, SQLITE_STATIC);

  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    unsigned char *gotten_userid =
        (unsigned char *)sqlite3_column_text(stmt, 1);
    unsigned char *username = (unsigned char *)sqlite3_column_text(stmt, 2);
    unsigned char *password_hash =
        (unsigned char *)sqlite3_column_text(stmt, 3);
    unsigned char *role = (unsigned char *)sqlite3_column_text(stmt, 5);

    yueah_log_info("userid: %s", gotten_userid);
    yueah_log_info("username: %s", username);
    yueah_log_info("password_hash: %s", password_hash);
    yueah_log_info("role: %s", role);

    user_buf->userid = yueah_strdup(pool, (const char *)userid,
                                    strlen((char *)gotten_userid) + 1);
    user_buf->username = yueah_strdup(pool, (const char *)username,
                                      strlen((char *)username) + 1);
    user_buf->password_hash = yueah_strdup(pool, (const char *)password_hash,
                                           strlen((char *)password_hash) + 1);
    user_buf->role =
        yueah_strdup(pool, (const char *)role, strlen((char *)role) + 1);

    yueah_log_info("Found user at table index %d", id);
    rc = sqlite3_step(stmt);
  } else if (rc == SQLITE_DONE) {
    yueah_log_info("User %s not found", userid);
    *err = (yueah_sql_err_t){NotFound, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->userid == NULL) {
    yueah_log_error("Failed to get user: no userid");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->username == NULL) {
    yueah_log_error("Failed to get user: no username");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->password_hash == NULL) {
    yueah_log_error("Failed to get user: no password_hash");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->role == NULL) {
    yueah_log_error("Failed to get user: no role");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to get user: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  *user = user_buf;

  sqlite3_finalize(stmt);
  return 0;
}

static int get_user(h2o_mem_pool_t *pool, sqlite3 *conn, const char *username,
                    yueah_user_t **user, yueah_sql_err_t *err) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT userid, username, password_hash, role FROM users "
                    "WHERE username = ?;";
  yueah_user_t *user_buf = h2o_mem_alloc_pool(pool, yueah_user_t, 1);

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = StmtPrepare, 0};
    return -1;
  }

  if (sqlite3_column_count(stmt) != 4) {
    yueah_log_error("Failed to get user: wrong number of columns");
    yueah_log_error("SQL: %s", sql);
    yueah_log_error("SQLITE_COLUMN_COUNT: %d", sqlite3_column_count(stmt));
    *err = (yueah_sql_err_t){IncompleteTable, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    unsigned char *userid = (unsigned char *)sqlite3_column_text(stmt, 0);
    unsigned char *username = (unsigned char *)sqlite3_column_text(stmt, 1);
    unsigned char *password_hash =
        (unsigned char *)sqlite3_column_text(stmt, 2);
    unsigned char *role = (unsigned char *)sqlite3_column_text(stmt, 3);

    yueah_log_info("userid: %s", userid);
    yueah_log_info("username: %s", username);
    yueah_log_info("password_hash: %s", password_hash);
    yueah_log_info("role: %s", role);

    user_buf->userid =
        yueah_strdup(pool, (const char *)userid, strlen((char *)userid) + 1);
    user_buf->username = yueah_strdup(pool, (const char *)username,
                                      strlen((char *)username) + 1);
    user_buf->password_hash = yueah_strdup(pool, (const char *)password_hash,
                                           strlen((char *)password_hash) + 1);
    user_buf->role =
        yueah_strdup(pool, (const char *)role, strlen((char *)role) + 1);

    rc = sqlite3_step(stmt);
  } else if (rc == SQLITE_DONE) {
    yueah_log_info("User %s not found", username);
    *err = (yueah_sql_err_t){NotFound, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->userid == NULL) {
    yueah_log_error("Failed to get user: no userid");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->username == NULL) {
    yueah_log_error("Failed to get user: no username");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->password_hash == NULL) {
    yueah_log_error("Failed to get user: no password_hash");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (user_buf->role == NULL) {
    yueah_log_error("Failed to get user: no role");
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to get user: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  *user = user_buf;

  sqlite3_finalize(stmt);
  return 0;
}

static int query_blacklist(sqlite3 *conn, const char *token,
                           yueah_sql_err_t *err) {
  int rc = 0;
  int fn_rc = 0;
  int exists = 0;

  sqlite3_stmt *stmt;
  const char *sql =
      "SELECT EXISTS(SELECT 1 FROM refresh_blacklist WHERE token = ?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = StmtPrepare, 0};
    return -1;
  }

  rc = sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to bind token to blacklist query: %d:%s",
                    sqlite3_errcode(conn), sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = Bind, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    exists = sqlite3_column_int(stmt, 0);
    rc = sqlite3_step(stmt);
  }

  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to get blacklist: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = Fetch, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  if (exists == 1) {
    fn_rc = -1;
    *err = (yueah_sql_err_t){.err_type = TokenIsBlacklisted, 0};
  }

  sqlite3_finalize(stmt);
  return fn_rc;
}
static int insert_blacklist(sqlite3 *conn, char *token, yueah_sql_err_t *err) {
  int rc = 0;
  sqlite3_stmt *stmt;

  const char *sql = "INSERT INTO refresh_blacklist (token) VALUES (?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    yueah_log_error("Failed to prepare statement: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = StmtPrepare, 0};
    return -1;
  }

  sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    yueah_log_error("Failed to add to blacklist: %d:%s", sqlite3_errcode(conn),
                    sqlite3_errmsg(conn));
    *err = (yueah_sql_err_t){.err_type = Insert, 0};
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int post_register_form(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->public);

  yueah_add_cors_headers(req, yueah_state->cors->public);

  yueah_verify_err_t verify_err = verify_headers(
      req, H2O_STRLIT("POST"), H2O_STRLIT("application/x-www-form-urlencoded"));
  if (verify_err != Success)
    switch (verify_err) {
    case MethodNotAllowed:
      return generic_response(req, 405, "Method not allowed");
    case FailedToFindContentType:
      return generic_response(req, 400,
                              "Failed to find header for Content-Type");
    case WrongContentType:
      return generic_response(
          req, 400, "Content-Type is not application/x-www-form-urlencoded");
    default:
      return generic_response(req, 500, "Unknown error");
    }

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

int post_login_form(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->public);

  yueah_add_cors_headers(req, yueah_state->cors->public);

  yueah_verify_err_t verify_err = verify_headers(
      req, H2O_STRLIT("POST"), H2O_STRLIT("application/x-www-form-urlencoded"));
  if (verify_err != Success)
    switch (verify_err) {
    case MethodNotAllowed:
      return generic_response(req, 405, "Method not allowed");
    case FailedToFindContentType:
      return generic_response(req, 400,
                              "Failed to find header for Content-Type");
    case WrongContentType:
      return generic_response(
          req, 400, "Content-Type is not application/x-www-form-urlencoded");
    default:
      return generic_response(req, 500, "Unknown error");
    }

  char *form_body = yueah_iovec_to_str(&req->pool, &req->entity);
  char **body = parse_post_body(&req->pool, form_body, req->entity.len);

  char *username = get_form_val(&req->pool, "username", body);
  if (!username)
    return generic_response(req, 400, "Failed to get username from form");

  char *password = get_form_val(&req->pool, "password", body);
  if (!password)
    return generic_response(req, 400, "Failed to get password from form");

  yueah_user_t *user = {0};
  sqlite3 *db;

  if (yueah_db_connect(yueah_state->db_path, &db, READ) != 0) {
    yueah_log_error("Failed to connect to db");
    return generic_response(req, 500, "Failed to connect to db");
  }

  yueah_sql_err_t err = {0};
  int rc = get_user(&req->pool, db, username, &user, &err);
  if (rc != 0) {
    char *err_msg = h2o_mem_alloc_pool(&req->pool, char, 1024);
    int status = 500;

    switch (err.err_type) {
    case NotFound:
      memcpy(err_msg, "Invalid username or password", 28);
      status = 401;
      break;
    case Fetch:
      memcpy(err_msg, "Failed to fetch user", 19);
      break;
    case IncompleteTable:
      memcpy(err_msg, "User table is incomplete", 24);
      break;
    case StmtPrepare:
      memcpy(err_msg, "Failed to prepare SQL statement", 25);
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

  if (!verify_password(user->password_hash, password, strlen(password)))
    return generic_response(req, 401, "Invalid username or password");

  yueah_jwt_claims_t *session_claims = yueah_jwt_create_claims(
      &req->pool, "yueah", user->userid, "blog", 900 * 12, 0);
  yueah_jwt_claims_t *refresh_claims = yueah_jwt_create_claims(
      &req->pool, "yueah", user->userid, "blog", 604800, 900);

  mem_t session_token_len = 0;
  mem_t refresh_token_len = 0;

  char *session_token =
      yueah_jwt_encode(&req->pool, session_claims, Access, &session_token_len);
  if (!session_token || session_token_len < 10) {
    yueah_log_error("Failed to encode session token");
    return generic_response(req, 500, "Failed to encode session token");
  }

  char *refresh_token =
      yueah_jwt_encode(&req->pool, refresh_claims, Refresh, &refresh_token_len);
  if (!refresh_token || refresh_token_len < 10) {
    yueah_log_error("Failed to encode refresh token");
    return generic_response(req, 500, "Failed to encode refresh token");
  }

  char *session_cookie_content =
      h2o_mem_alloc_pool(&req->pool, char, session_token_len);
  memcpy(session_cookie_content, session_token, session_token_len);

  char *refresh_cookie_content =
      h2o_mem_alloc_pool(&req->pool, char, refresh_token_len);
  memcpy(refresh_cookie_content, refresh_token, refresh_token_len);

  mem_t session_cookie_len = 0;
  unsigned char *session_cookie = yueah_cookie_new(
      &req->pool, "yueah_session", session_cookie_content, session_token_len,
      &session_cookie_len, HTTP_ONLY | MAX_AGE, 900);

  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                 "Set-Cookie", (const char *)session_cookie,
                 session_cookie_len);

  mem_t refresh_cookie_len = 0;
  unsigned char *refresh_cookie = yueah_cookie_new(
      &req->pool, "yueah_refresh", refresh_cookie_content, refresh_token_len,
      &refresh_cookie_len, HTTP_ONLY | MAX_AGE, 604800);

  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                 "Set-Cookie", (const char *)refresh_cookie,
                 refresh_cookie_len);

  return generic_response(req, 200, "Login successful");
}

static void check_time(const char *label, clock_t before, clock_t after) {
  double diff = ((double)(after - before)) / CLOCKS_PER_SEC;
  yueah_log_debug("%s: %f s", label, diff);
}

int get_refresh(h2o_handler_t *handler, h2o_req_t *req) {

  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  sqlite3 *db;

  clock_t before = clock();

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->private);

  yueah_add_cors_headers(req, yueah_state->cors->private);

  clock_t after = clock();

  check_time("Time taken to check for OPTIONS and add CORS", before, after);

  before = clock();

  yueah_verify_err_t verify_err =
      verify_headers(req, H2O_STRLIT("GET"), NULL, 0);
  if (verify_err != Success)
    switch (verify_err) {
    case MethodNotAllowed:
      return generic_response(req, 405, "Method not allowed");
    default:
      return generic_response(req, 500, "Unknown error");
    }

  after = clock();
  check_time("Time taken to verify headers", before, after);

  before = clock();
  int throwaway_cursor = 0;
  int throwaway_cookie_index =
      h2o_find_header(&req->headers, H2O_TOKEN_COOKIE, throwaway_cursor);

  if (throwaway_cookie_index == -1) {
    return generic_response(req, 400, "No cookie header found");
  }
  after = clock();
  check_time("Time taken to see if any cookie exists", before, after);

  before = clock();
  h2o_iovec_t *refresh_cookie_header = get_cookie_content(req, "yueah_refresh");
  if (refresh_cookie_header == NULL)
    return generic_response(req, 400, "No refresh cookie header found");
  after = clock();
  check_time("Time taken to get refresh cookie", before, after);

  before = clock();
  bool session_expired = false;
  h2o_iovec_t *session_cookie_header = get_cookie_content(req, "yueah_session");
  if (session_cookie_header == NULL)
    session_expired = true;
  after = clock();
  check_time("Time taken to get session cookie", before, after);

  before = clock();
  if (!session_expired) {
    mem_t out_len = 0;
    char *session_cookie_str =
        yueah_iovec_to_str(&req->pool, session_cookie_header);

    unsigned char *session_cookie_content = yueah_get_cookie_content(
        &req->pool, session_cookie_str, "yueah_session", &out_len);

    exit(0);

    if (yueah_jwt_verify(&req->pool, (char *)session_cookie_content, out_len,
                         "blog", Access) == false) {
      if (yueah_db_disconnect(db) != 0) {
        yueah_log_error("Failed to disconnect from db");
        return generic_response(req, 500, "Failed to disconnect from db");
      }

      return generic_response(req, 401, "Invalid session cookie");
    }
  }
  after = clock();
  check_time("Time taken to verify session cookie", before, after);

  before = clock();

  mem_t out_len = 0;
  char *refresh_cookie_str =
      yueah_iovec_to_str(&req->pool, refresh_cookie_header);
  unsigned char *refresh_cookie_content = yueah_get_cookie_content(
      &req->pool, refresh_cookie_str, "yueah_refresh", &out_len);
  if (!refresh_cookie_content || out_len < 10)
    return generic_response(req, 400, "Failed to get refresh cookie content");

  if (yueah_jwt_verify(&req->pool, (char *)refresh_cookie_content, out_len,
                       "blog", Refresh) == false)
    return generic_response(req, 401, "Invalid refresh cookie");

  after = clock();
  check_time("Time taken to verify refresh cookie", before, after);

  before = clock();
  if (yueah_db_connect(yueah_state->db_path, &db, READ | WRITE) != 0) {
    yueah_log_error("Failed to connect to db");
    return generic_response(req, 500, "Failed to connect to db");
  }
  after = clock();
  check_time("Time taken to connect to db", before, after);

  yueah_sql_err_t query_err = {0};

  before = clock();
  if (query_blacklist(db, (char *)refresh_cookie_content, &query_err) != 0) {
    char *err_msg = h2o_mem_alloc_pool(&req->pool, char, 1024);
    int status = 500;

    if (query_err.err_type == TokenIsBlacklisted) {
      if (yueah_db_disconnect(db) != 0) {
        yueah_log_error("Failed to disconnect from db");
        return generic_response(req, 500, "Failed to disconnect from db");
      }

      return generic_response(req, 401,
                              "Invalid refresh cookie, token is blacklisted");
    }

    switch (query_err.err_type) {
    case Fetch:
      strlcpy(err_msg, "Failed to fetch token", 1024);
      status = 500;
      break;

    case IncompleteTable:
      strlcpy(err_msg, "Failed to fetch token: incomplete table", 1024);
      status = 500;
      break;

    case NotFound:
      strlcpy(err_msg, "Invalid token", 1024);
      status = 401;
      break;
    case StmtPrepare:
      strlcpy(err_msg, "Failed to prepare SQL statement", 1024);
      status = 500;
      break;
    default:
      strlcpy(err_msg, "Unknown error", 1024);
      status = 500;
      break;
    }

    if (yueah_db_disconnect(db) != 0) {
      yueah_log_error("Failed to disconnect from db");
      return generic_response(req, 500, "Failed to disconnect from db");
    }

    return generic_response(req, status, err_msg);
  }
  after = clock();
  check_time("Time taken to query blacklist", before, after);

  before = clock();
  yueah_sql_err_t insert_err = {0};
  if (insert_blacklist(db, (char *)refresh_cookie_content, &insert_err) != 0) {
    char *err_msg = h2o_mem_alloc_pool(&req->pool, char, 1024);
    int status = 500;

    switch (insert_err.err_type) {
    case StmtPrepare:
      strlcpy(err_msg, "Failed to prepare SQL statement", 1024);
      status = 500;
      break;
    case IncompleteTable:
      strlcpy(err_msg, "Failed to fetch token: incomplete table", 1024);
      status = 500;
      break;
    case Insert:
      strlcpy(err_msg, "Failed to insert token into blacklist", 1024);
      status = 500;
      break;
    default:
      strlcpy(err_msg, "Unknown error", 1024);
      status = 500;
      break;
    }

    if (yueah_db_disconnect(db) != 0) {
      yueah_log_error("Failed to disconnect from db");
      return generic_response(req, 500, "Failed to disconnect from db");
    }

    return generic_response(req, status, err_msg);
  }
  after = clock();
  check_time("Time taken to insert into blacklist", before, after);

  before = clock();
  char *user_sub =
      yueah_jwt_get_sub(&req->pool, (char *)refresh_cookie_content, out_len);
  yueah_user_t *user = {0};
  if (get_user_userid(&req->pool, db, user_sub, &user, &query_err) != 0) {
    char *err_msg = h2o_mem_alloc_pool(&req->pool, char, 1024);
    int status = 500;

    switch (query_err.err_type) {
    case NotFound:
      memcpy(err_msg, "Could not find user", 20);
      status = 401;
      break;
    case Fetch:
      memcpy(err_msg, "Failed to fetch user", 19);
      break;
    case IncompleteTable:
      memcpy(err_msg, "User table is incomplete", 24);
      break;
    case StmtPrepare:
      memcpy(err_msg, "Failed to prepare SQL statement", 25);
      break;
    default:
      memcpy(err_msg, "Unknown error", 13);
      break;
    }
    if (yueah_db_disconnect(db) != 0) {
      yueah_log_error("Failed to disconnect from db");
      return generic_response(req, 500, "Failed to disconnect from db");
    }

    return generic_response(req, status, err_msg);
  }
  after = clock();
  check_time("Time taken to get user", before, after);

  before = clock();
  if (yueah_db_disconnect(db) != 0) {
    yueah_log_error("Failed to disconnect from db");
    return generic_response(req, 500, "Failed to disconnect from db");
  }
  after = clock();
  check_time("Time taken to disconnect from db", before, after);

  before = clock();
  int success = yueah_delete_cookie(req);
  if (success == -1) {
    yueah_log_error("Failed to delete cookie");
    return generic_response(req, 500, "Failed to delete cookie");
  }
  after = clock();
  check_time("Time taken to delete cookie", before, after);

  before = clock();
  yueah_jwt_claims_t *new_refresh_claims = yueah_jwt_create_claims(
      &req->pool, "yueah", user->userid, "blog", 604800, 0);

  mem_t new_refresh_token_len = 0;
  char *new_refresh_token = yueah_jwt_encode(&req->pool, new_refresh_claims,
                                             Refresh, &new_refresh_token_len);

  mem_t new_refresh_cookie_len = 0;
  unsigned char *new_refresh_cookie = yueah_cookie_new(
      &req->pool, "yueah_refresh", new_refresh_token, new_refresh_token_len,
      &new_refresh_cookie_len, HTTP_ONLY | MAX_AGE, 604800);
  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                 "Set-Cookie", (const char *)new_refresh_cookie,
                 new_refresh_cookie_len);
  after = clock();
  check_time("Time taken to generate new refresh cookie", before, after);

  before = clock();
  yueah_jwt_claims_t *new_access_claims = yueah_jwt_create_claims(
      &req->pool, "yueah", user->userid, "blog", 900, 0);

  mem_t new_access_token_len = 0;
  char *new_access_token = yueah_jwt_encode(&req->pool, new_access_claims,
                                            Access, &new_access_token_len);

  mem_t new_access_cookie_len = 0;
  unsigned char *new_access_cookie = yueah_cookie_new(
      &req->pool, "yueah", new_access_token, new_access_token_len,
      &new_access_cookie_len, HTTP_ONLY | MAX_AGE, 900);

  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                 "Set-Cookie", (const char *)new_access_cookie,
                 new_refresh_cookie_len);
  after = clock();
  check_time("Time taken to generate new access cookie", before, after);

  yueah_log_debug("Generated new tokens");

  return generic_response(req, 200, "Refresh successful");
}
