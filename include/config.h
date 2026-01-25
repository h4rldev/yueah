#ifndef YUEAH_CONFIG_H
#define YUEAH_CONFIG_H

#include <mem.h>
#include <stdbool.h>

typedef enum { File, Console, Both } log_type_t;

typedef struct {
  char *ip;
  uint16_t port;
} network_config_t;

typedef struct {
  bool enabled;
  uint8_t quality;
  mem_t min_size;
} compression_config_t;

typedef struct {
  bool enabled;
  bool mem_cached;
  char *cert_path;
  char *key_path;
} ssl_config_t;

typedef struct {
  char *db_path;
  log_type_t log_type;
  network_config_t *network;
  compression_config_t *compression;
  ssl_config_t *ssl;
} yueah_config_t;

int init_config(mem_arena *arena, yueah_config_t **config);

int read_config(mem_arena *arena, yueah_config_t **config);
int write_config(yueah_config_t *config);

#endif // !YUEAH_CONFIG_H
