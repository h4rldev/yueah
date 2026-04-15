#ifndef YUEAH_COOKIE_H
#define YUEAH_COOKIE_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Construct a Set-Cookie header with given arguments
 *
 * @param pool The memory pool to allocate from
 * @param cookie_name The name of the cookie
 * @param content The content of the cookie
 * @param mask The bitmask to set cookie attributes, see yueah_cookie_bitmask_t
 * @param ...  Additional arguments for bitmask, e.g.
 * yueah_cookie_new(pool, contents, SAME_SITE, LAX)
 *
 * @return A yueah_string_t containing the Cookie header.
 */
yueah_string_t *yueah_cookie_new(h2o_mem_pool_t *pool,
                                 const yueah_string_t *cookie_name,
                                 const yueah_string_t *content,
                                 yueah_cookie_mask mask, ...);

/*
 * @brief Check if cookie name exists in cookie header
 *
 * @param cookie_header The cookie header to check
 * @param cookie_name The name of the cookie to check
 *
 * @return true if the cookie name exists, false otherwise
 */
bool yueah_cookie_name_exists(h2o_mem_pool_t *pool, h2o_iovec_t cookie_header,
                              const yueah_string_t *cookie_name);

/*
 * @brief Get the content of a cookie
 *
 * @param pool The memory pool to allocate from
 * @param cookie_header The cookie header to get the content from
 * @param cookie_name The name of the cookie to get the content from
 *
 * @return A yueah_string_t containing the content of the cookie
 */
yueah_string_t *yueah_get_cookie_content(h2o_mem_pool_t *pool,
                                         const yueah_string_t *cookie_header,
                                         const yueah_string_t *cookie_name);

/*
 * @brief Get the content of a cookie from a h2o request
 *
 * @param req The request to get the cookie from
 * @param cookie_name The name of the cookie to get
 *
 * @return The content of the cookie or NULL if the cookie was not found
 */
yueah_string_t *yueah_req_get_cookie_content(h2o_req_t *req,
                                             const yueah_string_t *cookie_name);

/*
 * @brief Deletes the cookie header from a request
 *
 * @param req The request to delete the cookie from
 *
 * @return 0 on success, -1 on failure
 */
int yueah_req_delete_cookie(h2o_req_t *req);

#endif // !YUEAH_COOKIE_H
