#include <stddef.h>
#include <string.h>

#include <migrator/cli.h>
#include <migrator/file.h>
#include <migrator/log.h>
#include <migrator/mem.h>
#include <migrator/sql.h>

#include <sqlite3.h>

static mem_arena *arena;
static db_args_t *db_args;

int apply_migration(sqlite3 *db, char *sql, int user_version) {
  migrator_log(Debug, false, "Applying version %d", user_version);
  if (set_user_version(db, user_version) < 0)
    return -1;

  migrator_log(Debug, false, "New version = %d", get_user_version(db));

  migrator_log(Debug, false, "Beginning transaction");
  if (begin_transaction(db) < 0)
    return -1;

  migrator_log(Debug, false, "Running sql: %s", sql);
  migrator_log(Debug, false, "Running sql");
  if (run_sql(arena, db, sql) < 0)
    return -1;

  migrator_log(Debug, false, "Committing transaction");
  if (commit_transaction(db) < 0)
    return -1;

  return 0;
}

int migrate(void) {
  sqlite3 *db;
  mem_t file_count = 0;
  char *err_msg = 0;
  char **sql_bufs;

  int user_version = 0;

  char *db_path = db_args->db_path;
  char *migrations_path = db_args->migrations_path;

  migrator_log(Info, false, "Crawling %s for migrations", migrations_path);
  migration_t *migrations =
      find_migrations(arena, migrations_path, &file_count);

  if (!migrations) {
    migrator_log(Error, false, "crawl_one_dir failed");
    return -1;
  }

  if (db_connect(db_path, &db, true, db_args->create_db) < 0) {
    migrator_log(Error, false, "db_connect failed");
    return -1;
  }

  user_version = get_user_version(db);

  if (user_version < 0) {
    migrator_log(Error, false, "get_user_version failed");
    return -1;
  }

  if (user_version < 1) {
    if (set_user_version(db, 1) < 0) {
      migrator_log(Error, false, "set_user_version failed");
      return -1;
    }

    user_version = 1;
  }

  migrator_log(Debug, false, "user_version: %d", user_version);

  if (err_msg) {
    migrator_log(Error, false, "Failed to set user_version: %s", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }

  sql_bufs = arena_push_array(arena, char *, file_count, false);

  mem_t file_size = 0;
  for (mem_t file_idx = 0; file_idx < file_count; file_idx++) {
    sql_bufs[file_idx] =
        read_file(arena, migrations[file_idx].path, &file_size);
    migrator_log(Debug, false, "Read %lu bytes from %s", file_size,
                 migrations[file_idx].path);
  }

  for (mem_t i = 0; i < file_count; i++) {
    mem_t migration_idx =
        user_version + i; // This is the VERSION number (1-indexed)
    mem_t array_idx = migration_idx - 1; // Convert to array index (0-indexed)

    if (migration_idx == (mem_t)user_version || migration_idx > file_count)
      continue;

    // The version to set is migration_idx (which equals user_version + i + 1
    // conceptually)

    mem_t next_version = migration_idx;

    migrator_log(Info, false, "Migration idx: %lu", migration_idx);
    migrator_log(Info, false, "Array index: %lu", array_idx);
    migrator_log(Info, false, "Applying migration %s",
                 migrations[array_idx].path);

    if (apply_migration(db, sql_bufs[array_idx], next_version) < 0) {
      migrator_log(Error, false, "apply_migration failed");
      if (db_disconnect(db) < 0) {
        migrator_log(Error, false, "db_disconnect failed");
        return -1;
      }
      return -1;
    }
  }

  if (db_disconnect(db) < 0) {
    migrator_log(Error, false, "db_disconnect failed");
    return -1;
  }

  return 0;
}

int main(int argc, char **argv) {
  int return_status = 0;

  arena = arena_init(MiB(32), MiB(16));
  db_args = parse_args(arena, argc, argv);

  if (db_args->return_status != 0) {
    return_status = db_args->return_status;
    goto End;
  }

  if (db_args->clear_migrations && db_args->create_db) {
    migrator_log(Error, false,
                 "Cannot recreate and also create db at the same time");
    return_status = -1;
    goto End;
  }

  if (db_args->clear_migrations && db_args->create_db == false)
    clear_migrations(db_args->db_path);

  if (db_args->migration_name && db_args->migrations_path) {
    if (create_migration_file(arena, db_args->migration_name,
                              db_args->migrations_path) < 0)
      return_status = -1;
    else
      return_status = 0;
    goto End;
  }

  return_status = migrate();
  if (return_status < 0) {
    migrator_log(Error, false, "migrate failed");
    goto End;
  }

End:
  arena_destroy(arena);
  return return_status;
}
