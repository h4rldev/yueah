#ifndef YUEAH_STRING_H
#define YUEAH_STRING_H

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Create a new string.
 *
 * @param pool The memory pool to allocate the string from.
 * @param str The string to copy into the new string (CAN BE NULL).
 * @param size The size of the string.
 *
 * @return The new string.
 */
yueah_string_t *yueah_string_new(h2o_mem_pool_t *pool, const cstr_nullable *str,
                                 u64 size);

/*
 * @brief Convert string to cstr.
 *
 * @param pool The memory pool to allocate the cstr from.
 * @param str The string to convert.
 *
 * @return The cstr.
 */
cstr *yueah_string_to_cstr(h2o_mem_pool_t *pool, const yueah_string_t *str);

/*
 * @brief Convert from iovec to string.
 *
 * @param pool The memory pool to allocate the string from.
 * @param iovec The iovec to convert.
 *
 * @return The new string.
 */
yueah_string_t *yueah_string_from_iovec(h2o_mem_pool_t *pool,
                                        const h2o_iovec_t *iovec);

/*
 * @brief Convert from string to iovec.
 *
 * @param pool The memory pool to allocate the iovec from.
 * @param str The string to convert.
 *
 * @return The new iovec.
 */
h2o_iovec_t *yueah_string_to_iovec(h2o_mem_pool_t *pool,
                                   const yueah_string_t *str);

/*
 * @brief Convert all characters in a yueah_string_t to lowercase.
 *
 * @param str The string to convert.
 */
void yueah_string_lower(yueah_string_t *str);

/*
 * @brief Convert all characters in a yueah_string_t to uppercase.
 *
 * @param str The string to convert.
 */
void yueah_string_upper(yueah_string_t *str);

// Macro to make string new a lil easier to use.
#define YUEAH_STR(str) yueah_string_new(pool, str, sizeof(str) - 1)

// Macro alias for H2O_STRLIT
#define YUEAH_STRLIT(str) H2O_STRLIT(str)

// Macro to make string new a lil easier to use.
#define YUEAH_SSTRLIT(yueah_str) (cstr *)yueah_str->data, yueah_str->len

#endif // !YUEAH_STRING_H
