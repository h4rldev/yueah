#ifndef YUEAH_CLI_H
#define YUEAH_CLI_H

#include <stdbool.h>

#include <yueah/config.h>

int parse_args(int argc, char **argv, yueah_config_t **populated_args);

#endif // !YUEAH_CLI_H
