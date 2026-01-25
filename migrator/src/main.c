#include <mem.h>
#include <migrator/cli.h>
#include <migrator/log.h>

static mem_arena *arena;

int migrate(db_args_t *db_args) { return 0; }

int main(int argc, char **argv) {
  int return_status = 0;
  arena = arena_init(MiB(32), MiB(16));

  db_args_t *db_args = parse_args(arena, argc, argv);
  if (db_args->return_status != 0) {
    return_status = db_args->return_status;
    arena_destroy(arena);
    return return_status;
  }

  migrator_log(Info, "db_args->return_status: %d", db_args->return_status);
  migrator_log(Info, "db_args->migrations_path: %s", db_args->migrations_path);
  migrator_log(Info, "db_args->db_path: %s", db_args->db_path);

  arena_destroy(arena);
  return 0;
}
