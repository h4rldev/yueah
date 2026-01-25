#ifndef YUEAH_DB_H
#define YUEAH_DB_H

#include <sqlite3.h>

#include <config.h>

int init_db(yueah_config_t *config, sqlite3 **db);
void close_db(sqlite3 *db);

#endif // !YUEAH_DB_H
