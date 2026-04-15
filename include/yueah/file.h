#ifndef YUEAH_FILE_H
#define YUEAH_FILE_H

#include <stdbool.h>

#include <yueah/types.h>

/*
 * @brief Get current working directory
 *
 * @param pool Memory pool to allocate the string
 */
yueah_string_t *get_cwd(h2o_mem_pool_t *pool);

/*
 * @brief Make a directory
 *
 * @param pool Memory pool for conversion
 * @param path Path to the directory
 *
 * @return 0 on success, otherwise an error code
 */
int make_dir(h2o_mem_pool_t *pool, const yueah_string_t *path);

/*
 * @brief Check if a path exists
 *
 * @param pool Memory pool for conversion
 * @param path Path to check
 *
 * @return true if the path exists, false otherwise
 */
bool path_exist(h2o_mem_pool_t *pool, const yueah_string_t *path);

#endif // !YUEAH_FILE_H
