#include <stddef.h>

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/db.h>
#include <yueah/log.h>

int db_connect(const char *db_path, sqlite3 **db, int flags) {
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

/*
static int handle_step(sqlite3 *db, int rc) {
  if (rc != SQLITE_ROW)
    switch (rc) {
    case SQLITE_BUSY:
      yueah_log(Info, true, "Database is busy, waiting 5 seconds");
      if (sqlite3_busy_timeout(db, 5000) == SQLITE_BUSY) {
        yueah_log(Error, true, "Database is still busy, quitting");
        goto Error;
      }
      break;
    case SQLITE_ERROR:
      yueah_log(Error, true, "sqlite3_step failed: %d:%s", sqlite3_errcode(db),
                sqlite3_errmsg(db));
      goto Error;

    case SQLITE_MISUSE:
      yueah_log(Error, true, "sqlite3_step misuse");
      goto Error;
    }

  return 0;

Error:
  return -1;
}
*/

int db_disconnect(sqlite3 *db) {
  int rc = SQLITE_OK;

  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    return 0;

  yueah_log(Error, false, "sqlite3_close failed %d:%s", sqlite3_errcode(db),
            sqlite3_errmsg(db));
  return -1;
}
