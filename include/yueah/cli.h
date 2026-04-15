#ifndef YUEAH_CLI_H
#define YUEAH_CLI_H

#include <stdbool.h>

#include <yueah/config.h>
#include <yueah/types.h>

/*
 * @brief Parse the command line arguments
 *
 * @param argc The number of arguments
 * @param argv The argument vector
 * @param[out] populated_args The populated arguments
 *
 * @return yueah_error_t containing status.OK if no issues happened, else a
 * populated error
 */
yueah_error_t parse_args(int argc, char **argv,
                         yueah_config_t **populated_args);

#endif // !YUEAH_CLI_H
