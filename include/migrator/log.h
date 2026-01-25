#ifndef YUEAH_MIGRATOR_LOG_H
#define YUEAH_MIGRATOR_LOG_H

typedef enum {
  Error,
  Warning,
  Info,
  Debug,
} log_level_t;

#ifndef YUEAH_MIGRATOR_NO_LOG_COLORS
#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"
#endif

void migrator_log_time(log_level_t level, const char *fmt, ...);
void migrator_log(log_level_t level, const char *fmt, ...);

#endif // !YUEAH_MIGRATOR_LOG_H
