#include <stddef.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/db.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_error_t yueah_db_connect(h2o_mem_pool_t *pool,
                               const yueah_string_t *db_path, sqlite3 **db,
                               int flags) {
  int rc = SQLITE_OK;
  cstr *db_path_cstr = yueah_string_to_cstr(pool, db_path);

  switch (flags) {
  case READ:
    rc = sqlite3_open_v2(db_path_cstr, db, SQLITE_OPEN_READONLY, NULL);
    break;
  case WRITE:
    rc = sqlite3_open_v2(db_path_cstr, db, SQLITE_OPEN_READWRITE, NULL);
    break;
  case READ | WRITE:
    rc = sqlite3_open_v2(db_path_cstr, db, SQLITE_OPEN_READWRITE, NULL);
    break;
  }

  if (*db == NULL || rc != SQLITE_OK)
    return yueah_throw_error("sqlite3_open_v2 failed %d:%s",
                             sqlite3_errcode(*db), sqlite3_errmsg(*db));

  return yueah_success(NULL);
}

yueah_error_t yueah_db_disconnect(sqlite3 *db) {
  int rc = SQLITE_OK;

  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    return yueah_success(NULL);

  return yueah_throw_error("sqlite3_close failed %d:%s", sqlite3_errcode(db),
                           sqlite3_errmsg(db));
}
