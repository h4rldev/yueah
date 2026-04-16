#ifndef YUEAH_ERROR_H
#define YUEAH_ERROR_H

#include <h2o.h>
#include <yueah/types.h>

// Macro that gets the current file, not the whole path.
#define FILENAME                                                               \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/*
 * @brief Throw an error (Not meant for calling directly)
 *
 * @param status The status of the error.
 * @param file The file the error occurred in.
 * @param function The function the error occurred in.
 * @param line The line the error occurred on.
 * @param error_message_fmt The message fmt to print.
 * @param ... The arguments for the message fmt.
 *
 * @return A yueah_error_t containing the error.
 */
yueah_error_t __yueah_throw_error(const yueah_error_status_t status,
                                  const cstr *file, const cstr *function,
                                  const u64 line, const cstr *error_message_fmt,
                                  ...);

/*
 * @brief Throw an error (This is what you're supposed to use)
 *
 * @param error_message The message to print.
 *
 * @return A yueah_error_t containing the error.
 */
#define yueah_throw_error(error_message_fmt, ...)                              \
  __yueah_throw_error(ERROR, FILENAME, __func__, __LINE__, error_message_fmt,  \
                      ##__VA_ARGS__)

/*
 * @brief Print an error to stderr.
 *
 * @param error The error to print.
 */
void yueah_print_error(yueah_error_t error);

/*
 * @brief Return a success, meaning no error.
 *
 * @param message The message to return for info purposes, can be NULL.
 * @param ... The arguments for the message fmt.
 *
 * @return A yueah_error_t containing the optional message and an OK status.
 */
yueah_error_t yueah_success(const cstr_nullable *message_fmt, ...);
#endif // !YUEAH_ERROR_H
