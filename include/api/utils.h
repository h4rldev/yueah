#ifndef YUEAH_API_UTILS_H
#define YUEAH_API_UTILS_H

#include <h2o.h>

#include <yueah/shared.h>

typedef struct {
  char *message;
  int status;
} error_response_t;

/*
 * Decodes a url encoded string
 *
 *
 * [dst]       The destination to write the decoded string to
 * [src]       The source to decode
 *
 *
 * Returns nothing
 */
void urldecode(char *dst, const char *src);

/*
 * Get value of a key from a parsed post body
 *
 *
 * [pool]      Memory pool to allocate the array from
 *
 * [key]       The key to search for
 *
 * [input]     The body of the post request
 *
 * [val_len]   The length of the value gotten
 *
 *
 * Returns the value of the key and populates the val_len with the length of the
 * value
 */
char *get_form_val(h2o_mem_pool_t *pool, const char *key, char **input);

/*
 * Parses the body of a post request
 *
 *
 * [pool]      Memory pool to allocate the array from
 *
 * [input]     The body of the post request
 *
 * [input_len] The length of the body
 *
 *
 * Returns an array of strings terminated with a null which is at the last byte
 * of the array
 */
char **parse_post_body(h2o_mem_pool_t *pool, const char *input,
                       mem_t input_len);

/*
 * Send a generic json response to a request
 *
 *
 * [req]       The request to send the response to
 *
 * [status]    The status code of the response
 *
 * [message]   The message to send with the response
 *
 * Returns 0 on success (should be infallible)
 */
int generic_response(h2o_req_t *req, int status, const char *message);

#endif // !YUEAH_API_SHARED_H
