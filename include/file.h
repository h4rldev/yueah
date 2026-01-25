#ifndef YUEAH_FILE_H
#define YUEAH_FILE_H

#include <mem.h>
#include <stdbool.h>
// #include <stdio.h>

char *get_mime(const char *path);
char *get_cwd(void);

int make_dir(const char *path);
bool path_exist(const char *path);

/*
bool is_dir(const char *path);
bool is_file(const char *path);
char *read_file_from_fd(mem_arena *arena, FILE *file);
char *read_file(mem_arena *arena, const char *path);
*/

#endif // !YUEAH_FILE_H
