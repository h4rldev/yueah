#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cli.h>
#include <config.h>
#include <meta.h>
#include <time.h>

static void get_current_year(char (*buf)[5]) {
  struct tm local_time;
  time_t now = time(NULL);
  localtime_r(&now, &local_time);

  snprintf(*buf, 5, "%d", local_time.tm_year + 1900);
}

static void usage(void) {
  char current_year[5];
  get_current_year(&current_year);

  printf("USAGE: yueah [OPTIONS]\n"
         "Options:\n"
         "    -h, --help                     displays this message\n"
         "    -i, --ip-address [address]     override ip address specified in "
         "config\n"
         "    -p, --port       [port number] override port \n"
         "    -s, --ssl                      toggles HTTPS/SSL (needs a cert "
         "and key in path)\n"
         "    -c, --compress                 toggles gzip compression\n"
         "    -v, --verbose                  toggles verbose messaging "
         "(enables logs)\n\n"

         "%s, %s\n"
         "This program is licensed under %s\n",
         __AUTHOR__, current_year, __LICENSE__);
  exit(0);
}

int parse_args(int argc, char **argv, yueah_config_t **populated_args) {
  int option_index = 0;
  char *ip_buf = {0};
  unsigned int port_buf = 0;

  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"ip-address", required_argument, 0, 'i'},
      {"port", required_argument, 0, 'p'},
      // requires no arg due to being bool;
      {"ssl", no_argument, 0, 's'},
      {"compress", no_argument, 0, 'c'},
      {"verbose", no_argument, 0, 'v'},
      {0, 0, 0, 0}}; // end options_arr

  while (1) {
    char arg =
        getopt_long(argc, argv, ":hi:p:scv", long_options, &option_index);

    if (arg == -1)
      break;

    switch (arg) {
    case 'h':
      usage();
      break;

    case 'i':
      ip_buf = optarg;
      if (strlen(ip_buf) > 15) {
        fprintf(stderr, "Unknown ip address \"%s\"\n", ip_buf);
        return -1;
      } else {
        (*populated_args)->network->ip = ip_buf;
      }
      break;

    case 'p':
      port_buf = atoi(optarg);
      if (port_buf <= 0 || port_buf > 65535) {
        fprintf(stderr, "Unknown port \"%s\"\n", optarg);
        return -1;
      } else {
        (*populated_args)->network->port = port_buf;
      }
      break;

    case 's':
      (*populated_args)->ssl->enabled = !(*populated_args)->ssl->enabled;
      break;

    case 'c':
      (*populated_args)->compression->enabled =
          !(*populated_args)->compression->enabled;
      break;

    case '?':
      fprintf(stderr, "invalid flag passed \"%s\"\n", argv[optind - 1]);
      return 0;

    default:
      printf("?? getopt returned character code 0%o ??\n", arg);
    }
  }

  return 0;
}
