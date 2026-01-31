#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>

#include <migrator/log.h>
#include <migrator/mem.h>
#include <migrator/sql.h>

/// Utility functions

static int compare_identifiers(const void *a, const void *b) {
  const migration_t *mig_a = (const migration_t *)a;
  const migration_t *mig_b = (const migration_t *)b;

  if (mig_a->identifier < mig_b->identifier)
    return -1;
  if (mig_a->identifier > mig_b->identifier)
    return 1;
  return 0;
}

static id_type_t identify_id(mem_t id) {
  if (id < 19700101000000ULL)
    return ID_U64;

  return TIMESTAMP_U64;
}

static int validate_migration_format(migration_t *migrations, mem_t len) {
  if (len == 0)
    return -1;

  // Use first migration to determine expected format
  id_type_t expected_format = identify_id(migrations[0].identifier);

  // Check all migrations match
  for (mem_t i = 1; i < len; i++) {
    id_type_t current_format = identify_id(migrations[i].identifier);

    if (current_format != expected_format) {
      migrator_log(Error, false, "Mixed migration formats found!\n");
      migrator_log(
          Error, false, "Migration '%s' uses %s format, but expected %s\n",
          migrations[i].path,
          current_format == TIMESTAMP_U64 ? "timestamp" : "sequential ID",
          expected_format == TIMESTAMP_U64 ? "timestamp" : "sequential ID");
      return -1;
    }
  }

  return 0;
}

static int handle_step(sqlite3 *db, int rc) {
  if (rc != SQLITE_ROW)
    switch (rc) {
    case SQLITE_BUSY:
      migrator_log(Info, false, "Database is busy, waiting 5 seconds");
      if (sqlite3_busy_timeout(db, 5000) == SQLITE_BUSY) {
        migrator_log(Error, false, "Database is still busy, quitting");
        goto Error;
      }
      break;
    case SQLITE_ERROR:
      migrator_log(Error, false, "sqlite3_step failed: %d:%s",
                   sqlite3_errcode(db), sqlite3_errmsg(db));
      goto Error;

    case SQLITE_MISUSE:
      migrator_log(Error, false, "sqlite3_step misuse");
      goto Error;
    }

  return 0;

Error:
  return -1;
}
/// !Utility functions

int db_connect(const char *db_path, sqlite3 **db, bool rw, bool create) {
  int res;

  if (create && rw)
    res = sqlite3_open_v2(db_path, db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
  else if (create == false && rw)
    res = sqlite3_open_v2(db_path, db, SQLITE_OPEN_READWRITE, NULL);
  else
    res = sqlite3_open_v2(db_path, db, SQLITE_OPEN_READONLY, NULL);

  if (*db == NULL || res != SQLITE_OK) {
    migrator_log(Error, false, "sqlite3_open_v2 failed\n");
    return -1;
  }

  return 0;
}

int db_disconnect(sqlite3 *db) {
  int rc = SQLITE_OK;
  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    return 0;

  migrator_log(Error, false, "sqlite3_close failed");
  migrator_log(Error, false, "%d:%s", sqlite3_errcode(db), sqlite3_errmsg(db));
  return -1;
}

int clear_migrations(char *db_path) {
  sqlite3 *db;

  if (remove(db_path) < 0) {
    migrator_log(Error, false, "Failed to remove migrations: %s",
                 strerror(errno));
    return -1;
  }

  if (db_connect(db_path, &db, true, true) < 0) {
    migrator_log(Error, false, "db_connect failed");
    return -1;
  }

  if (db_disconnect(db) < 0) {
    migrator_log(Error, false, "db_disconnect failed");
    return -1;
  }

  return 0;
}

int get_user_version(sqlite3 *db) {
  int user_version = 0;
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    migrator_log(Error, false, "sqlite3_prepare_v2 failed: %d:%s",
                 sqlite3_errcode(db), sqlite3_errmsg(db));
    if (stmt)
      sqlite3_finalize(stmt);

    return -1;
  }

  rc = sqlite3_step(stmt);
  if (handle_step(db, rc) < 0)
    goto Error;

  user_version = sqlite3_column_int(stmt, 0);

  sqlite3_finalize(stmt);
  return user_version;

Error:
  sqlite3_finalize(stmt);
  return -1;
}

int set_user_version(sqlite3 *db, int user_version) {
  sqlite3_stmt *stmt;
  int rc;
  char *set_version =
      sqlite3_mprintf("PRAGMA user_version = %d;", user_version);

  rc = sqlite3_prepare_v2(db, set_version, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    migrator_log(Error, false, "sqlite3_prepare_v2 failed: %d:%s",
                 sqlite3_errcode(db), sqlite3_errmsg(db));
    goto Error;
  }

  rc = sqlite3_step(stmt);
  if (handle_step(db, rc) < 0)
    goto Error;

  sqlite3_free(set_version);
  sqlite3_finalize(stmt);
  return 0;
Error:
  sqlite3_free(set_version);
  sqlite3_finalize(stmt);
  return -1;
}

int begin_transaction(sqlite3 *db) {
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "BEGIN TRANSACTION;", -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    migrator_log(Error, false, "sqlite3_prepare_v2 failed: %d:%s",
                 sqlite3_errcode(db), sqlite3_errmsg(db));
    goto Error;
  }

  rc = sqlite3_step(stmt);
  if (handle_step(db, rc) < 0)
    goto Error;

  if (stmt)
    sqlite3_finalize(stmt);

  return 0;

Error:
  if (stmt)
    sqlite3_finalize(stmt);
  return -1;
}

int run_sql(mem_arena *arena, sqlite3 *db, char *sql) {
  char **tokens;
  int token_count, rc;
  sqlite3_stmt *stmt = NULL;

  if (!db || !sql)
    return -1;

  tokens = arena_split_by_delim(arena, sql, ';', &token_count);

  migrator_log(Debug, false, "Running %d sql statements", token_count);
  for (int i = 0; i < token_count; i++) {
    migrator_log(Debug, false, "Running sql: %s", tokens[i]);
    rc = sqlite3_prepare_v2(db, tokens[i], -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
      migrator_log(Error, false, "sqlite3_prepare_v2 failed: %d:%s",
                   sqlite3_errcode(db), sqlite3_errmsg(db));
      goto Error;
    }
    rc = sqlite3_step(stmt);
    if (handle_step(db, rc) < 0)
      goto Error;

    migrator_log(Debug, false, "Wrote %s to db", tokens[i]);
    if (stmt)
      sqlite3_finalize(stmt);
  }

  return 0;

Error:
  if (stmt)
    sqlite3_finalize(stmt);
  return -1;
}

int commit_transaction(sqlite3 *db) {
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2(db, "COMMIT;", -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    migrator_log(Error, false, "sqlite3_prepare_v2 failed: %d:%s",
                 sqlite3_errcode(db), sqlite3_errmsg(db));
    goto Error;
  }

  rc = sqlite3_step(stmt);
  if (handle_step(db, rc) < 0)
    goto Error;

  sqlite3_finalize(stmt);

  return 0;

Error:
  if (stmt)
    sqlite3_finalize(stmt);
  return -1;
}

/// IO

migration_t *find_migrations(mem_arena *arena, const char *path,
                             mem_t *file_count) {
  migration_t *migrations;
  mem_t count = 0, pos = 0, entry_idx = 0;
  struct dirent *entry;
  char canonical_path[1024] = {0};
  DIR *dir;

  dir = opendir(path);
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    if (entry->d_type == DT_REG)
      count++;
  }

  if (!count) {
    migrator_log(Error, "No files found in %s\n", path);

    closedir(dir);
    return NULL;
  }

  pos = arena->position;
  migrations = arena_push_array(arena, migration_t, count, false);

  // flush to reread the directory
  rewinddir(dir);

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    if (entry->d_type == DT_REG && strstr(entry->d_name, ".sql")) {
      migrator_log(Debug, false, "Found %s", entry->d_name);
      snprintf(canonical_path, 1024, "%s/%s", path, entry->d_name);
      migrations[entry_idx].path = arena_strdup(arena, canonical_path, 1024);
      sscanf(entry->d_name, "%lu_", &migrations[entry_idx].identifier);
      entry_idx++;
    }
  }

  if (migrations[entry_idx - 1].path == NULL) {
    migrator_log(Error, false, "Failed to find sql files in migrations dir\n");

    closedir(dir);
    arena_pop_to(arena, pos);
    return NULL;
  }

  closedir(dir);

  qsort(migrations, entry_idx, sizeof(migration_t), compare_identifiers);

  if (validate_migration_format(migrations, entry_idx) < 0) {
    migrator_log(Error, false, "validate_migration_format failed");
    return NULL;
  }

  *file_count = entry_idx;
  return migrations;
}

/// !IO
