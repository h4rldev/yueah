#ifndef YUEAH_URL_H
#define YUEAH_URL_H

#include <yueah/types.h>

/*
 * @brief Decode a url encoded cstring
 *
 * @note This function doesn't populate a yueah_error_t, so if something goes
 * wrong, you'll have to troubleshoot yourself.
 *
 * @param dst The destination to write the decoded string to
 * @param src The source to decode
 */
void yueah_urldecode(cstr *dst, const cstr *src);

#endif // !YUEAH_URL_H
