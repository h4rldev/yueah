#ifndef YUEAH_MIGRATOR_FILE_H
#define YUEAH_MIGRATOR_FILE_H

#include <mem.h>
#include <stdio.h>
char *read_file_from_fd(mem_arena *arena, FILE *fd);
char *read_file(mem_arena *arena, const char *path);
bool is_path(const char *path);

#endif // !YUEAH_MIGRATOR_FILE_H
