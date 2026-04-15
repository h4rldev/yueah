#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <yueah/file.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *get_cwd(h2o_mem_pool_t *pool) {
  static char cwd[1024] = {0};

  if (getcwd(cwd, 1024) == NULL) {
    perror("getcwd");
    return NULL;
  }

  return yueah_string_new(pool, cwd, 1024);
}

int make_dir(h2o_mem_pool_t *pool, const yueah_string_t *path) {
  cstr *path_cstr = yueah_string_to_cstr(pool, path);

  if (mkdir(path_cstr, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    fprintf(stderr, "Failed to create directory @ path: %s\n", path_cstr);
    return -1;
  }
  return 0;
}

bool path_exist(h2o_mem_pool_t *pool, const yueah_string_t *path) {
  cstr *path_cstr = yueah_string_to_cstr(pool, path);

  struct stat sb;
  return (stat(path_cstr, &sb) == 0);
}
