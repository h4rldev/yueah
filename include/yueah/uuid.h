#ifndef YUEAH_UUID_H
#define YUEAH_UUID_H

#include <h2o.h>

#include <yueah/types.h>

/*
 * @brief Create a new uuidv4
 *
 * @note This function doesn't populate a yueah_error_t, so if something goes
 * wrong, you'll have to troubleshoot yourself.
 *
 * @param pool Memory pool to allocate from
 *
 * @return A yueah_string_t containing the new uuid
 */
yueah_string_t *yueah_uuid_new(h2o_mem_pool_t *pool);

#endif // !YUEAH_UUID_H
