#ifndef YUEAH_DB_H
#define YUEAH_DB_H

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/config.h>

typedef enum {
  READ = 1 << 0,
  WRITE = 1 << 1,
} yueah_db_flags_t;

int yueah_db_connect(const char *db_path, sqlite3 **db, int flags);
int yueah_db_disconnect(sqlite3 *db);

#endif // !YUEAH_DB_H
