# yueah

A small blogging backend written in C.

Utilizing:

- [sqlite3](https://www.sqlite.org) for database functionality
- [libh2o](https://h2o.examp1e.net) for HTTP server functionality
- [Jansson](https://github.com/akheron/jansson) for JSON parsing

## Building

### Dependencies

- [CMake](https://cmake.org) for building libH2O
- [libh2o](https://h2o.examp1e.net) for HTTP server functionality
- [jansson](https://github.com/akheron/jansson) for JSON parsing

### libH2O's dependencies

- [libuv](https://github.com/libuv/libuv) for libh2o's event loop
- [zlib](https://zlib.net) for libh2o's compression
- [brotli](https://github.com/google/brotli) for libh2o's compression (despite not working)
- [OpenSSL](https://www.openssl.org) for libh2o's SSL
- [picotls](https://github.com/h2o/picotls) for libh2o's SSL
- [libcap](https://git.kernel.org/pub/scm/libs/libcap/libcap.git/) for libh2o's privileges
- [wslay](https://github.com/tatsuhiro-t/wslay) for libh2o's websockets (probably not required for building)

### Building libH2O

(Done automatically when missing)

```bash
just ensure_h2o
```

### Building yueah

#### Release

```bash
just release
```

#### Debug

```bash
just debug
```

### Running

```bash
./bin/yueah or ./bin/yueah -h for help
```

## License

This project is licensed under the [BSD 3-Clause](./LICENSE) license.
