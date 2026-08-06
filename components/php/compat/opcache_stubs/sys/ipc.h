/*
 * Minimal <sys/ipc.h> stub for building Zend OPcache on ESP-IDF (picolibc has no System V IPC).
 * Only used by OPcache's SysV-SHM backend, which is compiled out here and never reached in
 * file_cache_only mode. See opcache_stubs/sys/mman.h for the rationale.
 */
#ifndef PHP_ESP32_OPCACHE_STUB_SYS_IPC_H
#define PHP_ESP32_OPCACHE_STUB_SYS_IPC_H

#include <sys/types.h>   /* key_t */

#define IPC_PRIVATE ((key_t) 0)
#define IPC_CREAT   01000
#define IPC_EXCL    02000
#define IPC_RMID    0
#define IPC_STAT    2

struct ipc_perm {
    key_t         __key;
    unsigned int  uid, gid, cuid, cgid, mode;
};

key_t ftok(const char *pathname, int proj_id);

#endif
