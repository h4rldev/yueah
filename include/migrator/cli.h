#ifndef YUEAH_MIGRATOR_CLI_H
#define YUEAH_MIGRATOR_CLI_H

#include <stdbool.h>

#include <mem.h>

typedef struct {
  char *migrations_path;
  char *db_path;
  int return_status;
  bool create_db;
  bool clear_migrations;
} db_args_t;

db_args_t *parse_args(mem_arena *arena, int argc, char **argv);

#endif // !YUEAH_MIGRATOR_CLI_H
