#ifndef YUEAH_TYPES_H
#define YUEAH_TYPES_H

#include <h2o.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef char cstr;
typedef cstr cstr_nullable;
typedef unsigned char ucstr;
typedef ucstr ucstr_nullable;

/*
 * @brief A string with a length, to not be limited by null terminator.
 *
 * @param data The string data.
 * @param len The length of the string.
 */
typedef struct {
  u8 *data;
  u64 len;
} yueah_string_t;

// Type alias for readability
typedef yueah_string_t yueah_string_nullable_t;

/*
 * @brief A string array.
 *
 * @param strings The strings in the array.
 * @param len The length of the array.
 */
typedef struct {
  yueah_string_t **strings;
  u64 len;
} yueah_string_array_t;

// Type alias for readability
typedef yueah_string_array_t yueah_string_array_nullable_t;

/*
 * @brief The status of an error, used to differentiate between success and
 * error.
 *
 * @param OK There was no error, meaning it was successful.
 * @param ERROR There was an error, meaning it was unsuccessful.
 */
typedef enum {
  OK,
  ERROR,
} yueah_error_status_t;

/*
 * @brief A struct for storing an error.
 *
 * @param status The status of the error.
 * @param message The message to print.
 * @param file The file the error occurred in.
 * @param function The function the error occurred in.
 * @param line The line the error occurred on.
 */
typedef struct {
  yueah_error_status_t status;
  const cstr *message;
  const cstr *file;
  const cstr *function;
  u64 line;
} yueah_error_t;

/*
 * @brief The configuration for CORS.
 *
 * @param allow_origin The origins that are allowed to make requests.
 * @param allow_methods The methods that are allowed to make requests.
 * @param allow_headers The headers that are allowed to make requests.
 * @param expose_headers The headers that are exposed to the client.
 *
 * @param allow_credentials Whether or not credentials are allowed.
 * @param max_age The maximum age of the credentials.
 */
typedef struct {
  const yueah_string_t *allow_origin;
  const yueah_string_t *allow_methods;
  const yueah_string_t *allow_headers;
  const yueah_string_t *expose_headers;
  bool allow_credentials : 1;
} yueah_cors_config_t;

/*
 * @brief The different configurations for CORS.
 *
 * @param public The configuration for public requests.
 * @param private The configuration for private requests.
 */
typedef struct {
  yueah_cors_config_t *public;
  yueah_cors_config_t *private;
} yueah_cors_configs_t;

/*
 * @brief The state for each request handler.
 *
 * @param db_path The path to the database.
 * @param cors The CORS configurations.
 */
typedef struct {
  yueah_string_t *db_path;
  yueah_cors_configs_t *cors;
} yueah_state_t;

/*
 * @brief The handler for each request.
 *
 * @param super The original h2o handler, so that all the original data is kept,
 * for easy casting.
 * @param state The state for the request handler.
 */
typedef struct {
  h2o_handler_t super;
  yueah_state_t *state;
} yueah_handler_t;

/*
 * @brief The different types of log targets.
 *
 * @param LOG_TO_FD The type that specifies to log to file descriptor.
 * @param LOG_TO_PATH The type that specifies to log to file path.
 */
typedef enum { LOG_TO_FD, LOG_TO_PATH } yueah_log_type_t;

/*
 * @brief The different types of log targets.
 *
 * @param type The type of the log target.
 * @param as The data of the log target (either FILE *fd, or cstr *path).
 */
typedef struct {
  yueah_log_type_t type;
  union {
    FILE *fd;
    cstr *path;
  } as;
} log_target_t;

/*
 * @brief Log levels.
 *
 * @param Error The log level for error messages.
 * @param Warning The log level for warning messages.
 * @param Info The log level for info messages.
 * @param Debug The log level for debug messages.
 */
typedef enum {
  Error,
  Warning,
  Info,
  Debug,
} log_level_t;

/*
 * @brief The state for the console logger
 *
 * @param fd The file descriptor to log to
 * @param path The path to log to
 */
typedef struct {
  FILE *fd;
  char *path;
} state_console_target_t;
/*
 * @brief JWT header.
 *
 * @param alg The algorithm used to sign the token.
 * @param typ The type of the token.
 */
typedef struct {
  yueah_string_t *alg;
  yueah_string_t *typ;
} yueah_jwt_header_t;

/*
 * @brief JWT claims.
 * @param iss The issuer of the token.
 * @param sub The subject of the token.
 * @param aud The audience of the token.
 * @param jti The token id.
 * @param iat The issued at time.
 * @param exp The expiration time.
 * @param nbf The not before time.
 */
typedef struct {
  const yueah_string_t *iss;
  const yueah_string_t *sub;
  const yueah_string_t *aud;
  const yueah_string_t *jti;
  u64 iat;
  u64 exp;
  u64 nbf;
} yueah_jwt_claims_t;

/*
 * @brief The different types of JWT keys.
 *
 * @param Access The type that specifies to use an access key.
 * @param Refresh The type that specifies to use a refresh key.
 */
typedef enum {
  Access,
  Refresh,
} yueah_jwt_key_type_t;

/*
 * @brief Database opening flags.
 *
 * @param READ The flag that specifies to open the database for reading.
 * @param WRITE The flag that specifies to open the database for writing.
 */
typedef enum {
  READ = 1 << 0,
  WRITE = 1 << 1,
} yueah_db_flags_t;

/*
 * @brief Cookie bitmasks.
 *
 * @param SAME_SITE The bitmask that specifies to set the SameSite attribute.
 * @param HTTP_ONLY The bitmask that specifies to set the HttpOnly attribute.
 * @param SECURE The bitmask that specifies to set the Secure attribute.
 * @param PATH The bitmask that specifies to set the Path attribute.
 * @param EXPIRES The bitmask that specifies to set the Expires attribute.
 * @param MAX_AGE The bitmask that specifies to set the Max-Age attribute.
 * @param DOMAIN The bitmask that specifies to set the Domain attribute.
 */
typedef enum {
  SAME_SITE = 1 << 0,
  HTTP_ONLY = 1 << 1,
  SECURE = 1 << 2,
  PATH = 1 << 3,
  EXPIRES = 1 << 4,
  MAX_AGE = 1 << 5,
  DOMAIN = 1 << 6,
} yueah_cookie_bitmask_t;

/*
 * @brief SameSite attribute values.
 *
 * @param INVALID The value that specifies an invalid SameSite attribute.
 * @param LAX The value that specifies the SameSite attribute to be lax.
 * @param STRICT The value that specifies the SameSite attribute to be strict.
 * @param NONE The value that specifies the SameSite attribute to be none.
 */
typedef enum { INVALID = -1, LAX, STRICT, NONE } yueah_same_site_t;

// Type alias for readability
typedef int yueah_cookie_mask;

/*
 * @brief The different types of access log targets.
 *
 * @param File The type that specifies to log to file.
 * @param Console The type that specifies to log to console.
 * @param Both The type that specifies to log to both file and console.
 *
 * @note TODO: make this a bitmask
 */
typedef enum { File, Console, Both } log_type_t;

/*
 * @brief The network section of the yueah config.
 *
 * @param ip The IP address to bind to.
 * @param port The port to bind to.
 */
typedef struct {
  yueah_string_t *ip;
  u16 port;
} network_config_t;

/*
 * @brief The configuration for yueah.
 *
 * @param db_path The path to the database.
 * @param log_type The type of log to use.
 * @param network The network configuration.
 */
typedef struct {
  yueah_string_t *db_path;
  log_type_t log_type;
  network_config_t *network;
} yueah_config_t;

/*
 * @brief A generic struct for generic HTTP error responses.
 *
 * @param message The message to send with the response.
 * @param status The status code of the response.
 */
typedef struct {
  char *message;
  int status;
} yueah_error_response_t;

#endif // !YUEAH_TYPES_H
