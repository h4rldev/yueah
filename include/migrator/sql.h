#ifndef YUEAH_MIGRATOR_SQL_H
#define YUEAH_MIGRATOR_SQL_H

#include <stdbool.h>
#include <stdint.h>

#include <sqlite3.h>

#include <mem.h>

typedef enum { TIMESTAMP_U64, ID_U64 } id_type_t;
typedef struct {
  uint64_t identifier; // Can be timestamp or >1
  char *path;
} migration_t;

int db_connect(const char *db_path, sqlite3 **db, bool rw, bool create);
int db_disconnect(sqlite3 *db);
int clear_migrations(char *db_path);
int get_user_version(sqlite3 *db);
int set_user_version(sqlite3 *db, int user_version);
int begin_transaction(sqlite3 *db);
int run_sql(mem_arena *arena, sqlite3 *db, char *sql);
int commit_transaction(sqlite3 *db);

/// IO
migration_t *find_migrations(mem_arena *arena, const char *path,
                             mem_t *file_count);
/// !IO

#endif // !YUEAH_MIGRATOR_SQL_H
