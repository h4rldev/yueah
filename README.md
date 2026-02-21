# yueah

A small blogging backend written in C.

## Building

### Dependencies

- [Just](https://github.com/casey/just) for building
- [CMake](https://cmake.org) for building libH2O
- [SQLite3](https://www.sqlite.org) for database functionality
- [libh2o](https://h2o.examp1e.net) for HTTP server functionality
- [yyjson](https://github.com/ibireme/yyjson) for JSON parsing

### yueah's migrator's Dependencies

- [SQLite3](sqlite.org) for database functionality

### yueah's dotenv's Dependencies

- [glibc](https://www.gnu.org/software/libc/)

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

### Building yueah's dotenv

(Done automatically when missing)

```
just ensure_dotenv
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

### Building yueah's migrator

#### Release

```bash
just build-migrator release
```

#### Debug

```bash
just build-migratore debug
```

### Running yueah's migrator

```bash
./bin/migrator -h
```

### Running

```bash
./bin/yueah or ./bin/yueah -h for help
```

## License

This project is licensed under the [BSD 3-Clause](./LICENSE) license.
