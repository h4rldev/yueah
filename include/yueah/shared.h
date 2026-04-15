#ifndef YUEAH_SHARED_H
#define YUEAH_SHARED_H

#include <h2o.h>

#include <yueah/types.h>

// Data conversion macros
#define KiB(num) ((mem_t)(num) << 10)
#define MiB(num) ((mem_t)(num) << 20)
#define GiB(num) ((mem_t)(num) << 30)

/*
 * @brief Duplicate a cstr
 *
 * @param pool The memory pool to allocate from
 * @param str The string to duplicate
 * @param size The size of the string
 *
 * @return A cstr containing the duplicated string
 */
cstr *yueah_cstrdup(h2o_mem_pool_t *pool, const cstr *str, u64 size);

/*
 * @brief Print a hex dump of a string
 *
 * @param label The label to print before the hex dump
 * @param str The string to print
 * @param size The size of the string
 */
void print_hex(const char *label, const yueah_string_t *str);

/*
 * @brief Convert a h2o_iovec to a cstr
 *
 * @param pool The memory pool to allocate from
 * @param iovec The iovec to convert
 *
 * @return A null terminated cstr containing the data the iovec pointed to.
 */
cstr *yueah_iovec_to_cstr(h2o_mem_pool_t *pool, h2o_iovec_t *iovec);

/*
 * @brief Convert a h2o_iovec to a yueah_string_t
 *
 * @param pool The memory pool to allocate from
 * @param iovec The iovec to convert
 *
 * @return A yueah_string_t containing the data the iovec pointed to.
 */
yueah_string_t *yueah_iovec_to_string(h2o_mem_pool_t *pool, h2o_iovec_t *iovec);

#endif // !YUEAH_SHARED_
