#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <h2o.h>
#include <h2o/file.h>
#include <h2o/http1.h>
#include <h2o/http2.h>
#include <h2o/memcached.h>

#include <cli.h>
#include <config.h>
#include <file.h>
#include <mem.h>
#include <meta.h>

static h2o_pathconf_t *
register_handler(h2o_hostconf_t *hostconf, const char *path,
                 int (*on_req)(h2o_handler_t *, h2o_req_t *)) {
  h2o_pathconf_t *pathconf = h2o_config_register_path(hostconf, path, 0);
  h2o_handler_t *handler = h2o_create_handler(pathconf, sizeof(*handler));
  handler->on_req = on_req;
  return pathconf;
}

static h2o_globalconf_t config;
static h2o_context_t ctx;
static h2o_multithread_receiver_t libmemcached_receiver;
static h2o_accept_ctx_t accept_ctx;

mem_arena *arena;
static void on_accept(uv_stream_t *listener, int status) {
  uv_tcp_t *conn;
  h2o_socket_t *sock;

  if (status != 0)
    return;

  conn = h2o_mem_alloc(sizeof(*conn));
  uv_tcp_init(listener->loop, conn);

  if (uv_accept(listener, (uv_stream_t *)conn) != 0) {
    uv_close((uv_handle_t *)conn, (uv_close_cb)free);
    return;
  }

  sock = h2o_uv_socket_create((uv_handle_t *)conn, (uv_close_cb)free);
  h2o_accept(&accept_ctx, sock);
}

static int create_listener(char *ip, unsigned int port) {
  static uv_tcp_t listener;
  struct sockaddr_in addr;
  int r;

  uv_tcp_init(ctx.loop, &listener);
  uv_ip4_addr(ip, port, &addr);
  if ((r = uv_tcp_bind(&listener, (struct sockaddr *)&addr, 0)) != 0) {
    fprintf(stderr, "uv_tcp_bind:%s\n", uv_strerror(r));
    goto Error;
  }
  if ((r = uv_listen((uv_stream_t *)&listener, 128, on_accept)) != 0) {
    fprintf(stderr, "uv_listen:%s\n", uv_strerror(r));
    goto Error;
  }

  return 0;
Error:
  uv_close((uv_handle_t *)&listener, NULL);
  return r;
}

static int setup_ssl(const char *cert_file, const char *key_file,
                     const char *ciphers, char *ip, bool use_memcached) {
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();

  accept_ctx.ssl_ctx = SSL_CTX_new(SSLv23_server_method());
  SSL_CTX_set_options(accept_ctx.ssl_ctx, SSL_OP_NO_SSLv2);

  if (use_memcached == true) {
    accept_ctx.libmemcached_receiver = &libmemcached_receiver;
    h2o_accept_setup_memcached_ssl_resumption(
        h2o_memcached_create_context(ip, 11211, 0, 1, "h2o:ssl-resumption:"),
        86400);
    h2o_socket_ssl_async_resumption_setup_ctx(accept_ctx.ssl_ctx);
  }

#ifdef SSL_CTX_set_ecdh_auto
  SSL_CTX_set_ecdh_auto(accept_ctx.ssl_ctx, 1);
#endif

  /* load certificate and private key */
  if (SSL_CTX_use_certificate_chain_file(accept_ctx.ssl_ctx, cert_file) != 1) {
    fprintf(
        stderr,
        "an error occurred while trying to load server certificate file:%s\n",
        cert_file);
    return -1;
  }
  if (SSL_CTX_use_PrivateKey_file(accept_ctx.ssl_ctx, key_file,
                                  SSL_FILETYPE_PEM) != 1) {
    fprintf(stderr,
            "an error occurred while trying to load private key file:%s\n",
            key_file);
    return -1;
  }

  if (SSL_CTX_set_cipher_list(accept_ctx.ssl_ctx, ciphers) != 1) {
    fprintf(stderr, "ciphers could not be set: %s\n", ciphers);
    return -1;
  }

/* setup protocol negotiation methods */
#if H2O_USE_NPN
  h2o_ssl_register_npn_protocols(accept_ctx.ssl_ctx, h2o_http2_npn_protocols);
#endif
#if H2O_USE_ALPN
  h2o_ssl_register_alpn_protocols(accept_ctx.ssl_ctx, h2o_http2_alpn_protocols);
#endif

  return 0;
}

static int get_year(void) {
  time_t time_container = time(NULL);
  struct tm *time = localtime(&time_container);
  return time->tm_year + 1900;
}

static int not_found(h2o_handler_t *handler, h2o_req_t *req) {
  h2o_generator_t generator = {NULL, NULL};

  yueah_config_t *yueah_config;
  read_config(arena, &yueah_config);
  char html_buffer[1024] = {0};

  sprintf(html_buffer,
          "<title>Error 404</title>"
          "<style>"
          "body{font-family: sans-serif;}"
          "h1{font-size: 1.5em;}"
          "p{font-size: 1em; margin-bottom: 0.5em;}"
          "sub{font-size: 0.7em;}"
          "</style>"
          "<h1>Error 404</h1>"
          "<p>Not found</p>"
          "<sub><a href=\"https://github.com/h4rldev/portfolio\" "
          "target=\"_blank\">h4rl's portfolio :3</a>, %d</sub>",
          get_year());

  h2o_iovec_t body = h2o_strdup(&req->pool, html_buffer, strlen(html_buffer));

  req->res.status = 404;
  req->res.reason = "Not Found";
  h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL,
                 H2O_STRLIT("text/html; charset=utf-8"));
  h2o_start_response(req, &generator);
  h2o_send(req, &body, 1, 1);

  return 0;
}

int main(int argc, char **argv) {
  arena = arena_init(MiB(32), MiB(16));

  yueah_config_t *yueah_config;
  time_t time_container = time(NULL);
  struct tm *time = localtime(&time_container);
  char log_fname[28] = {0};

  uv_loop_t loop;
  h2o_access_log_filehandle_t *log2file = NULL;
  h2o_access_log_filehandle_t *logfh = NULL;
  h2o_hostconf_t *hostconf = NULL;
  h2o_pathconf_t *pathconf = NULL;

  snprintf(log_fname, 28, "./logs/yueah-%d-%02d-%02d.log", time->tm_year + 1900,
           time->tm_mon + 1, time->tm_mday);

  if (read_config(arena, &yueah_config) != 0) {
    init_config(arena, &yueah_config);
    write_config(yueah_config);
  }

  switch (yueah_config->log_type) {
  case Both:
    if (path_exist("./logs/") == false)
      make_dir("./logs/");
    log2file =
        h2o_access_log_open_handle(log_fname, NULL, H2O_LOGCONF_ESCAPE_APACHE);
    logfh = h2o_access_log_open_handle("/dev/stdout", NULL,
                                       H2O_LOGCONF_ESCAPE_APACHE);
    break;
  case File:
    if (path_exist("./logs/") == false)
      make_dir("./logs/");
    log2file =
        h2o_access_log_open_handle(log_fname, NULL, H2O_LOGCONF_ESCAPE_APACHE);
    break;

  case Console:
    logfh = h2o_access_log_open_handle("/dev/stdout", NULL,
                                       H2O_LOGCONF_ESCAPE_APACHE);
    break;
  }

  signal(SIGPIPE, SIG_IGN);

  if (parse_args(argc, argv, &yueah_config) != 0) {
    fprintf(stderr, "yueah: failed parsing args, something is very wrong..\n");
    return -1;
  }

  h2o_config_init(&config);
  h2o_compress_register_configurator(&config);

  hostconf = h2o_config_register_host(
      &config, h2o_iovec_init(H2O_STRLIT("default")), 65535);

  pathconf = register_handler(hostconf, "/", not_found);
  if (logfh)
    h2o_access_log_register(pathconf, logfh);
  if (log2file)
    h2o_access_log_register(pathconf, log2file);

  if (!yueah_config->compression->enabled)
    goto NotCompress;

  h2o_compress_args_t ca = {
      .gzip.quality = yueah_config->compression->quality,
  };

  if (yueah_config->compression->min_size != 0)
    ca.min_size = yueah_config->compression->min_size;

  h2o_compress_register(pathconf, &ca);
  if (logfh != NULL)
    h2o_access_log_register(pathconf, logfh);

  if (log2file != NULL)
    h2o_access_log_register(pathconf, log2file);

NotCompress:
  uv_loop_init(&loop);
  h2o_context_init(&ctx, &loop, &config);

  if (yueah_config->ssl->mem_cached)
    h2o_multithread_register_receiver(ctx.queue, &libmemcached_receiver,
                                      h2o_memcached_receiver);

  if (yueah_config->ssl->enabled &&
      setup_ssl(yueah_config->ssl->cert_path, yueah_config->ssl->key_path,
                "DEFAULT:!MD5:!DSS:!DES:!RC4:!RC2:!SEED:!IDEA:!"
                "NULL:!ADH:!EXP:!SRP:!PSK",
                yueah_config->network->ip, yueah_config->ssl->mem_cached) != 0)
    goto Error;

  accept_ctx.ctx = &ctx;
  accept_ctx.hosts = config.hosts;
  config.server_name = h2o_iovec_init(H2O_STRLIT("toast"));

  if (create_listener(yueah_config->network->ip, yueah_config->network->port) !=
      0) {
    fprintf(stderr, "failed to listen to %s:%u:%s\n", yueah_config->network->ip,
            yueah_config->network->port, strerror(errno));
    goto Error;
  }

  printf("yueah: running on %s:%u\n", yueah_config->network->ip,
         yueah_config->network->port);
  uv_run(ctx.loop, UV_RUN_DEFAULT);

  arena_destroy(arena);
  return 0;

Error:
  return -1;
}
