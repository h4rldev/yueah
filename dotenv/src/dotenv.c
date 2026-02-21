#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dotenv.h>

static void trim(char *str) {
  char *start = str;
  while (isspace(*start))
    start++;

  while (start != str)
    memmove(str, start, strlen(start) + 1);

  char *end = str + strlen(str) - 1;
  while (end > str && isspace(*end)) {
    *end = '\0';
    end--;
  }
}

static void to_upper(char *str) {
  char *c = str;

  while (*c) {
    *c = toupper(*c);
    c++;
  }
}

static int remove_quotes(char *str) {
  size_t len = strlen(str);

  if (len < 2)
    return 0;

  if ((str[0] == '"' && str[len - 1] == '"') ||
      (str[0] == '\'' && str[len] == '\'')) {
    str[len - 1] = '\0';
    memmove(str, str + 1, len - 1);
  }

  return 0;
}

static int parse_line(char *line, char *key, char *value) {
  if (line[0] == '#' || line[0] == '\n')
    return -1;

  if (sscanf(line, "%[^=]=%[^\n]", key, value) != 2)
    return -1;

  trim(key);
  to_upper(key);
  trim(value);
  remove_quotes(value);

  if (strlen(key) > 0 && strlen(value) > 0)
    return 0;

  return -1;
}

int load_dotenv(const char *path) {
  size_t path_len = strlen(path);
  char line[1024] = {0};
  char key[256] = {0};
  char value[767] = {0};
  int matches = 0;

  if (path_len == 0 || path_len > 1024)
    return -1;

  FILE *fd = fopen(path, "r");
  if (!fd)
    return -1;

  while (!feof(fd)) {
    while (fgets(line, 1024, fd) != NULL) {
      if (parse_line(line, key, value) == -1)
        continue;

      if (setenv(key, value, 1) != 0) {
        fprintf(stderr, "Failed to set env %s=%s\n", key, value);
        return -1;
      }

      matches++;
    }
  }

  fclose(fd);
  return matches;
}
