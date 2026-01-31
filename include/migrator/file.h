#ifndef YUEAH_MIGRATOR_FILE_H
#define YUEAH_MIGRATOR_FILE_H

#include <mem.h>
#include <stdio.h>

char *read_file_from_fd(mem_arena *arena, FILE *fd, mem_t *file_size);
char *read_file(mem_arena *arena, const char *path, mem_t *file_size);

bool is_path(const char *path);

#endif // !YUEAH_MIGRATOR_FILE_H
