#include <log.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
  FILE *fd;
  char *path;
} state_console_target_t;

static state_console_target_t state_console_target = {NULL, NULL};

int __register_logger_fd(FILE *fd) {
  if (!fd)
    return -1;

  state_console_target.fd = fd;
  return 0;
}
int __register_logger_path(char *path) {
  if (!path)
    return -1;

  state_console_target.path = path;
  return 0;
}

static char *get_time(void) {
  static char time_str[32] = {0};
  struct tm local_time;

  time_t now = time(NULL);
  struct tm *time = localtime_r(&now, &local_time);
  if (!time)
    return NULL;

  snprintf(time_str, 32, "%02d/%02d/%02d-%02d:%02d:%02d", time->tm_mday,
           time->tm_mon + 1, time->tm_year + 1900, time->tm_hour, time->tm_min,
           time->tm_sec);

  return time_str;
}

void yueah_log(log_level_t level, bool time, const char *fmt, ...) {
#ifndef YUEAH_DEBUG
  if (level == Debug)
    return;
#endif

  va_list args;
  char *time_str;
  static char level_str[23] = {0};
  char fmt_buf[1024] = {0};
  char log_str[1024] = {0};

  FILE *log_file1 = NULL;
  FILE *log_file2 = NULL;

  if (!state_console_target.fd && !state_console_target.path) {
    fprintf(stderr, "No loggers registered\n");
    return;
  }

  if (state_console_target.fd)
    log_file1 = state_console_target.fd;

  if (state_console_target.path)
    log_file2 = fopen(state_console_target.path, "a");

  switch (level) {
  case Error:
    if (log_file1)
      snprintf(level_str, 23, "[%s%s ERR %s]:", FG_TRANS, COLOR_RED,
               COLOR_RESET);
    else
      strlcpy(level_str, "[ ERR ]:", 23);
    if (log_file1)
      log_file1 = stderr;
    break;
  case Warning:
    if (log_file1)
      snprintf(level_str, 23, "[%s%s WRN %s]:", FG_TRANS, COLOR_YELLOW,
               COLOR_RESET);
    else
      strlcpy(level_str, "[ WRN ]:", 23);
    break;
  case Info:
    if (log_file1)
      snprintf(level_str, 23, "[%s%s INF %s]:", FG_TRANS, COLOR_CYAN,
               COLOR_RESET);
    else
      strlcpy(level_str, "[ INF ]:", 23);
    break;
  case Debug:
    snprintf(level_str, 23, "[%s%s DEB %s]:", FG_TRANS, COLOR_BLUE,
             COLOR_RESET);
    break;
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

  if (log_file1)
    fprintf(log_file1, "%s\n", log_str);

  if (log_file2) {
    switch (level) {
    case Error:
      strlcpy(level_str, "[ DEB ]:", 23);
      break;

    case Warning:
      strlcpy(level_str, "[ WRN ]:", 23);
      break;

    case Info:
      strlcpy(level_str, "[ INF ]:", 23);
      break;

    case Debug:
      strlcpy(level_str, "[ DEB ]:", 23);
      break;
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

    fprintf(log_file2, "%s\n", log_str);
    fclose(log_file2);
  }
}
