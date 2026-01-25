#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mem.h>
#include <migrator/cli.h>
#include <migrator/file.h>
#include <migrator/log.h>
#include <migrator/meta.h>

void print_usage(void) {
  printf("Usage: %s [<OPTIONS>] <migrations> <db>\n"
         "\n"
         "Options:\n"
         "  -h, --help\t\tShow this help message and exit\n"
         "  -v, --version\t\tShow version information and exit\n"
         "  -n, --new\t\tCreate a new migration file\n"
         "  -c, --create\t\tCreate database file if it doesn't exist\n"
         "\n"
         "  -m, --migrations\tPath to migration files\n"
         "  -d, --db\t\tPath to database file\n"
         "\n"
         "Made with <3 by %s\n",
         PROG_NAME, PROG_AUTHOR);
}

void print_version(void) { printf("%s %s\n", PROG_NAME, PROG_VERSION); }

db_args_t *parse_args(mem_arena *arena, int argc, char **argv) {
  int option_index = 0;
  char arg = 0;
  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"new", no_argument, NULL, 'n'},
      {"create", no_argument, NULL, 'c'},
      {"migrations", required_argument, NULL, 'm'},
      {"db", required_argument, NULL, 'd'},
      {0, 0, 0, 0},
  };
  db_args_t *db_args = arena_push_struct(arena, db_args_t);

  if (argc < 2) {
    print_usage();
    exit(0);
  }

  while ((arg = getopt_long(argc, argv, ":hvm:d:", long_options,
                            &option_index)) != -1) {
    switch (arg) {
    case 'v':
      print_version();
      goto Ok;

    case 'h':
      print_usage();
      goto Ok;

    case 'm':
      if (optarg == NULL) {
        print_usage();
        goto Error;
      }

      if (!is_path(optarg)) {
        migrator_log(Error, "migrations_path is not a path\n");
        print_usage();
        goto Error;
      }

      db_args->migrations_path = arena_strdup(arena, optarg, 1024);
      break;

    case 'd':
      if (optarg == NULL) {
        print_usage();
        goto Error;
      }

      if (!is_path(optarg)) {
        migrator_log(Error, "db_path is not a path\n");
        print_usage();
        goto Error;
      }

      db_args->db_path = arena_strdup(arena, optarg, 1024);
      break;

    case '?':
      migrator_log(Error, "Unknown option: %c\n", optopt);
      print_usage();
      goto Error;

    default:
      migrator_log(Error, "Unknown error, getopt returned character: 0%o\n",
                   arg);
      return NULL;
    }
  }

  if ((!db_args->migrations_path || !db_args->db_path) && optind != argc - 2) {
    migrator_log(Error, "Missing path arguments\n");
    print_usage();
    goto Error;
  }

  if (!db_args->migrations_path) {
    if (!is_path(argv[optind])) {
      migrator_log(Error, "migrations_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->migrations_path = arena_strdup(arena, argv[optind], 1024);
  }

  if (!db_args->db_path) {
    if (!is_path(argv[optind + 1])) {
      migrator_log(Error, "db_path is not a path\n");
      print_usage();
      goto Error;
    }

    db_args->db_path = arena_strdup(arena, argv[optind + 1], 1024);
  }

Ok:
  db_args->return_status = 0;
  return db_args;

Error:
  db_args->return_status = 1;
  return db_args;
}
