#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <migrator/cli.h>
#include <migrator/file.h>
#include <migrator/log.h>
#include <migrator/mem.h>
#include <migrator/meta.h>

void print_usage(void) {
  printf("Usage: %s [<OPTIONS>] <migrations> <db>\n"
         "\n"
         "Options:\n"
         "  -h, --help\t\tShow this help message and exit\n"
         "  -v, --version\t\tShow version information and exit\n"
         "  -c, --create\t\tCreate database file if it doesn't exist\n"
         "  -r, --remove\t\tClear migrations\n"
         "\n"
         "  -m, --migrations\tPath to migration files\n"
         "  -d, --db\t\tPath to database file\n"
         "  -n, --new\t\tCreate a new migration file with name\n"
         "\n"
         "Made with <3 by %s\n",
         PROG_NAME, PROG_AUTHOR);
}

void print_version(void) { printf("%s %s\n", PROG_NAME, PROG_VERSION); }

db_args_t *parse_args(mem_arena *arena, int argc, char **argv) {
  int option_index = 0;
  char arg = 0;
  bool making_new_migration = false;
  db_args_t *db_args;

  char temp_migrations_path[1024] = {0};
  char temp_db_path[1024] = {0};
  char temp_migration_name[1024] = {0};

  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"new", required_argument, NULL, 'n'},
      {"create", no_argument, NULL, 'c'},
      {"remove", no_argument, NULL, 'r'},
      {"migrations", required_argument, NULL, 'm'},
      {"db", required_argument, NULL, 'd'},
      {0, 0, 0, 0},
  };

  db_args = arena_push_struct(arena, db_args_t, false);
  if (argc < 2) {
    print_usage();
    goto Ok;
  }

  while ((arg = getopt_long(argc, argv, ":hvm:d:n:r", long_options,
                            &option_index)) != -1) {
    switch (arg) {
    case 'v':
      print_version();
      goto Ok;

    case 'h':
      print_usage();
      goto Ok;

    case 'c':
      db_args->create_db = true;
      break;

    case 'r':
      db_args->clear_migrations = true;
      migrator_log(Info, false, "Clearing migrations and reapplying");
      break;

    case 'n':
      if (!optarg) {
        migrator_log(Error, false, "(--new/-n) requires a migration name");
        print_usage();
        goto Error;
      }

      making_new_migration = true;
      strlcpy(temp_migration_name, optarg, 1024);
      migrator_log(Info, false, "Making new migration: %s",
                   temp_migration_name);
      break;

    case 'm':
      if (!optarg) {
        migrator_log(Error, false, "(--migrations/-m) requires a path");
        print_usage();
        goto Error;
      }

      strlcpy(temp_migrations_path, optarg, 1024);
      break;

    case 'd':
      if (!optarg) {
        migrator_log(Error, false, "(--db/-d) requires a path");
        print_usage();
        goto Error;
      }

      strlcpy(temp_db_path, optarg, 1024);
      break;

    case '?':
      migrator_log(Error, false, "Unknown option: %c\n", optopt);
      print_usage();
      goto Error;

    default:
      migrator_log(Error, false,
                   "Unknown error, getopt returned character: 0%o\n", arg);
      goto Error;
    }
  }

  if ((strlen(temp_db_path) == 0 || strlen(temp_migrations_path) == 0) &&
      optind != argc - 2 && making_new_migration == false) {
    migrator_log(Debug, false, "temp_db_path: %s\n",
                 !temp_db_path[0] ? "empty" : "full");
    migrator_log(Debug, false, "temp_migrations_path: %s\n",
                 temp_migrations_path[0] == 0 ? "empty" : "full");
    migrator_log(Debug, false, "optind: %d\n", optind);
    migrator_log(Debug, false, "argc: %d\n", argc);
    migrator_log(Debug, false, "making_new_migration: %s\n",
                 making_new_migration ? "true" : "false");

    migrator_log(Error, false, "Missing path arguments\n");
    print_usage();
    goto Error;
  }

  if (strlen(temp_migrations_path) != 0) {
    migrator_log(Debug, false, "temp_migrations_path is not empty");
    if (!is_path(temp_migrations_path)) {
      migrator_log(Error, false, "migrations_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->migrations_path = arena_strdup(arena, temp_migrations_path, 1024);
  } else if (strlen(argv[optind]) != 0) {

    if (!is_path(argv[optind])) {
      migrator_log(Error, false, "migrations_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->migrations_path = arena_strdup(arena, argv[optind], 1024);
  }

  if (strlen(temp_db_path) != 0) {
    if (!is_path(temp_db_path) && db_args->create_db == false) {
      migrator_log(Error, false, "db_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->db_path = arena_strdup(arena, temp_db_path, 1024);
  } else if (strlen(argv[optind + 1]) != 0 && db_args->create_db == false) {
    if (!is_path(argv[optind + 1]) && db_args->create_db == false) {
      migrator_log(Error, false, "db_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->db_path = arena_strdup(arena, argv[optind + 1], 1024);
  }

  if (temp_migration_name[0] && temp_migrations_path[0])
    db_args->migration_name = arena_strdup(arena, temp_migration_name, 1024);

Ok:
  db_args->return_status = 0;
  return db_args;

Error:
  db_args->return_status = 1;
  return db_args;
}
