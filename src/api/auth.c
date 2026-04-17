#include <string.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/cookie.h>
#include <yueah/cors.h>
#include <yueah/db.h>
#include <yueah/error.h>
#include <yueah/hash.h>
#include <yueah/jwt.h>
#include <yueah/log.h>
#include <yueah/response.h>
#include <yueah/string.h>
#include <yueah/types.h>
#include <yueah/uuid.h>

#include <api/auth.h>

static yueah_error_t verify_headers(h2o_req_t *req, yueah_string_t *method,
                                    yueah_string_t *content_type) {
  if (!h2o_memis(req->method.base, req->method.len, YUEAH_SSTRLIT(method)))
    return yueah_throw_error("Method not allowed");

  if (content_type != NULL) {
    int header_index =
        h2o_find_header(&req->headers, H2O_TOKEN_CONTENT_TYPE, -1);
    if (header_index == -1)
      return yueah_throw_error("Failed to find header for Content-Type");

    if (!h2o_memis(req->headers.entries[header_index].value.base,
                   req->headers.entries[header_index].value.len,
                   YUEAH_SSTRLIT(content_type)))
      return yueah_throw_error(
          "Content-Type is not application/x-www-form-urlencoded");
  }

  return yueah_success(NULL);
}

static yueah_error_t get_constraint_err(const int errcode,
                                        const char *error_msg) {
  if (strstr(error_msg, "UNIQUE constraint failed: users.username") != NULL)
    return yueah_throw_error(
        "Failed to insert user (%d:%s), i.e Username already exists", errcode,
        error_msg);

  if (strstr(error_msg, "UNIQUE constraint failed: users.userid") != NULL)
    return yueah_throw_error(
        "Failed to insert user (%d:%s), i.e UserID already exists", errcode,
        error_msg);

  return yueah_success(NULL);
}

static yueah_error_t insert_user(h2o_mem_pool_t *pool, sqlite3 *conn,
                                 yueah_user_t *user) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO users (userid, username, password_hash, role) "
                    "VALUES (?, ?, ?, ?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return yueah_throw_error("Failed to prepare statement: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));

  cstr *userid_cstr = yueah_string_to_cstr(pool, user->userid);
  cstr *username_cstr = yueah_string_to_cstr(pool, user->username);
  cstr *password_hash_cstr = yueah_string_to_cstr(pool, user->password_hash);
  cstr *role_cstr = yueah_string_to_cstr(pool, user->role);

  sqlite3_bind_text(stmt, 1, userid_cstr, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, username_cstr, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, password_hash_cstr, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, role_cstr, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    if (rc == SQLITE_CONSTRAINT)
      return get_constraint_err(sqlite3_errcode(conn), sqlite3_errmsg(conn));

    return yueah_throw_error("Failed to insert user (%d:%s)",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));
  }

  sqlite3_finalize(stmt);
  yueah_log_info("Inserted user: %.*s:%.*s", (int)user->username->len,
                 user->username->data, (int)user->userid->len,
                 user->userid->data);

  return yueah_success(NULL);
}

static yueah_user_t *get_user_userid(h2o_mem_pool_t *pool, sqlite3 *conn,
                                     const yueah_string_t *userid,
                                     yueah_error_t *error) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT username, password_hash, role FROM users "
                    "WHERE userid = ?;";
  yueah_user_t *user_buf = h2o_mem_alloc_pool(pool, yueah_user_t, 1);

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    *error = yueah_throw_error("Failed to prepare statement: %d:%s",
                               sqlite3_errcode(conn), sqlite3_errmsg(conn));
    return NULL;
  }

  cstr *userid_cstr = yueah_string_to_cstr(pool, userid);
  sqlite3_bind_text(stmt, 1, userid_cstr, -1, SQLITE_STATIC);

  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const ucstr *username = sqlite3_column_text(stmt, 0);
    const ucstr *password_hash = sqlite3_column_text(stmt, 1);
    const ucstr *role = sqlite3_column_text(stmt, 2);

    yueah_log_debug("(%s) Gotten username: %s", userid_cstr, username);
    yueah_log_debug("(%s) Gotten password_hash: %s", userid_cstr,
                    password_hash);
    yueah_log_debug("(%s) Gotten role: %s", userid_cstr, role);

    user_buf->userid = (yueah_string_t *)userid;
    user_buf->username =
        yueah_string_new(pool, (cstr *)username, strlen((cstr *)username));
    user_buf->password_hash = yueah_string_new(pool, (cstr *)password_hash,
                                               strlen((cstr *)password_hash));
    user_buf->role = yueah_string_new(pool, (cstr *)role, strlen((cstr *)role));
    rc = sqlite3_step(stmt);
  } else if (rc == SQLITE_DONE) {
    *error = yueah_throw_error("User %s not found", userid_cstr);
    sqlite3_finalize(stmt);
    return NULL;
  }

  sqlite3_finalize(stmt);
  *error = yueah_success(NULL);
  return user_buf;
}

static yueah_user_t *get_user(h2o_mem_pool_t *pool, sqlite3 *conn,
                              const yueah_string_t *username,
                              yueah_error_t *error) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const cstr *sql = "SELECT userid, password_hash, role FROM users "
                    "WHERE username = ?;";
  yueah_user_t *user_buf = h2o_mem_alloc_pool(pool, yueah_user_t, 1);

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    *error = yueah_throw_error("Failed to prepare statement: %d:%s",
                               sqlite3_errcode(conn), sqlite3_errmsg(conn));
    return NULL;
  }

  cstr *username_cstr = yueah_string_to_cstr(pool, username);
  sqlite3_bind_text(stmt, 1, username_cstr, -1, SQLITE_STATIC);

  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const ucstr *userid = sqlite3_column_text(stmt, 0);
    const ucstr *password_hash = sqlite3_column_text(stmt, 1);
    const ucstr *role = sqlite3_column_text(stmt, 2);

    yueah_log_debug("(%s) Gotten Userid: %s", username_cstr, userid);
    yueah_log_debug("(%s) Gotten password_hash: %s", username_cstr,
                    password_hash);
    yueah_log_debug("(%s) Gotten role: %s", username_cstr, role);

    user_buf->username = (yueah_string_t *)username;
    user_buf->userid =
        yueah_string_new(pool, (cstr *)userid, strlen((cstr *)userid));
    user_buf->password_hash = yueah_string_new(pool, (cstr *)password_hash,
                                               strlen((cstr *)password_hash));
    user_buf->role = yueah_string_new(pool, (cstr *)role, strlen((cstr *)role));
    rc = sqlite3_step(stmt);
  } else if (rc == SQLITE_DONE) {
    *error = yueah_throw_error("User %s not found", username_cstr);
    sqlite3_finalize(stmt);
    return NULL;
  }

  sqlite3_finalize(stmt);
  *error = yueah_success(NULL);
  return user_buf;
}

static yueah_error_t query_blacklist(h2o_mem_pool_t *pool, sqlite3 *conn,
                                     const yueah_string_t *token) {
  int rc = 0;
  int exists = 0;

  sqlite3_stmt *stmt;
  const cstr *sql =
      "SELECT EXISTS(SELECT 1 FROM refresh_blacklist WHERE token = ?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return yueah_throw_error("Failed to prepare statement: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));

  cstr *token_cstr = yueah_string_to_cstr(pool, token);
  rc = sqlite3_bind_text(stmt, 1, token_cstr, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(stmt);
    return yueah_throw_error("Failed to bind token to blacklist query: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    exists = sqlite3_column_int(stmt, 0);
    rc = sqlite3_step(stmt);
  }

  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return yueah_throw_error("Failed to get blacklist: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));
  }

  sqlite3_finalize(stmt);
  return exists == 1 ? yueah_throw_error("Token is blacklisted")
                     : yueah_success(NULL);
}
static yueah_error_t insert_blacklist(h2o_mem_pool_t *pool, sqlite3 *conn,
                                      yueah_string_t *token) {
  int rc = 0;
  sqlite3_stmt *stmt;
  const cstr *sql = "INSERT INTO refresh_blacklist (token) VALUES (?);";

  rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return yueah_throw_error("Failed to prepare statement: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));

  cstr *token_cstr = yueah_string_to_cstr(pool, token);
  sqlite3_bind_text(stmt, 1, token_cstr, -1, SQLITE_STATIC);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return yueah_throw_error("Failed to add to blacklist: %d:%s",
                             sqlite3_errcode(conn), sqlite3_errmsg(conn));
  }

  sqlite3_finalize(stmt);
  return yueah_success(NULL);
}

int post_register_form(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  h2o_mem_pool_t *pool = &req->pool;

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->public);

  yueah_add_cors_headers(req, yueah_state->cors->public);

  yueah_error_t verify_err = verify_headers(
      req, YUEAH_STR("POST"), YUEAH_STR("application/x-www-form-urlencoded"));
  if (verify_err.status != OK) {
    if (strcmp(verify_err.message, "Method not allowed") == 0)
      return yueah_generic_response(req, 405, YUEAH_STR("Method not allowed"));
    if (strcmp(verify_err.message, "Failed to find header for Content-Type") ==
        0)
      return yueah_generic_response(
          req, 400, YUEAH_STR("Failed to find header for Content-Type"));
    if (strcmp(verify_err.message,
               "Content-Type is not application/x-www-form-urlencoded") == 0)
      return yueah_generic_response(
          req, 400,
          YUEAH_STR("Content-Type is not application/x-www-form-urlencoded"));

    return yueah_generic_response(req, 500, YUEAH_STR("Unknown error"));
  }

  yueah_user_t *user = h2o_mem_alloc_pool(&req->pool, yueah_user_t, 1);
  sqlite3 *db;

  yueah_error_t error = yueah_success(NULL);
  yueah_string_t *form_body = yueah_string_from_iovec(pool, &req->entity);
  yueah_string_array_t *form_data =
      yueah_parse_form_body(pool, form_body, &error);
  if (!form_data) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 500, YUEAH_STR("Failed to parse form, is your form empty?"));
  }

  user->username =
      yueah_get_form_val(pool, YUEAH_STR("username"), form_data, &error);
  if (!user->username) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 400, YUEAH_STR("Failed to get username, is the form malformed?"));
  }

  yueah_string_t *password =
      yueah_get_form_val(pool, YUEAH_STR("password"), form_data, &error);
  if (!password) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 400, YUEAH_STR("Failed to get password, is the form malformed?"));
  }

  user->password_hash = hash_password(pool, password, &error);
  if (!user->password_hash) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 500,
        YUEAH_STR("Failed to hash password, something went very wrong"));
  }

  yueah_string_t *uuid = yueah_uuid_new(pool);
  if (!uuid) {
    yueah_log_error("Failed to generate uuid, something is very wrong");
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to generate uuid"));
  }

  user->userid = uuid;
  user->role = yueah_string_new(pool, "poster", 6);

  error = yueah_db_connect(pool, yueah_state->db_path, &db, READ | WRITE);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to connect to db"));
  }

  error = insert_user(pool, db, user);
  if (error.status != OK) {
    yueah_print_error(error);
    yueah_string_t *err_msg = yueah_string_new(pool, NULL, 1024);
    u16 status = 500;

    if (strstr(error.message, "UNIQUE constraint failed: users.username") !=
        NULL) {
      err_msg->len =
          snprintf((cstr *)err_msg->data, 1024, "User already exists");
      status = 400;
    } else if (strstr(error.message,
                      "UNIQUE constraint failed: users.userid") != NULL) {
      err_msg->len =
          snprintf((cstr *)err_msg->data, 1024, "User already exists");
      status = 400;
    } else {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024, "Unknown error");
      status = 500;
    }

    return yueah_generic_response(req, status, err_msg);
  }

  error = yueah_db_disconnect(db);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to disconnect from db"));
  }

  cstr *username_cstr = yueah_string_to_cstr(pool, user->username);
  cstr *userid_cstr = yueah_string_to_cstr(pool, user->userid);

  yueah_string_t *message = yueah_string_new(pool, NULL, 1024);
  message->len =
      snprintf((cstr *)message->data, 1024, "Register of %s:%s successful",
               username_cstr, userid_cstr);

  return yueah_generic_response(req, 200, message);
}

int post_login_form(h2o_handler_t *handler, h2o_req_t *req) {
  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  h2o_mem_pool_t *pool = &req->pool;

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->public);

  yueah_add_cors_headers(req, yueah_state->cors->public);

  yueah_error_t verify_err = verify_headers(
      req, YUEAH_STR("POST"), YUEAH_STR("application/x-www-form-urlencoded"));
  if (verify_err.status != OK) {
    if (strcmp(verify_err.message, "Method not allowed") == 0)
      return yueah_generic_response(req, 405, YUEAH_STR("Method not allowed"));
    if (strcmp(verify_err.message, "Failed to find header for Content-Type") ==
        0)
      return yueah_generic_response(
          req, 400, YUEAH_STR("Failed to find header for Content-Type"));
    if (strcmp(verify_err.message,
               "Content-Type is not application/x-www-form-urlencoded") == 0)
      return yueah_generic_response(
          req, 400,
          YUEAH_STR("Content-Type is not application/x-www-form-urlencoded"));

    return yueah_generic_response(req, 500, YUEAH_STR("Unknown error"));
  }

  sqlite3 *db;
  yueah_error_t error = yueah_success(NULL);
  yueah_string_t *form_body = yueah_string_from_iovec(pool, &req->entity);
  yueah_string_array_t *form_data =
      yueah_parse_form_body(pool, form_body, &error);
  if (!form_data) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 500, YUEAH_STR("Failed to parse form, is your form empty?"));
  }

  yueah_string_t *username =
      yueah_get_form_val(pool, YUEAH_STR("username"), form_data, &error);
  if (!username) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 400, YUEAH_STR("Failed to get username, is your form malformed?"));
  }

  yueah_string_t *password =
      yueah_get_form_val(pool, YUEAH_STR("password"), form_data, &error);
  if (!password) {
    yueah_print_error(error);
    return yueah_generic_response(
        req, 400, YUEAH_STR("Failed to get password, is your form malformed?"));
  }

  error = yueah_db_connect(pool, yueah_state->db_path, &db, READ);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to connect to db"));
  }

  yueah_user_t *user = get_user(pool, db, username, &error);
  if (!user) {
    yueah_print_error(error);
    yueah_string_t *err_msg = yueah_string_new(pool, NULL, 1024);
    u16 status = 500;

    if (strstr(error.message, "User") != NULL) {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024, "User not found");
      status = 401;
    } else {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024, "Unknown error");
      status = 500;
    }

    error = yueah_db_disconnect(db);
    if (error.status != OK)
      yueah_print_error(error);

    return yueah_generic_response(req, status, err_msg);
  }

  error = yueah_db_disconnect(db);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to disconnect from db"));
  }

  if (!verify_password(pool, password, user->password_hash))
    return yueah_generic_response(req, 401,
                                  YUEAH_STR("Invalid username or password"));

  yueah_jwt_claims_t *session_claims =
      yueah_jwt_create_claims(&req->pool, YUEAH_STR("yueah"), user->userid,
                              YUEAH_STR("blog"), 900 * 12, 0, &error);
  yueah_jwt_claims_t *refresh_claims =
      yueah_jwt_create_claims(&req->pool, YUEAH_STR("yueah"), user->userid,
                              YUEAH_STR("blog"), 604800, 900, &error);

  yueah_string_t *session_token =
      yueah_jwt_encode(&req->pool, session_claims, Access, &error);
  if (!session_token || session_token->len < 10) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to encode token"));
  }

  yueah_string_t *refresh_token =
      yueah_jwt_encode(&req->pool, refresh_claims, Refresh, &error);
  if (!refresh_token || refresh_token->len < 10) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500, YUEAH_STR("Failed to token"));
  }

  yueah_string_t *session_cookie =
      yueah_cookie_new(&req->pool, YUEAH_STR("yueah_session"), session_token,
                       &error, HTTP_ONLY | MAX_AGE, 900);
  if (!session_cookie) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create cookie"));
  }

  h2o_add_header(pool, &req->res.headers, H2O_TOKEN_SET_COOKIE, "Set-Cookie",
                 YUEAH_SSTRLIT(session_cookie));

  yueah_string_t *refresh_cookie =
      yueah_cookie_new(pool, YUEAH_STR("yueah_refresh"), refresh_token, &error,
                       HTTP_ONLY | MAX_AGE, 604800);
  if (!refresh_cookie) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create cookie"));
  }

  h2o_add_header(pool, &req->res.headers, H2O_TOKEN_SET_COOKIE, "Set-Cookie",
                 YUEAH_SSTRLIT(refresh_cookie));

  return yueah_generic_response(req, 200, YUEAH_STR("Login successful"));
}

/*
static void check_time(const char *label, clock_t before, clock_t after) {
  double diff = ((double)(after - before)) / CLOCKS_PER_SEC;
  yueah_log_debug("%s: %f s", label, diff);
}
*/
int get_refresh(h2o_handler_t *handler, h2o_req_t *req) {

  yueah_handler_t *yueah_handler = (yueah_handler_t *)handler;
  yueah_state_t *yueah_state = yueah_handler->state;
  sqlite3 *db;
  yueah_error_t error = yueah_success(NULL);
  h2o_mem_pool_t *pool = &req->pool;

  if (h2o_memis(req->method.base, req->method.len, H2O_STRLIT("OPTIONS")))
    return yueah_handle_options(req, yueah_state->cors->private);

  yueah_add_cors_headers(req, yueah_state->cors->private);

  yueah_error_t verify_err = verify_headers(req, YUEAH_STR("GET"), NULL);
  if (verify_err.status != OK) {
    if (strcmp(verify_err.message, "Method not allowed") == 0)
      return yueah_generic_response(req, 405, YUEAH_STR("Method not allowed"));

    return yueah_generic_response(req, 500, YUEAH_STR("Unknown error"));
  }

  int throwaway_cursor = 0;
  int throwaway_cookie_index =
      h2o_find_header(&req->headers, H2O_TOKEN_COOKIE, throwaway_cursor);

  if (throwaway_cookie_index == -1)
    return yueah_generic_response(req, 400, YUEAH_STR("No cookie found"));

  yueah_string_t *refresh_cookie_header =
      yueah_req_get_cookie_content(req, YUEAH_STR("yueah_refresh"), &error);
  if (!refresh_cookie_header) {
    yueah_print_error(error);
    return yueah_generic_response(req, 400,
                                  YUEAH_STR("No refresh cookie header found"));
  }

  bool session_expired = false;
  yueah_string_t *session_cookie_header =
      yueah_req_get_cookie_content(req, YUEAH_STR("yueah_session"), &error);
  if (!session_cookie_header)
    session_expired = true;

  if (!session_expired) {
    yueah_string_t *session_cookie_content = yueah_get_cookie_content(
        pool, session_cookie_header, YUEAH_STR("yueah_session"), &error);
    if (!session_cookie_content) {
      yueah_print_error(error);
      return yueah_generic_response(req, 401,
                                    YUEAH_STR("Invalid session cookie"));
    }

    if (yueah_jwt_verify(pool, session_cookie_content, YUEAH_STR("blog"),
                         Access, &error) == false) {
      if (error.status != OK)
        yueah_print_error(error);
      return yueah_generic_response(req, 401,
                                    YUEAH_STR("Invalid session cookie"));
    }
  }

  yueah_string_t *refresh_cookie_content = yueah_get_cookie_content(
      pool, refresh_cookie_header, YUEAH_STR("yueah_refresh"), &error);
  if (!refresh_cookie_content) {
    yueah_print_error(error);
    return yueah_generic_response(req, 400,
                                  YUEAH_STR("Failed to get refresh cookie"));
  }

  if (yueah_jwt_verify(pool, refresh_cookie_content, YUEAH_STR("blog"), Refresh,
                       &error) == false) {
    yueah_print_error(error);
    return yueah_generic_response(req, 401,
                                  YUEAH_STR("Invalid refresh cookie"));
  }

  error = yueah_db_connect(pool, yueah_state->db_path, &db, READ | WRITE);
  if (error.status != OK) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to connect to db"));
  }

  yueah_error_t disconnect_error = yueah_success(NULL);
  error = query_blacklist(pool, db, refresh_cookie_content);
  if (error.status != OK) {
    yueah_print_error(error);

    disconnect_error = yueah_db_disconnect(db);
    if (disconnect_error.status != OK)
      yueah_print_error(disconnect_error);

    if (strstr(error.message, "Token") != NULL) {
      return yueah_generic_response(
          req, 401, YUEAH_STR("Can't refresh, Token is blacklisted"));
    } else if (strstr(error.message, "Failed to prepare") != NULL) {
      return yueah_generic_response(
          req, 500, YUEAH_STR("Failed to prepare SQL statement"));
    }

    return yueah_generic_response(
        req, 401, YUEAH_STR("Unable to refresh, unknown error"));
  }

  error = insert_blacklist(pool, db, refresh_cookie_content);
  if (error.status != OK) {
    yueah_string_t *err_msg = yueah_string_new(pool, NULL, 1024);
    int status = 500;

    disconnect_error = yueah_db_disconnect(db);
    if (disconnect_error.status != OK)
      yueah_print_error(disconnect_error);

    if (strstr(error.message, "Failed to add") != NULL) {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024,
                              "Failed to add Token to blacklist.");
      status = 500;
    } else if (strstr(error.message, "Failed to prepare") != NULL) {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024,
                              "Failed to prepare SQL statement.");
      status = 500;
    } else {
      err_msg->len = snprintf((cstr *)err_msg->data, 1024, "Unknown error");
      status = 500;
    }

    return yueah_generic_response(req, status, err_msg);
  }

  yueah_string_t *user_sub =
      yueah_jwt_get_sub(pool, refresh_cookie_content, &error);
  if (!user_sub) {
    yueah_print_error(error);

    disconnect_error = yueah_db_disconnect(db);
    if (disconnect_error.status != OK)
      yueah_print_error(disconnect_error);

    return yueah_generic_response(req, 500, YUEAH_STR("Failed to get user"));
  }

  yueah_user_t *user = get_user_userid(pool, db, user_sub, &error);
  if (!user) {
    yueah_print_error(error);

    disconnect_error = yueah_db_disconnect(db);
    if (disconnect_error.status != OK)
      yueah_print_error(disconnect_error);

    if (strstr(error.message, "Failed to prepare") != NULL)
      return yueah_generic_response(
          req, 500, YUEAH_STR("Failed to prepare SQL statement"));
    if (strstr(error.message, "User") != NULL)
      return yueah_generic_response(req, 401, YUEAH_STR("Could not find user"));

    return yueah_generic_response(req, 500, YUEAH_STR("Failed to get user"));
  }

  disconnect_error = yueah_db_disconnect(db);
  if (disconnect_error.status != OK) {
    yueah_print_error(disconnect_error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to disconnect from db"));
  }

  error = yueah_req_delete_cookie(req);
  if (error.status != OK) {
    yueah_print_error(error);
    if (strstr(error.message, "Failed to delete cookie") != NULL)
      return yueah_generic_response(req, 500,
                                    YUEAH_STR("Failed to delete old cookie"));
    if (strstr(error.message, "Didn't find cookie") != NULL)
      return yueah_generic_response(req, 400, YUEAH_STR("No cookie found"));
  }

  yueah_jwt_claims_t *new_refresh_claims =
      yueah_jwt_create_claims(pool, YUEAH_STR("yueah"), user->userid,
                              YUEAH_STR("blog"), 604800, 0, &error);
  if (!new_refresh_claims) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create JWT claims"));
  }

  yueah_string_t *new_refresh_token =
      yueah_jwt_encode(pool, new_refresh_claims, Refresh, &error);
  if (!new_refresh_token) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500, YUEAH_STR("Failed to encode JWT"));
  }

  yueah_string_t *new_refresh_cookie =
      yueah_cookie_new(pool, YUEAH_STR("yueah_refresh"), new_refresh_token,
                       &error, HTTP_ONLY | MAX_AGE, 604800);
  if (!new_refresh_cookie) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create cookie"));
  }

  ssize_t idx = h2o_add_header(pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                               "Set-Cookie", YUEAH_SSTRLIT(new_refresh_cookie));
  if (idx < 0) {
    yueah_log_error("Failed to add Refresh Set-Cookie header, idx: %ld", idx);
    return yueah_generic_response(req, 500, YUEAH_STR("Failed to add cookie"));
  }

  yueah_jwt_claims_t *new_access_claims =
      yueah_jwt_create_claims(pool, YUEAH_STR("yueah"), user->userid,
                              YUEAH_STR("blog"), 900, 0, &error);
  if (!new_access_claims) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create JWT claims"));
  }

  yueah_string_t *new_access_token =
      yueah_jwt_encode(pool, new_access_claims, Access, &error);
  if (!new_access_token) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500, YUEAH_STR("Failed to encode JWT"));
  }

  yueah_string_t *new_access_cookie =
      yueah_cookie_new(pool, YUEAH_STR("yueah_session"), new_access_token,
                       &error, HTTP_ONLY | MAX_AGE, 900);
  if (!new_access_cookie) {
    yueah_print_error(error);
    return yueah_generic_response(req, 500,
                                  YUEAH_STR("Failed to create cookie"));
  }

  idx = h2o_add_header(pool, &req->res.headers, H2O_TOKEN_SET_COOKIE,
                       "Set-Cookie", YUEAH_SSTRLIT(new_access_cookie));
  if (idx < 0) {
    yueah_log_error("Failed to add Access Set-Cookie header, idx: %ld", idx);
    return yueah_generic_response(req, 500, YUEAH_STR("Failed to add cookie"));
  }

  return yueah_generic_response(req, 200, YUEAH_STR("Refresh successful"));
}
