#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include <h2o.h>
#include <yyjson.h>

#include <yueah/config.h>
#include <yueah/error.h>
#include <yueah/file.h>
#include <yueah/log.h>
#include <yueah/shared.h>
#include <yueah/string.h>
#include <yueah/types.h>

#define DEFAULT_PATH YUEAH_STR("/config/")
#define DEFAULT_LEVEL 6

static yueah_error_t handle_parse_err(char *categ, char *field) {
  return yueah_throw_error(
      "Error parsing json, something went wrong on category %s, field %s",
      categ, field);
}

yueah_error_t init_config(h2o_mem_pool_t *pool, yueah_config_t **config) {
  yueah_config_t *local_config = h2o_mem_alloc_pool(pool, yueah_config_t, 1);

  local_config->db_path = YUEAH_STR("./yueah.db");
  local_config->log_type =
      Both; // Console, File, Both are the available options
  local_config->network = h2o_mem_alloc_pool(pool, network_config_t, 1);
  local_config->network->ip = YUEAH_STR("127.0.0.1");
  local_config->network->port = 8080;

  *config = local_config;

  return yueah_success(NULL);
}

yueah_error_t write_config(h2o_mem_pool_t *pool, yueah_config_t *config,
                           yueah_string_nullable_t *path) {
  yyjson_alc alc = {0};
  yyjson_write_err err;

  yyjson_mut_doc *doc;
  yyjson_mut_val *root;

  yyjson_mut_val *network_key;
  yyjson_mut_val *network_object;

  yueah_string_t *cwd = {0};
  yueah_string_t *final_path = {0};
  yueah_string_t *filename = YUEAH_STR("config.json");
  char json_buf[KiB(10)] = {0};
  yueah_string_t *default_path = {0};
  bool write_res;

  default_path = DEFAULT_PATH;

  yueah_error_t error = yueah_success(NULL);
  cwd = get_cwd(pool, &error);
  if (!cwd || error.status != OK)
    return error;

  if (!path)
    final_path = yueah_string_append(pool, cwd, default_path);
  else
    final_path = path;

  if (!path_exist(pool, final_path)) {
    error = make_dir(pool, final_path);
    if (error.status != OK)
      return error;
  }

  final_path = yueah_string_append(pool, final_path, filename);
  if (config == NULL)
    return yueah_throw_error("config is NULL");

  yyjson_alc_pool_init(&alc, json_buf, KiB(10));

  doc = yyjson_mut_doc_new(NULL);
  root = yyjson_mut_obj(doc);

  network_key = yyjson_mut_str(doc, "network");
  network_object = yyjson_mut_obj(doc);

  if (config->db_path == NULL)
    yyjson_mut_obj_add_str(doc, root, "db_path", "./yueah.db");
  else {
    cstr *db_path_cstr = yueah_string_to_cstr(pool, config->db_path);
    yyjson_mut_obj_add_str(doc, root, "db_path", db_path_cstr);
  }

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
    return yueah_throw_error("Unknown log type");
  }

  if (config->network->ip == NULL)
    yyjson_mut_obj_add_str(doc, network_object, "ip", "127.0.0.1");
  else {
    cstr *ip_cstr = yueah_string_to_cstr(pool, config->network->ip);
    yyjson_mut_obj_add_str(doc, network_object, "ip", ip_cstr);
  }

  if (config->network->port == 0 || !config->network->port)
    yyjson_mut_obj_add_int(doc, network_object, "port", 8080);
  else
    yyjson_mut_obj_add_int(doc, network_object, "port", config->network->port);

  yyjson_mut_obj_add(root, network_key, network_object);

  yyjson_mut_doc_set_root(doc, root);

  cstr *final_path_cstr = yueah_string_to_cstr(pool, final_path);
  write_res = yyjson_mut_write_file(final_path_cstr, doc,
                                    YYJSON_WRITE_PRETTY_TWO_SPACES, &alc, &err);
  if (!write_res) {
    yyjson_mut_doc_free(doc);
    return yueah_throw_error("Error writing config file: %d:%s", err.code,
                             err.msg);
  }

  yyjson_mut_doc_free(doc);
  return yueah_success(NULL);
}

yueah_error_t read_config(h2o_mem_pool_t *pool, yueah_config_t **config,
                          yueah_string_nullable_t *path) {
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

  yueah_string_t *db_path;
  log_type_t log_type;

  yueah_string_t *ip;
  u16 port;

  yueah_config_t *local_config;

  yueah_string_t *default_path = DEFAULT_PATH;
  yueah_string_t *final_path = {0};
  yueah_string_t *cwd = {0};
  yueah_string_t *filename = YUEAH_STR("config.json");

  yueah_error_t error = yueah_success(NULL);
  cwd = get_cwd(pool, &error);
  if (!cwd || error.status != OK)
    return error;

  yyjson_alc_pool_init(&alc, json_buf, KiB(10));

  local_config = h2o_mem_alloc_pool(pool, yueah_config_t, 1);
  local_config->network = h2o_mem_alloc_pool(pool, network_config_t, 1);

  if (!path)
    final_path = yueah_string_append(pool, cwd, default_path);
  else
    final_path = path;

  if (!path_exist(pool, final_path))
    return yueah_throw_error("Failed to find config file");

  final_path = yueah_string_append(pool, final_path, filename);

  cstr *final_path_cstr = yueah_string_to_cstr(pool, final_path);
  doc = yyjson_read_file(final_path_cstr, YYJSON_READ_NOFLAG, &alc, &err);
  if (!doc)
    return yueah_throw_error("Error parsing json at %lu: %d:%s", err.pos,
                             err.code, err.msg);

  root = yyjson_doc_get_root(doc);

  db_path_val = yyjson_obj_get(root, "db_path");
  log_type_val = yyjson_obj_get(root, "log_type");

  network_object = yyjson_obj_get(root, "network");
  ip_val = yyjson_obj_get(network_object, "ip");
  port_val = yyjson_obj_get(network_object, "port");

  if (!root)
    return yueah_throw_error("Error parsing json, root is null");

  if (!db_path_val || !yyjson_is_str(db_path_val))
    return handle_parse_err("root", "db_path");
  if (!log_type_val || !yyjson_is_str(log_type_val))
    return handle_parse_err("root", "log_type");
  if (!network_object || !yyjson_is_obj(network_object))
    return handle_parse_err("root", "network");
  if (!ip_val || !yyjson_is_str(ip_val))
    return handle_parse_err("network", "ip");
  if (!port_val || !yyjson_is_uint(port_val))
    return handle_parse_err("root", "port");

  cstr db_path_cstr[1024];
  strlcpy(db_path_cstr, yyjson_get_str(db_path_val), 1024);
  db_path = yueah_string_new(pool, db_path_cstr, strlen(db_path_cstr));

  if (strncasecmp("both", yyjson_get_str(log_type_val), 4) == 0)
    log_type = Both;
  else if (strncasecmp("file", yyjson_get_str(log_type_val), 4) == 0)
    log_type = File;
  else
    log_type = Console;

  cstr ip_cstr[32];
  strlcpy(ip_cstr, yyjson_get_str(ip_val), 32);
  ip = yueah_string_new(pool, ip_cstr, strlen(ip_cstr));
  port = yyjson_get_uint(port_val);

  local_config->db_path = db_path;
  local_config->log_type = log_type;
  local_config->network->ip = ip;
  local_config->network->port = port;

  *config = local_config;
  return yueah_success(NULL);
}
