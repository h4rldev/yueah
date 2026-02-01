#ifndef YUEAH_MIGRATOR_LOG_H
#define YUEAH_MIGRATOR_LOG_H

#include <stdbool.h>

typedef enum {
  Error,
  Warning,
  Info,
  Debug,
} log_level_t;

#ifndef YUEAH_MIGRATOR_NO_LOG_COLORS
#define COLOR_RESET "\x1b[0m"
#define FG_TRANS "\x1b[30m"
#define COLOR_RED "\x1b[41m"
#define COLOR_GREEN "\x1b[42m"
#define COLOR_YELLOW "\x1b[43m"
#define COLOR_BLUE "\x1b[44m"
#define COLOR_CYAN "\x1b[46m"
#endif

int migrator_log(log_level_t level, bool time, const char *fmt, ...);

#endif // !YUEAH_MIGRATOR_LOG_H
