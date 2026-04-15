#ifndef YUEAH_BASE64_H
#define YUEAH_BASE64_H

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Encode a string to base64
 *
 * @param pool The memory pool to allocate from
 * @param content The content to encode
 *
 * @return A yueah_string_t containing the base64 encoded content
 */
yueah_string_t *yueah_base64_encode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *content);

/*
 * @brief Decode a base64 string
 *
 * @param pool The memory pool to allocate from
 * @param base64 The base64 string to decode
 * @param max_out_len The maximum length to decode to
 *
 * @return A yueah_string_t containing the decoded content
 */
yueah_string_t *yueah_base64_decode(h2o_mem_pool_t *pool,
                                    const yueah_string_t *base64,
                                    u64 max_out_len);

#endif // !YUEAH_BASE64_H
