#ifndef CONFIG_H_IMPLEMENTATION
#define CONFIG_H_IMPLEMENTATION

#include <mem.h>
#include <stdbool.h>

typedef struct {
  char *ip;
  unsigned int port;
} network_config_t;

typedef struct {
  bool enabled;
  unsigned int quality;
  unsigned int min_size;
} compression_config_t;

typedef struct {
  bool enabled;
  bool mem_cached;
  char *cert_path;
  char *key_path;
} ssl_config_t;

typedef struct {
  enum { File, Console, Both } log_type;
  network_config_t *network;
  compression_config_t *compression;
  ssl_config_t *ssl;
} yueah_config_t;

int init_config(mem_arena *arena, yueah_config_t **config);

int read_config(mem_arena *arena, yueah_config_t **config);
int write_config(yueah_config_t *config);

#endif // !CONFIG_H_IMPLEMENTATION
