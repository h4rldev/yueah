#include <stddef.h>
#include <stdio.h>

#include <sqlite3.h>

#include <db.h>

int init_db(yueah_config_t *config, sqlite3 **db) {
  char *path = config->db_path;

  sqlite3_open(path, db);
  if (*db == NULL) {
    return -1;
  }

  return 0;
}

void close_db(sqlite3 *db) { sqlite3_close(db); }
