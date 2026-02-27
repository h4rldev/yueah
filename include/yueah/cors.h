#ifndef YUEAH_CORS_H
#define YUEAH_CORS_H

#include <stdbool.h>

#include <h2o.h>

#include <yueah/shared.h>

/*
 * Add CORS headers to the request
 *
 *
 * [req]  Request struct to add CORS headers to
 *
 * [config]  Configuration struct for CORS headers (usually gotten the req
 * handler)
 *
 *
 * Returns 0 on success
 */
int yueah_add_cors_headers(h2o_req_t *req, const yueah_cors_config_t *config);

/*
 * Handles OPTIONS requests for CORS
 *
 *
 * [handler]  Handler struct for OPTIONS request
 *
 * [req]  Request struct for OPTIONS request
 *
 *
 * Returns 0 on success (is meant to be returned in a registered handler when
 * the METHOD is OPTIONS)
 */
int yueah_handle_options(h2o_req_t *req, const yueah_cors_config_t *config);

#endif // !YUEAH_CORS_H
