#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include <mem.h>
#include <migrator/file.h>

char *read_file_from_fd(mem_arena *arena, FILE *fd) {
  size_t file_size = 0;
  (void)fseek(fd, 0L, SEEK_END);
  file_size = ftell(fd);
  (void)fseek(fd, 0L, SEEK_SET);

  mem_t pos = arena->position;
  char *buf = arena_push_array(arena, char, file_size + 1);

  if (fread(buf, 1, file_size + 1, fd) != file_size) {
    fprintf(stderr, "Amount read is more or less than file_size, quitting\n");
    fclose(fd);
    arena_pop_to(arena, pos);
    return NULL;
  }

  fclose(fd);

  return buf;
}

char *read_file(mem_arena *arena, const char *path) {
  FILE *fp = fopen(path, "rb");

  return read_file_from_fd(arena, fp);
}

/*char **crawl_one_dir(mem_arena *arena, const char *path) {
  char **filepaths = {0};
  struct dirent *entry;
  DIR *dir;

  dir = opendir(path);
}*/

bool is_path(const char *path) {
  struct stat sb;
  return (stat(path, &sb) == 0);
}
