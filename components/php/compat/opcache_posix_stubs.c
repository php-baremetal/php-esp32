/*
 * Weak POSIX stubs for the parts of Zend OPcache that assume a full Unix (shared memory, process
 * restart via fork/wait/kill, permission checks via getpwnam). None of this runs in file_cache_only
 * mode -- the code paths are compiled but never executed on this target -- so these definitions only
 * exist to satisfy the linker. They are weak, so if picolibc/ESP-IDF provides a real symbol it wins.
 *
 * Paired with the stub headers in opcache_stubs/ (sys/ipc.h, sys/shm.h, sys/mman.h), which picolibc
 * doesn't ship. Compiled only when PHP_EXT_OPCACHE is on.
 */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>   /* stub */
#include <sys/ipc.h>    /* stub */
#include <sys/shm.h>    /* stub */

#define WEAK __attribute__((weak))

/* --- privilege drop in the preload subprocess (preload is unused here, never called) --- */
WEAK int setuid(uid_t uid) { (void) uid; return 0; }
WEAK int setgid(gid_t gid) { (void) gid; return 0; }
WEAK int initgroups(const char *user, gid_t group) { (void) user; (void) group; return 0; }

/* writev(): actually used by the file-cache writer, but picolibc doesn't ship it. Implement it as
 * a sequence of write() calls (this target has no atomicity guarantees to preserve anyway).
 * struct iovec is standard ABI ({void*, size_t}) and picolibc doesn't expose it here without PHP's
 * feature macros, so declare it locally -- layout-compatible with the caller's. */
struct iovec {
    void  *iov_base;
    size_t iov_len;
};

WEAK ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        const char *p = (const char *) iov[i].iov_base;
        size_t remaining = iov[i].iov_len;
        while (remaining > 0) {
            ssize_t n = write(fd, p, remaining);
            if (n <= 0) {
                return total ? total : -1;   /* real error */
            }
            p += n;
            remaining -= (size_t) n;
            total += n;
        }
    }
    return total;   /* always the full requested length unless a write() errored */
}

/* --- memory mapping (OPcache SHM backends -- compiled out, never called) --- */
WEAK void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    (void) addr; (void) length; (void) prot; (void) flags; (void) fd; (void) offset;
    errno = ENOSYS;
    return MAP_FAILED;
}
WEAK int munmap(void *addr, size_t length) { (void) addr; (void) length; errno = ENOSYS; return -1; }
WEAK int mprotect(void *addr, size_t length, int prot)
{
    (void) addr; (void) length; (void) prot;
    return 0;   /* pretend success: memory protection is a no-op here */
}

/* --- System V shared memory (SysV-SHM backend -- compiled out, never called) --- */
WEAK key_t ftok(const char *pathname, int proj_id) { (void) pathname; (void) proj_id; return (key_t) -1; }
WEAK int shmget(key_t key, size_t size, int shmflg) { (void) key; (void) size; (void) shmflg; errno = ENOSYS; return -1; }
WEAK void *shmat(int shmid, const void *shmaddr, int shmflg) { (void) shmid; (void) shmaddr; (void) shmflg; errno = ENOSYS; return (void *) -1; }
WEAK int shmdt(const void *shmaddr) { (void) shmaddr; errno = ENOSYS; return -1; }
WEAK int shmctl(int shmid, int cmd, struct shmid_ds *buf) { (void) shmid; (void) cmd; (void) buf; errno = ENOSYS; return -1; }
