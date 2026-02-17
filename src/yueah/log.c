#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <yueah/log.h>

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
  static char level_str[20] = {0};
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
    snprintf(level_str, 20, "%s[ERROR]%s:", COLOR_RED, COLOR_RESET);
    if (log_file1)
      log_file1 = stderr;
    break;
  case Warning:
    snprintf(level_str, 20, " %s[WARN]%s:", COLOR_YELLOW, COLOR_RESET);
    break;
  case Info:
    snprintf(level_str, 20, " %s[INFO]%s:", COLOR_CYAN, COLOR_RESET);
    break;
  case Debug:
    snprintf(level_str, 20, "%s[DEBUG]%s:", COLOR_BLUE, COLOR_RESET);
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
      strlcpy(level_str, "[ERROR]:", 20);
      break;

    case Warning:
      strlcpy(level_str, " [WARN]:", 20);
      break;

    case Info:
      strlcpy(level_str, " [INFO]:", 20);
      break;

    case Debug:
      strlcpy(level_str, "[DEBUG]:", 20);
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
