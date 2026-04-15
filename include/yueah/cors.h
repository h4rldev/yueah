#ifndef YUEAH_CORS_H
#define YUEAH_CORS_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Add CORS headers to a request
 *
 * @param req Request struct to add CORS headers to
 * @param config Configuration struct for CORS headers (usually gotten through
 * req handler)
 *
 * @return 0 on success
 */
int yueah_add_cors_headers(h2o_req_t *req, const yueah_cors_config_t *config);

/*
 * Handles OPTIONS requests for CORS
 *
 * @note This function is meant to be returned when a handler recieves an
 * OPTIONS request
 *
 * @param handler The h2o handler for the OPTIONS request
 * @param req The h2o request for the OPTIONS request
 *
 * @return 0 on success.
 */
int yueah_handle_options(h2o_req_t *req, const yueah_cors_config_t *config);

#endif // !YUEAH_CORS_H
