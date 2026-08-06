/*
 * Minimal <sys/mman.h> stub for building Zend OPcache on ESP-IDF (picolibc has no sys/mman.h).
 *
 * OPcache references mmap/munmap/mprotect only in its shared-memory backends, which are compiled
 * out here (no USE_MMAP/USE_SHM*) and never reached in file_cache_only mode. These declarations
 * exist only so the sources compile; the symbols are weak no-ops (see opcache_posix_stubs.c).
 * Scoped to the opcache sources via the php component's include path -- no other file includes it.
 */
#ifndef PHP_ESP32_OPCACHE_STUB_SYS_MMAN_H
#define PHP_ESP32_OPCACHE_STUB_SYS_MMAN_H

#include <sys/types.h>

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void *) -1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);
int   mprotect(void *addr, size_t length, int prot);

#endif
