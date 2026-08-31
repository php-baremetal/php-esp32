/*
 * Load-extension stubs for the SQLite build.
 *
 * The amalgamation is compiled with SQLITE_OMIT_LOAD_EXTENSION (loading a shared object makes no
 * sense on bare metal), which drops sqlite3_load_extension() / sqlite3_enable_load_extension() from
 * it. But both PHP API layers reference them: pdo_sqlite exposes Pdo\Sqlite::loadExtension() and
 * ext/sqlite3 exposes SQLite3::loadExtension(), each calling straight into the C API. Without these
 * the link fails (undefined reference to sqlite3_load_extension).
 *
 * So provide honest no-op stubs: enabling load-extension "succeeds" (it does nothing), and any actual
 * load fails cleanly at runtime. Declared locally rather than via sqlite3.h so it doesn't matter that
 * the header hides these under SQLITE_OMIT_LOAD_EXTENSION; the linker matches by name (C linkage).
 */

#define SQLITE_OK    0
#define SQLITE_ERROR 1

int sqlite3_enable_load_extension(void *db, int onoff)
{
    (void) db;
    (void) onoff;
    return SQLITE_OK;   /* no-op: there is nothing to enable */
}

int sqlite3_load_extension(void *db, const char *zFile, const char *zProc, char **pzErrMsg)
{
    (void) db;
    (void) zFile;
    (void) zProc;
    if (pzErrMsg) {
        *pzErrMsg = 0;
    }
    return SQLITE_ERROR;   /* loading a shared object isn't possible on this target */
}
