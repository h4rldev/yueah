#ifndef YUEAH_DB_H
#define YUEAH_DB_H

#include <h2o.h>
#include <sqlite3.h>

#include <yueah/types.h>

/*
 * @brief Connect to a database
 *
 * @param db_path The path to the database
 * @param db The database pointer
 * @param flags The flags to use when connecting to the database
 *
 * @return 0 on success
 */
int yueah_db_connect(h2o_mem_pool_t *pool, const yueah_string_t *db_path,
                     sqlite3 **db, int flags);

/*
 * @brief Disconnect from a database
 *
 * @param db The database pointer
 *
 * @return 0 on success
 */
int yueah_db_disconnect(sqlite3 *db);

#endif // !YUEAH_DB_H
