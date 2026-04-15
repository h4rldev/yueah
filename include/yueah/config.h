#ifndef YUEAH_CONFIG_H
#define YUEAH_CONFIG_H

#include <stdbool.h>

#include <yueah/types.h>

/*
 * @brief Initialize the config
 *
 * @param pool The memory pool to allocate from
 * @param config The config to initialize
 *
 * @return 0 on success
 */
int init_config(h2o_mem_pool_t *pool, yueah_config_t **config);

/*
 * @brief Read the config
 *
 * @param pool The memory pool to allocate from
 * @param config The config to read
 * @param path The path to the config file (CAN BE NULL to use default path
 * (current directory))
 *
 * @return 0 on success
 */
int read_config(h2o_mem_pool_t *pool, yueah_config_t **config,
                yueah_string_nullable_t *path);

/*
 * @brief Write the config
 *
 * @param config The config to write
 * @param path The path to write the config to (CAN BE NULL to use default path
 * (current directory))
 *
 * @return 0 on success
 */
int write_config(yueah_config_t *config, yueah_string_nullable_t *path);

#endif // !YUEAH_CONFIG_H
