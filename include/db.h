#ifndef YUEAH_DB_H
#define YUEAH_DB_H

#include <h2o.h>
#include <sqlite3.h>

#include <config.h>

typedef enum {
  READ = 1 << 0,
  WRITE = 1 << 1,
} db_flags_t;

int db_connect(const char *db_path, sqlite3 **db, int flags);
int db_disconnect(sqlite3 *db);

#endif // !YUEAH_DB_H
