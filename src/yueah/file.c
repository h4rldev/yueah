#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include <yueah/error.h>
#include <yueah/file.h>
#include <yueah/string.h>
#include <yueah/types.h>

yueah_string_t *get_cwd(h2o_mem_pool_t *pool, yueah_error_t *error) {
  static char cwd[1024] = {0};

  if (getcwd(cwd, 1024) == NULL) {
    *error = yueah_throw_error("Failed to get current working directory: %s",
                               strerror(errno));
    return NULL;
  }

  return yueah_string_new(pool, cwd, 1024);
}

yueah_error_t make_dir(h2o_mem_pool_t *pool, const yueah_string_t *path) {
  cstr *path_cstr = yueah_string_to_cstr(pool, path);

  if (mkdir(path_cstr, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
    return yueah_throw_error("Failed to create directory @ path: %s",
                             path_cstr);
  return yueah_success(NULL);
}

bool path_exist(h2o_mem_pool_t *pool, const yueah_string_t *path) {
  cstr *path_cstr = yueah_string_to_cstr(pool, path);

  struct stat sb;
  return (stat(path_cstr, &sb) == 0);
}

const yueah_string_t *yueah_getenv(h2o_mem_pool_t *pool, yueah_string_t *key,
                                   yueah_error_t *error) {
  const cstr *key_cstr = yueah_string_to_cstr(pool, key);
  const cstr *value_cstr = getenv(key_cstr);

  if (!value_cstr) {
    *error =
        yueah_throw_error("Failed to get environment variable %s", key_cstr);
    return NULL;
  }

  return yueah_string_new(pool, value_cstr, strlen(value_cstr));
}
