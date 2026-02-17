#ifndef YUEAH_LOG_H
#define YUEAH_LOG_H

#include <stdbool.h>
#include <stdio.h>

typedef enum { LOG_TO_FD, LOG_TO_PATH } yueah_log_type_t;
typedef struct {
  yueah_log_type_t type;
  FILE *fd;
  char *path;
} log_target_t;

typedef enum {
  Error,
  Warning,
  Info,
  Debug,
} log_level_t;

#ifndef YUEAH_NO_LOG_COLORS
#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"
#else
#define COLOR_RESET ""
#define FG_TRANS ""
#define COLOR_RED ""
#define COLOR_GREEN ""
#define COLOR_YELLOW ""
#define COLOR_BLUE ""
#define COLOR_CYAN ""
#endif

int __register_logger_fd(FILE *fd);
int __register_logger_path(char *path);

void yueah_log(log_level_t level, bool time, const char *fmt, ...);

#define register_logger(target)                                                \
  _Generic((target),                                                           \
      FILE *: __register_logger_fd,                                            \
      char *: __register_logger_path)(target)

#define yueah_log_error(fmt, ...) yueah_log(Error, true, fmt, ##__VA_ARGS__)
#define yueah_log_warning(fmt, ...) yueah_log(Warning, true, fmt, ##__VA_ARGS__)
#define yueah_log_info(fmt, ...) yueah_log(Info, true, fmt, ##__VA_ARGS__)
#define yueah_log_debug(fmt, ...) yueah_log(Debug, true, fmt, ##__VA_ARGS__)
#define yueah_log_error_tl(fmt, ...) yueah_log(Error, false, fmt, ##__VA_ARGS__)
#define yueah_log_warning_tl(fmt, ...)                                         \
  yueah_log(Warning, false, fmt, ##__VA_ARGS__)
#define yueah_log_info_tl(fmt, ...) yueah_log(Info, false, fmt, ##__VA_ARGS__)
#define yueah_log_debug_tl(fmt, ...) yueah_log(Debug, false, fmt, ##__VA_ARGS__)

#endif // !YUEAH_LOG_H
