#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <migrator/log.h>

static char *get_time(void) {
  static char time_str[32] = {0};
  struct tm local_time;

  time_t now = time(NULL);
  struct tm *time = localtime_r(&now, &local_time);
  if (!time)
    return NULL;

  snprintf(time_str, 32, "%d/%02d/%02d-%02d:%02d:%02d", time->tm_mday,
           time->tm_mon + 1, time->tm_year + 1900, time->tm_hour, time->tm_min,
           time->tm_sec);

  return time_str;
}

int migrator_log(log_level_t level, bool time, const char *fmt, ...) {
#ifndef YUEAH_DEBUG
  if (level == Debug)
    return 0;
#endif

  va_list args;
  char *time_str;
  static char level_str[23] = {0};
  char fmt_buf[1024] = {0};
  char log_str[1024] = {0};
  FILE *log_file = stdout;

  switch (level) {
  case Error:
    snprintf(level_str, 23, "[%s%s ERR %s]:", FG_TRANS, COLOR_RED, COLOR_RESET);
    log_file = stderr;
    break;
  case Warning:
    snprintf(level_str, 23, "[%s%s WRN %s]:", FG_TRANS, COLOR_YELLOW,
             COLOR_RESET);
    break;
  case Info:
    snprintf(level_str, 23, "[%s%s INF %s]:", FG_TRANS, COLOR_CYAN,
             COLOR_RESET);
    break;
  case Debug:
    snprintf(level_str, 23, "[%s%s DEB %s]:", FG_TRANS, COLOR_BLUE,
             COLOR_RESET);
  }

  if (time) {
    time_str = get_time();
    snprintf(fmt_buf, 1024, "[%s] - %s %s", time_str, level_str, fmt);
  } else {
    snprintf(fmt_buf, 1024, "%s %s", level_str, fmt);
  }

  va_start(args, fmt);
  vsnprintf(log_str, 1024, fmt_buf, args);
  va_end(args);

  return fprintf(log_file, "%s\n", log_str);
}
