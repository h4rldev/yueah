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
#define FG_TRANS "\x1b[30m"
#define COLOR_RED "\x1b[41m"
#define COLOR_GREEN "\x1b[42m"
#define COLOR_YELLOW "\x1b[43m"
#define COLOR_BLUE "\x1b[44m"
#define COLOR_CYAN "\x1b[46m"
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

#endif // !YUEAH_LOG_H
