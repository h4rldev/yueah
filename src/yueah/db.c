#include <stddef.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/db.h>
#include <yueah/log.h>

int yueah_db_connect(const char *db_path, sqlite3 **db, int flags) {
  int rc = SQLITE_OK;

  switch (flags) {
  case READ:
    rc = sqlite3_open_v2(db_path, db, SQLITE_OPEN_READONLY, NULL);
    break;
  case WRITE:
    rc = sqlite3_open_v2(db_path, db, SQLITE_OPEN_READWRITE, NULL);
    break;
  case READ | WRITE:
    rc = sqlite3_open_v2(db_path, db, SQLITE_OPEN_READWRITE, NULL);
    break;
  }

  if (*db == NULL || rc != SQLITE_OK) {
    yueah_log(Error, false, "sqlite3_open_v2 failed %d:%s",
              sqlite3_errcode(*db), sqlite3_errmsg(*db));
    return -1;
  }

  return 0;
}

int yueah_db_disconnect(sqlite3 *db) {
  int rc = SQLITE_OK;

  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    return 0;

  yueah_log(Error, false, "sqlite3_close failed %d:%s", sqlite3_errcode(db),
            sqlite3_errmsg(db));
  return -1;
}
