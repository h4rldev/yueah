#include <jansson.h>
#include <mem.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <config.h>
#include <file.h>

#define DEFAULT_PATH "/config/"
#define DEFAULT_LEVEL 6

static int handle_parse_err(char *categ, char *field) {
  fprintf(stderr,
          "JSON didn't read properly, something went wrong on category %s, "
          "field %s\n",
          categ, field);
  return -1;
}

static json_t *bool_to_json_bool(bool b) {
  if (b)
    return json_true();
  else
    return json_false();
}

int init_config(mem_arena *arena, yueah_config_t **config) {
  yueah_config_t *local_config = arena_push_struct(arena, yueah_config_t);

  local_config->log_type =
      Both; // Console, File, Both are the available options
  local_config->network = arena_push_struct(arena, network_config_t);
  local_config->network->ip = arena_strdup(arena, "127.0.0.1", 16);
  local_config->network->port = 8080;

  local_config->compression = arena_push_struct(arena, compression_config_t);
  local_config->compression->enabled = true;
  local_config->compression->quality = 6;
  local_config->compression->min_size = 150;

  local_config->ssl = arena_push_struct(arena, ssl_config_t);
  local_config->ssl->enabled = false;
  local_config->ssl->mem_cached = false;
  local_config->ssl->cert_path = arena_push_array(arena, char, 1024);
  local_config->ssl->key_path = arena_push_array(arena, char, 1024);

  *config = local_config;

  return 0;
}

int write_config(yueah_config_t *config) {
  char *default_path = DEFAULT_PATH;
  char *cwd = get_cwd();
  static char path[1024];
  char cwd_buf[1024] = {0};

  if (!cwd)
    return -1;

  strlcpy(cwd_buf, cwd, 1024);
  snprintf(path, 1024, "%s%s", cwd_buf, default_path);

  if (path_exist(path) == false) {
    if (make_dir(path) != 0) {
      return -1;
    }
  }

  strlcat(path, "config.json", 1024);

  if (config == NULL)
    return -1;

  json_t *root = json_object();
  json_t *network_object = json_object();
  json_t *compression_object = json_object();
  json_t *ssl_object = json_object();

  switch (config->log_type) {
  case Both:
    json_object_set_new(root, "log_type", json_string("both"));
    break;
  case File:
    json_object_set_new(root, "log_type", json_string("file"));
    break;
  case Console:
    json_object_set_new(root, "log_type", json_string("console"));
    break;
  default:
    json_decref(root);
    return -1;
  }

  if (config->network->ip == NULL)
    json_object_set_new(network_object, "ip", json_string("127.0.0.1"));
  else
    json_object_set_new(network_object, "ip", json_string(config->network->ip));

  if (config->network->port == 0 || config->network->port > 65535)
    json_object_set_new(network_object, "port", json_integer(8080));
  else
    json_object_set_new(network_object, "port",
                        json_integer(config->network->port));

  json_object_set_new(compression_object, "enabled",
                      bool_to_json_bool(config->compression->enabled));

  if (config->compression->quality > 9 ||
      (config->compression->quality == 0 &&
       config->compression->enabled == true))
    json_object_set_new(compression_object, "quality",
                        json_integer(DEFAULT_LEVEL));
  else
    json_object_set_new(compression_object, "quality",
                        json_integer(config->compression->quality));

  json_object_set_new(compression_object, "min_size",
                      json_integer(config->compression->min_size));
  json_object_set_new(ssl_object, "enabled",
                      bool_to_json_bool(config->ssl->enabled));
  json_object_set_new(ssl_object, "mem_cached",
                      bool_to_json_bool(config->ssl->mem_cached));

  if (!config->ssl->cert_path) {
    json_object_set_new(ssl_object, "cert_path", json_string(""));
    json_object_set(ssl_object, "enabled", json_false());
  } else {
    json_object_set_new(ssl_object, "cert_path",
                        json_string(config->ssl->cert_path));
  }

  if (!config->ssl->key_path) {
    json_object_set_new(ssl_object, "key_path", json_string(""));
    json_object_set(ssl_object, "enabled", json_false());
  } else {
    json_object_set_new(ssl_object, "key_path",
                        json_string(config->ssl->key_path));
  }

  json_object_set_new(root, "network", network_object);
  json_object_set_new(root, "compression", compression_object);
  json_object_set_new(root, "ssl", ssl_object);

  FILE *file = fopen(path, "w");
  json_dumpf(root, file, JSON_INDENT(2));

  fclose(file);
  json_decref(root);

  return 0;
}

int read_config(mem_arena *arena, yueah_config_t **config) {
  json_t *root;
  json_error_t error;

  yueah_config_t *local_config = arena_push_struct(arena, yueah_config_t);
  local_config->network = arena_push_struct(arena, network_config_t);
  local_config->compression = arena_push_struct(arena, compression_config_t);
  local_config->ssl = arena_push_struct(arena, ssl_config_t);

  char *default_path = DEFAULT_PATH;

  char *cwd = get_cwd();
  if (!cwd) {
    return -1;
  }

  char path[1024];

  snprintf(path, 1024, "%s%sconfig.json", cwd, default_path);
  if (!path_exist(path)) {
    return -1;
  }

  root = json_load_file(path, 0, &error);
  if (!root) {
    fprintf(stderr, "Error parsing json on line %d: %s\n", error.line,
            error.text);
    return -1;
  }

  json_t *log_type_enum = json_object_get(root, "log_type");

  int log_type = Both;
  if (json_is_string(log_type_enum)) {
    if (strncasecmp("both", json_string_value(log_type_enum), 4) == 0)
      log_type = Both;
    else if (strncasecmp("file", json_string_value(log_type_enum), 4) == 0)
      log_type = File;
    else
      log_type = Console;
  } else {
    json_decref(root);
    return handle_parse_err("root", "log_type");
  }

  json_t *network_object = json_object_get(root, "network");
  if (!json_is_object(network_object)) {
    json_decref(root);
    return -1;
  }

  json_t *ip_string = json_object_get(network_object, "ip");

  char *ip;
  if (json_is_string(ip_string)) {
    ip = arena_strdup(arena, (char *)json_string_value(ip_string), 16);
  } else {
    json_decref(root);
    return handle_parse_err("network", "ip");
  }

  json_t *port_integer = json_object_get(network_object, "port");

  unsigned int port = 0;
  if (json_is_integer(port_integer)) {
    port = json_integer_value(port_integer);
  } else {
    json_decref(root);

    return handle_parse_err("root", "port");
  }

  json_t *compression_object = json_object_get(root, "compression");
  if (!json_is_object(compression_object)) {
    json_decref(root);
    return -1;
  }

  json_t *compression_enabled_bool =
      json_object_get(compression_object, "enabled");

  bool compression_enabled = false;
  if (json_is_boolean(compression_enabled_bool)) {
    compression_enabled = json_boolean_value(compression_enabled_bool);
  } else {
    json_decref(root);

    return handle_parse_err("compression", "enabled");
  }

  json_t *compression_quality_uint =
      json_object_get(compression_object, "quality");

  unsigned int compression_quality = DEFAULT_LEVEL;
  if (json_is_integer(compression_quality_uint)) {
    compression_quality = json_integer_value(compression_quality_uint);
  } else {
    json_decref(root);

    return handle_parse_err("compression", "quality");
  }

  json_t *compression_min_size_uint =
      json_object_get(compression_object, "min_size");

  unsigned int compression_min_size = 150;
  if (json_is_integer(compression_min_size_uint)) {
    compression_min_size = json_integer_value(compression_quality_uint);
  } else {
    json_decref(root);

    return handle_parse_err("compression", "min_size");
  }

  json_t *ssl_object = json_object_get(root, "ssl");
  if (!json_is_object(ssl_object)) {
    json_decref(root);

    return -1;
  }

  json_t *ssl_enabled_bool = json_object_get(ssl_object, "enabled");

  bool ssl_enabled = false;
  if (json_is_boolean(ssl_enabled_bool)) {
    ssl_enabled = json_boolean_value(ssl_enabled_bool);
  } else {
    json_decref(root);

    return handle_parse_err("ssl", "enabled");
  }

  json_t *mem_cached_bool = json_object_get(ssl_object, "mem_cached");

  bool mem_cached = false;
  if (json_is_boolean(mem_cached_bool)) {
    mem_cached = json_boolean_value(mem_cached_bool);
  } else {
    json_decref(root);
    return handle_parse_err("ssl", "mem_cached");
  }

  json_t *cert_path_string = json_object_get(ssl_object, "cert_path");

  char *cert_path;
  if (json_is_string(cert_path_string)) {
    cert_path =
        arena_strdup(arena, (char *)json_string_value(cert_path_string), 1024);
  } else {
    json_decref(root);
    return handle_parse_err("ssl", "cert_path");
  }

  json_t *key_path_string = json_object_get(ssl_object, "key_path");

  char *key_path;
  if (json_is_string(key_path_string)) {
    key_path =
        arena_strdup(arena, (char *)json_string_value(key_path_string), 1024);
  } else {
    json_decref(root);
    return handle_parse_err("ssl", "key_path");
  }

  local_config->log_type = log_type;
  local_config->network->ip = ip;
  local_config->network->port = port;

  local_config->compression->enabled = compression_enabled;
  local_config->compression->quality = compression_quality;
  local_config->compression->min_size = compression_min_size;

  local_config->ssl->enabled = ssl_enabled;
  local_config->ssl->mem_cached = mem_cached;
  local_config->ssl->cert_path = cert_path;
  local_config->ssl->key_path = key_path;

  *config = local_config;

  json_decref(root);
  return 0;
}
