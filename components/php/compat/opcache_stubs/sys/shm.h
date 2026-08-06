/*
 * Minimal <sys/shm.h> stub for building Zend OPcache on ESP-IDF (picolibc has no System V SHM).
 * Only used by OPcache's SysV-SHM backend, which is compiled out here and never reached in
 * file_cache_only mode. See opcache_stubs/sys/mman.h for the rationale.
 */
#ifndef PHP_ESP32_OPCACHE_STUB_SYS_SHM_H
#define PHP_ESP32_OPCACHE_STUB_SYS_SHM_H

#include <sys/ipc.h>
#include <sys/types.h>

#define SHM_R    0400
#define SHM_W    0200
#define SHM_RDONLY 010000

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t          shm_segsz;
};

int   shmget(key_t key, size_t size, int shmflg);
void *shmat(int shmid, const void *shmaddr, int shmflg);
int   shmdt(const void *shmaddr);
int   shmctl(int shmid, int cmd, struct shmid_ds *buf);

#endif
