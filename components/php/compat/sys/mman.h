/*
 * newlib here has no <sys/mman.h>, but a few PHP files include it
 * unconditionally (e.g. zend_alloc.c under #ifndef _WIN32). With HAVE_MMAP off
 * the actual mmap calls are compiled out, so this header just lets those
 * includes parse and provides courtesy constants. mmap() is a stub that fails.
 */
#ifndef COMPAT_SYS_MMAN_H
#define COMPAT_SYS_MMAN_H

#include <sys/types.h>
#include <stddef.h>

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void *)-1)

#ifdef __cplusplus
extern "C" {
#endif

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);
int   mprotect(void *addr, size_t length, int prot);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_SYS_MMAN_H */
