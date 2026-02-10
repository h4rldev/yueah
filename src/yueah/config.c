
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <h2o.h>
#include <yyjson.h>

#include <config.h>
#include <file.h>
#include <shared.h>

#define DEFAULT_PATH "/config/"
#define DEFAULT_LEVEL 6

static int handle_parse_err(char *categ, char *field) {
  fprintf(stderr,
          "Error parsing json, something went wrong on category %s, field %s\n",
          categ, field);
  return -1;
}

int init_config(h2o_mem_pool_t *pool, yueah_config_t **config) {
  yueah_config_t *local_config = h2o_mem_alloc_pool(pool, yueah_config_t, 1);

  local_config->db_path = yueah_strdup(pool, "./yueah.db", 1024);
  local_config->log_type =
      Both; // Console, File, Both are the available options
  local_config->network = h2o_mem_alloc_pool(pool, network_config_t, 1);
  local_config->network->ip = yueah_strdup(pool, "127.0.0.1", 16);
  local_config->network->port = 8080;

  local_config->compression = h2o_mem_alloc_pool(pool, compression_config_t, 1);
  local_config->compression->enabled = true;
  local_config->compression->quality = 6;
  local_config->compression->min_size = 1000;

  local_config->ssl = h2o_mem_alloc_pool(pool, ssl_config_t, 1);
  local_config->ssl->enabled = false;
  local_config->ssl->mem_cached = false;
  local_config->ssl->cert_path = h2o_mem_alloc_pool(pool, char, 1024);
  local_config->ssl->key_path = h2o_mem_alloc_pool(pool, char, 1024);

  *config = local_config;

  return 0;
}

int write_config(yueah_config_t *config) {
  yyjson_alc alc = {0};
  yyjson_write_err err;

  yyjson_mut_doc *doc;
  yyjson_mut_val *root;

  yyjson_mut_val *network_key;
  yyjson_mut_val *network_object;

  yyjson_mut_val *compression_key;
  yyjson_mut_val *compression_object;

  yyjson_mut_val *ssl_key;
  yyjson_mut_val *ssl_object;

  char *cwd;
  static char path[1024] = {0};
  char cwd_buf[1024] = {0};
  char json_buf[KiB(10)] = {0};
  char *default_path;
  bool write_res;

  default_path = DEFAULT_PATH;

  cwd = get_cwd();
  if (!cwd)
    return -1;

  strlcpy(cwd_buf, cwd, 1024);
  snprintf(path, 1024, "%s%s", cwd_buf, default_path);

  if (!path_exist(path))
    if (make_dir(path) != 0)
      return -1;

  strlcat(path, "config.json", 1024);

  if (config == NULL)
    return -1;

  yyjson_alc_pool_init(&alc, json_buf, KiB(10));

  doc = yyjson_mut_doc_new(NULL);
  root = yyjson_mut_obj(doc);

  network_key = yyjson_mut_str(doc, "network");
  network_object = yyjson_mut_obj(doc);

  compression_key = yyjson_mut_str(doc, "compression");
  compression_object = yyjson_mut_obj(doc);

  ssl_key = yyjson_mut_str(doc, "ssl");
  ssl_object = yyjson_mut_obj(doc);

  if (config->db_path == NULL)
    yyjson_mut_obj_add_str(doc, root, "db_path", "./yueah.db");
  else
    yyjson_mut_obj_add_str(doc, root, "db_path", config->db_path);

  switch (config->log_type) {
  case Both:
    yyjson_mut_obj_add_str(doc, root, "log_type", "both");
    break;
  case File:
    yyjson_mut_obj_add_str(doc, root, "log_type", "file");
    break;
  case Console:
    yyjson_mut_obj_add_str(doc, root, "log_type", "file");
    break;
  default:
    yyjson_mut_doc_free(doc);
    return -1;
  }

  if (config->network->ip == NULL)
    yyjson_mut_obj_add_str(doc, network_object, "ip", "127.0.0.1");
  else
    yyjson_mut_obj_add_str(doc, network_object, "ip", config->network->ip);

  if (config->network->port == 0 || !config->network->port)
    yyjson_mut_obj_add_int(doc, network_object, "port", 8080);
  else
    yyjson_mut_obj_add_int(doc, network_object, "port", config->network->port);

  yyjson_mut_obj_add_bool(doc, compression_object, "enabled",
                          config->compression->enabled);

  yyjson_mut_obj_add(root, network_key, network_object);

  if (config->compression->quality > 9 ||
      (config->compression->quality == 0 &&
       config->compression->enabled == true))
    yyjson_mut_obj_add_int(doc, compression_object, "quality", DEFAULT_LEVEL);
  else
    yyjson_mut_obj_add_int(doc, compression_object, "quality",
                           config->compression->quality);

  yyjson_mut_obj_add_int(doc, compression_object, "min_size",
                         config->compression->min_size);

  yyjson_mut_obj_add(root, compression_key, compression_object);

  yyjson_mut_obj_add_bool(doc, ssl_object, "enabled", config->ssl->enabled);
  yyjson_mut_obj_add_bool(doc, ssl_object, "mem_cached",
                          config->ssl->mem_cached);

  if (!config->ssl->cert_path) {
    yyjson_mut_obj_add_str(doc, ssl_object, "cert_path", "");
  } else {
    yyjson_mut_obj_add_str(doc, ssl_object, "cert_path",
                           config->ssl->cert_path);
  }

  if (!config->ssl->key_path) {
    yyjson_mut_obj_add_str(doc, ssl_object, "key_path", "");
  } else {
    yyjson_mut_obj_add_str(doc, ssl_object, "key_path", config->ssl->key_path);
  }

  yyjson_mut_val *ssl_enabled_key = yyjson_mut_str(doc, "enabled");
  yyjson_mut_val *ssl_enabled_val = yyjson_mut_bool(doc, false);

  if ((!config->ssl->key_path || !config->ssl->cert_path) &&
      config->ssl->enabled)
    yyjson_mut_obj_replace(ssl_object, ssl_enabled_key, ssl_enabled_val);

  yyjson_mut_obj_add(root, ssl_key, ssl_object);

  yyjson_mut_doc_set_root(doc, root);

  write_res = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY_TWO_SPACES,
                                    &alc, &err);
  if (!write_res) {
    fprintf(stderr, "Error writing config file: %d:%s\n", err.code, err.msg);
    yyjson_mut_doc_free(doc);
    return -1;
  }

  yyjson_mut_doc_free(doc);
  return 0;
}

int read_config(h2o_mem_pool_t *pool, yueah_config_t **config) {
  yyjson_alc alc = {0};
  yyjson_read_err err;
  char json_buf[KiB(10)] = {0};

  yyjson_doc *doc;
  yyjson_val *root;

  yyjson_val *db_path_val;
  yyjson_val *log_type_val;

  yyjson_val *network_object;
  yyjson_val *ip_val;
  yyjson_val *port_val;

  yyjson_val *compression_object;
  yyjson_val *compression_enabled_val;
  yyjson_val *quality_val;
  yyjson_val *min_size_val;

  yyjson_val *ssl_object;
  yyjson_val *ssl_enabled_val;
  yyjson_val *mem_cached_val;
  yyjson_val *cert_path_val;
  yyjson_val *key_path_val;

  char *db_path;
  log_type_t log_type;

  char *ip;
  uint16_t port;

  bool compression_enabled;
  uint8_t quality;
  uint64_t min_size;

  bool ssl_enabled;
  bool mem_cached;
  char *cert_path;
  char *key_path;

  yueah_config_t *local_config;

  char *default_path = DEFAULT_PATH;
  char path[1024];
  char *cwd;

  cwd = get_cwd();
  if (!cwd) {
    return -1;
  }

  yyjson_alc_pool_init(&alc, json_buf, KiB(10));

  local_config = h2o_mem_alloc_pool(pool, yueah_config_t, 1);
  local_config->network = h2o_mem_alloc_pool(pool, network_config_t, 1);
  local_config->compression = h2o_mem_alloc_pool(pool, compression_config_t, 1);
  local_config->ssl = h2o_mem_alloc_pool(pool, ssl_config_t, 1);

  snprintf(path, 1024, "%s%sconfig.json", cwd, default_path);
  if (!path_exist(path)) {
    return -1;
  }

  doc = yyjson_read_file(path, YYJSON_READ_NOFLAG, &alc, &err);
  if (!doc) {
    fprintf(stderr, "Error parsing json at %lu: %d:%s\n", err.pos, err.code,
            err.msg);
    return -1;
  }

  root = yyjson_doc_get_root(doc);

  db_path_val = yyjson_obj_get(root, "db_path");
  log_type_val = yyjson_obj_get(root, "log_type");

  network_object = yyjson_obj_get(root, "network");
  ip_val = yyjson_obj_get(network_object, "ip");
  port_val = yyjson_obj_get(network_object, "port");

  compression_object = yyjson_obj_get(root, "compression");
  compression_enabled_val = yyjson_obj_get(compression_object, "enabled");
  quality_val = yyjson_obj_get(compression_object, "quality");
  min_size_val = yyjson_obj_get(compression_object, "min_size");

  ssl_object = yyjson_obj_get(root, "ssl");
  ssl_enabled_val = yyjson_obj_get(ssl_object, "enabled");
  mem_cached_val = yyjson_obj_get(ssl_object, "mem_cached");
  cert_path_val = yyjson_obj_get(ssl_object, "cert_path");
  key_path_val = yyjson_obj_get(ssl_object, "key_path");

  if (!root) {
    yyjson_doc_free(doc);
    fprintf(stderr, "Error parsing json, root is null\n");
    return -1;
  }

  if (!db_path_val || !yyjson_is_str(db_path_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "db_path");
  }

  if (!log_type_val || !yyjson_is_str(log_type_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "log_type");
  }

  if (!network_object || !yyjson_is_obj(network_object)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "network");
  }

  if (!ip_val || !yyjson_is_str(ip_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("network", "ip");
  }

  if (!port_val || !yyjson_is_uint(port_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "port");
  }

  if (!compression_object || !yyjson_is_obj(compression_object)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "compression");
  }

  if (!compression_enabled_val || !yyjson_is_bool(compression_enabled_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("compression", "enabled");
  }

  if (!quality_val || !yyjson_is_uint(quality_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("compression", "quality");
  }

  if (!min_size_val || !yyjson_is_uint(min_size_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("compression", "min_size");
  }

  if (!ssl_object || !yyjson_is_obj(ssl_object)) {
    yyjson_doc_free(doc);
    return handle_parse_err("root", "ssl");
  }

  if (!ssl_enabled_val || !yyjson_is_bool(ssl_enabled_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("ssl", "enabled");
  }

  if (!mem_cached_val || !yyjson_is_bool(mem_cached_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("ssl", "mem_cached");
  }

  if (!cert_path_val || !yyjson_is_str(cert_path_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("ssl", "cert_path");
  }

  if (!key_path_val || !yyjson_is_str(key_path_val)) {
    yyjson_doc_free(doc);
    return handle_parse_err("ssl", "key_path");
  }

  db_path = yueah_strdup(pool, yyjson_get_str(db_path_val), 1024);
  if (strncasecmp("both", yyjson_get_str(log_type_val), 4) == 0)
    log_type = Both;
  else if (strncasecmp("file", yyjson_get_str(log_type_val), 4) == 0)
    log_type = File;
  else
    log_type = Console;

  ip = yueah_strdup(pool, yyjson_get_str(ip_val), 16);
  port = yyjson_get_uint(port_val);

  compression_enabled = yyjson_get_bool(compression_enabled_val);
  quality = yyjson_get_uint(quality_val);
  min_size = yyjson_get_uint(min_size_val);

  ssl_enabled = yyjson_get_bool(ssl_enabled_val);
  mem_cached = yyjson_get_bool(mem_cached_val);
  cert_path = yueah_strdup(pool, yyjson_get_str(cert_path_val), 1024);
  key_path = yueah_strdup(pool, yyjson_get_str(key_path_val), 1024);

  local_config->db_path = db_path;
  local_config->log_type = log_type;
  local_config->network->ip = ip;
  local_config->network->port = port;

  local_config->compression->enabled = compression_enabled;
  local_config->compression->quality = quality;
  local_config->compression->min_size = min_size;

  local_config->ssl->enabled = ssl_enabled;
  local_config->ssl->mem_cached = mem_cached;
  local_config->ssl->cert_path = cert_path;
  local_config->ssl->key_path = key_path;

  *config = local_config;

  yyjson_doc_free(doc);
  return 0;
}
