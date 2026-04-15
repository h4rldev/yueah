#ifndef YUEAH_CLI_H
#define YUEAH_CLI_H

#include <stdbool.h>

#include <yueah/config.h>

/*
 * @brief Parse the command line arguments
 *
 * @param argc The number of arguments
 * @param argv The argument vector
 * @param[out] populated_args The populated arguments
 *
 * @return 0 on success
 */
int parse_args(int argc, char **argv, yueah_config_t **populated_args);

#endif // !YUEAH_CLI_H
