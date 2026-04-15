#ifndef YUEAH_SHARED_H
#define YUEAH_SHARED_H

#include <h2o.h>

#include <yueah/types.h>

// Data conversion macros
#define KiB(num) ((u64)(num) << 10)
#define MiB(num) ((u64)(num) << 20)
#define GiB(num) ((u64)(num) << 30)

/*
 * @brief Print a hex dump of a string
 *
 * @param label The label to print before the hex dump
 * @param str The string to print
 * @param size The size of the string
 */
void print_hex(const char *label, const yueah_string_t *str);

#endif // !YUEAH_SHARED_
