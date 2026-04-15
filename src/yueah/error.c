#include <stdarg.h>

#include <h2o.h>

#include <yueah/error.h>
#include <yueah/log.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_error_t __yueah_throw_error(const yueah_error_status_t status,
                                  const cstr *file, const cstr *function,
                                  const u64 line, const cstr *error_message_fmt,
                                  ...) {
  va_list args;
  static char error_message[1024] = {0};

  va_start(args, error_message_fmt);
  vsnprintf(error_message, 1024, error_message_fmt, args);
  va_end(args);

  static yueah_error_t error;

  error.status = status;
  error.message = error_message;
  error.file = file;
  error.function = function;
  error.line = line;

  return error;
}

void yueah_print_error(yueah_error_t error) {
  if (error.status == OK) {
    yueah_log_warning(
        "Printing an error when there was none, might be a sign of misuse");
    yueah_log_info("%s", error.message);
    return;
  }

  yueah_log_error("(%s:%lu:%s) Error: %s", error.file, error.line,
                  error.function, error.message);
}

yueah_error_t yueah_success(const cstr_nullable *message_fmt, ...) {
  va_list args;
  char message[1024] = {0};

  static yueah_error_t error = {
      .status = OK,
      .file = NULL,
      .function = NULL,
      .message = NULL,
      .line = 0,
  };

  if (message_fmt) {
    va_start(args, message_fmt);
    vsnprintf(message, 1024, message_fmt, args);
    va_end(args);
    error.message = message;
  }

  return error;
}
