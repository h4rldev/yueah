#include <stdbool.h>
#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <file.h>

char *get_cwd(void) {
  static char cwd[1024] = {0};

  if (getcwd(cwd, 1024) == NULL) {
    perror("getcwd");
    return NULL;
  }

  return cwd;
}

int make_dir(const char *path) {
  if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    fprintf(stderr, "Failed to create directory @ path: %s\n", path);
    return -1;
  }
  return 0;
}

bool path_exist(const char *path) {
  struct stat sb;
  return (stat(path, &sb) == 0);
}

/*
bool is_dir(const char *path) {
  struct stat st;
  return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

bool is_file(const char *path) {
  struct stat st;
  return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

char *read_file_from_fd(mem_arena *arena, FILE *file) {
  size_t file_size = 0;
  (void)fseek(file, 0L, SEEK_END);
  file_size = ftell(file);
  (void)fseek(file, 0L, SEEK_SET);

  mem_t pos = arena->position;
  char *buf = arena_push_array(arena, char, file_size + 1);

  if (fread(buf, 1, file_size + 1, file) != file_size) {
    fprintf(stderr, "Amount read is more or less than file_size, quitting\n");
    fclose(file);
    arena_pop_to(arena, pos);
    return NULL;
  }

  fclose(file);

  return buf;
}

char *read_file(mem_arena *arena, const char *path) {
  FILE *fp = fopen(path, "rb");

  return read_file_from_fd(arena, fp);
}
*/
