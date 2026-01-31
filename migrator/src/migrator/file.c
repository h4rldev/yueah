#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <migrator/file.h>
#include <migrator/log.h>
#include <migrator/mem.h>

char *read_file_from_fd(mem_arena *arena, FILE *fd, mem_t *file_size) {
  size_t file_size_buf = 0;
  (void)fseek(fd, 0L, SEEK_END);
  file_size_buf = ftell(fd);
  (void)fseek(fd, 0L, SEEK_SET);

  mem_t pos = arena->position;
  char *buf = arena_push_array(arena, char, file_size_buf, false);

  if (fread(buf, 1, file_size_buf, fd) != file_size_buf) {
    migrator_log(Error, false,
                 "Amount read is more or less than file_size, quitting");
    fclose(fd);
    arena_pop_to(arena, pos);
    return NULL;
  }

  *file_size = file_size_buf;
  fclose(fd);
  return buf;
}

char *read_file(mem_arena *arena, const char *path, mem_t *file_size) {
  migrator_log(Debug, false, "Reading %s", path);
  FILE *fp = fopen(path, "r");
  if (!fp) {
    migrator_log(Error, false, "read_file failed");
    return NULL;
  }

  return read_file_from_fd(arena, fp, file_size);
}

bool is_path(const char *path) {
  struct stat sb;
  return (stat(path, &sb) == 0);
}
