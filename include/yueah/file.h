#ifndef YUEAH_FILE_H
#define YUEAH_FILE_H

#include <stdbool.h>

#include <yueah/types.h>

/*
 * @brief Get current working directory
 *
 * @param pool Memory pool to allocate the string
 * @param error Error to set if something goes wrong
 */
yueah_string_t *get_cwd(h2o_mem_pool_t *pool, yueah_error_t *error);

/*
 * @brief Make a directory
 *
 * @param pool Memory pool for conversion
 * @param path Path to the directory
 *
 * @return a yueah_error_t containing status.OK if no issues happened, else a
 * populated error
 */
yueah_error_t make_dir(h2o_mem_pool_t *pool, const yueah_string_t *path);

/*
 * @brief Check if a path exists
 *
 * @param pool Memory pool for conversion
 * @param path Path to check
 *
 * @return true if the path exists, false otherwise
 */
bool path_exist(h2o_mem_pool_t *pool, const yueah_string_t *path);

/*
 * @brief Get an environment variable
 *
 * @param pool Memory pool for conversion
 * @param key The key to get
 * @param error Error to set if something goes wrong
 *
 * @return a yueah_string_t containing the value of the key
 */
const yueah_string_t *yueah_getenv(h2o_mem_pool_t *pool, yueah_string_t *key,
                                   yueah_error_t *error);

#endif // !YUEAH_FILE_H
