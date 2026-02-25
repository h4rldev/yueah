#ifndef YUEAH_MIGRATOR_CLI_H
#define YUEAH_MIGRATOR_CLI_H

#include <stdbool.h>

#include <migrator/mem.h>
#include <migrator/sql.h>

typedef struct {
  bool help : 1;
  bool version : 1;
  bool create : 1;
  bool remove : 1;
  bool migrations_path_added : 1;
  bool db_path_added : 1;
  bool create_migration : 1;
  bool create_db : 1;
  bool clear_migrations : 1;

  char *migrations_path, *db_path, *migration_name;
  int return_status;
} db_args_t;

db_args_t *parse_args(mem_arena *arena, int argc, char **argv);

#endif // !YUEAH_MIGRATOR_CLI_H
