#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <yueah/cli.h>
#include <yueah/config.h>
#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/meta.h>
#include <yueah/string.h>
#include <yueah/types.h>

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

         "%s, %s\n"
         "This program is licensed under %s\n",
         __AUTHOR__, current_year, __LICENSE__);
  exit(0);
}

yueah_error_t parse_args(int argc, char **argv,
                         yueah_config_t **populated_args) {
  int option_index = 0;
  char *ip_buf = NULL;
  unsigned int port_buf = 0;

  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"ip-address", required_argument, 0, 'i'},
      {"port", required_argument, 0, 'p'},
      {0, 0, 0, 0}}; // end options_arr

  while (1) {
    char arg = getopt_long(argc, argv, ":hi:p:", long_options, &option_index);

    if (arg == -1)
      break;

    switch (arg) {
    case 'h':
      usage();
      break;

    case 'i':
      ip_buf = optarg;
      if (strlen(ip_buf) > 15) {
        return yueah_throw_error("Unknown ip address \"%s\"\n", ip_buf);
      } else {
        (*populated_args)->network->ip->data = (u8 *)ip_buf;
        (*populated_args)->network->ip->len = strlen(ip_buf);
      }
      break;

    case 'p':
      port_buf = atoi(optarg);
      if (port_buf <= 0 || port_buf > 65535) {
        return yueah_throw_error("Unknown port \"%s\"\n", optarg);
      } else {
        (*populated_args)->network->port = port_buf;
      }
      break;
    case ':':
      switch (optopt) {
      case 'i':
        return yueah_throw_error("Missing param for -i/--ip-address\n");
      case 'p':
        return yueah_throw_error("Missing param for -p/--port\n");
      default:
        return yueah_throw_error("Unknown option \"%c\"\n", optopt);
      }
      break;

    case '?':
      return yueah_throw_error("Invalid flag passed \"%s\"", optopt);

    default:
      return yueah_throw_error("?? getopt returned character code 0%o ??",
                               optopt);
    }
  }

  return yueah_success(NULL);
}
