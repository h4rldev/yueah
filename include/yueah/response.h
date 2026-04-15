#ifndef YUEAH_RESPONSE_H
#define YUEAH_RESPONSE_H

#include <h2o.h>
#include <yueah/types.h>

/*
 * @brief Parse the form body of a post request
 *
 * @param pool Memory pool for allocation
 * @param input The body of the post request
 * @param error Error to set if something goes wrong
 *
 * @return a yueah_string_array_t containing the parsed form data
 */
yueah_string_array_t *yueah_parse_form_body(h2o_mem_pool_t *pool,
                                            const yueah_string_t *form_body,
                                            yueah_error_t *error);

/*
 * @brief Get value of a key from a parsed post body
 *
 * @param pool Memory pool for allocation
 * @param key The key to search for
 * @param input The body of the request form
 * @param error Error to set if something goes wrong
 *
 * @return a yueah_string_t with the value of the key.
 */
yueah_string_t *yueah_get_form_val(h2o_mem_pool_t *pool,
                                   const yueah_string_t *key,
                                   const yueah_string_array_t *form_data,
                                   yueah_error_t *error);

/*
 * @brief Send a generic json response to a request
 *
 * @param req The request to send the response to
 * @param status The status code of the response
 * @param message The message to send with the response
 *
 * Returns 0 on success (should be infallible)
 */
int yueah_generic_response(h2o_req_t *req, u16 status,
                           const yueah_string_t *message);

#endif // !YUEAH_RESPONSE_H
