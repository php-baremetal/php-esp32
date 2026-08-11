/*
 * Fill-ins for POSIX symbols PHP references but newlib doesn't provide here.
 * They cover things that simply don't exist on this hardware: processes/exec,
 * users/groups, symlinks/chown, socketpairs, mmap, and fibers.
 *
 * Everything is weak, so if a future newlib grows a real implementation it wins.
 * The stubs fail cleanly (error return) instead of leaving the link broken.
 *
 * Compiled with -w -fpermissive (inherited from the component), so small
 * prototype mismatches against the system headers aren't fatal.
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#define WEAK __attribute__((weak))
#define FAIL_ENOSYS() do { errno = ENOSYS; return -1; } while (0)

/* --- processes / exec: no fork or exec --- */
WEAK int    pipe(int p[2])                 { (void)p; FAIL_ENOSYS(); }
WEAK int    socketpair(int a,int b,int c,int s[2]) { (void)a;(void)b;(void)c;(void)s; FAIL_ENOSYS(); }
WEAK int    waitpid(int pid,int *st,int o) { (void)pid;(void)st;(void)o; FAIL_ENOSYS(); }
WEAK int    nice(int inc)                  { (void)inc; FAIL_ENOSYS(); }
WEAK FILE  *popen(const char *c,const char *m) { (void)c;(void)m; errno = ENOSYS; return NULL; }
WEAK int    pclose(FILE *f)                { (void)f; FAIL_ENOSYS(); }
WEAK int    getdtablesize(void)            { return 0; }

/* nanosleep maps onto usleep (which yields the core under ESP-IDF) */
WEAK int nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    if (req) {
        usleep((useconds_t)(req->tv_sec * 1000000UL + req->tv_nsec / 1000));
    }
    return 0;
}

/* no signals: register nothing, report "default" */
WEAK void (*signal(int signum, void (*handler)(int)))(int)
{
    (void)signum;
    return handler;
}

/* posix_spawn family */
WEAK int posix_spawn(int *pid, const char *path, const void *fa, const void *attr, char *const av[], char *const ev[]) { (void)pid;(void)path;(void)fa;(void)attr;(void)av;(void)ev; return ENOSYS; }
WEAK int posix_spawnp(int *pid, const char *file, const void *fa, const void *attr, char *const av[], char *const ev[]) { (void)pid;(void)file;(void)fa;(void)attr;(void)av;(void)ev; return ENOSYS; }
WEAK int posix_spawn_file_actions_init(void *fa)                          { (void)fa; return 0; }
WEAK int posix_spawn_file_actions_destroy(void *fa)                       { (void)fa; return 0; }
WEAK int posix_spawn_file_actions_addclose(void *fa,int fd)              { (void)fa;(void)fd; return 0; }
WEAK int posix_spawn_file_actions_adddup2(void *fa,int fd,int nfd)       { (void)fa;(void)fd;(void)nfd; return 0; }
WEAK int posix_spawn_file_actions_addchdir_np(void *fa,const char *p)    { (void)fa;(void)p; return 0; }

/* --- users / groups: no such concept here --- */
WEAK uid_t getuid(void)  { return 0; }
WEAK uid_t geteuid(void) { return 0; }
WEAK gid_t getgid(void)  { return 0; }
WEAK pid_t getppid(void) { return 1; }   /* PHP 8.4 ext/random fallback-seed mixes it in; picolibc lacks it */
WEAK gid_t getegid(void) { return 0; }
WEAK int   getgroups(int n, gid_t *g) { (void)n;(void)g; return 0; }
WEAK void *getpwuid(uid_t uid)    { (void)uid; return NULL; }
/* PHP 8.5's bundled glob (main/php_glob.c) does ~user expansion with getpw*_r; picolibc lacks
 * them. Report "no such user" (return 0, *result = NULL) so tilde expansion is simply a no-op. */
WEAK int getpwuid_r(uid_t uid, void *pwd, char *buf, size_t buflen, void **result) { (void)uid;(void)pwd;(void)buf;(void)buflen; if (result) *result = NULL; return 0; }
WEAK int getpwnam_r(const char *name, void *pwd, char *buf, size_t buflen, void **result) { (void)name;(void)pwd;(void)buf;(void)buflen; if (result) *result = NULL; return 0; }
WEAK void *getpwnam(const char *n){ (void)n;   return NULL; }
WEAK void *getgrnam(const char *n){ (void)n;   return NULL; }

/* --- POSIX filesystem bits FATFS/VFS don't cover --- */
WEAK int umask(int m)        { (void)m; return 0; }
WEAK int dup(int fd)         { (void)fd; FAIL_ENOSYS(); }
WEAK int chown(const char *p,uid_t u,gid_t g)  { (void)p;(void)u;(void)g; FAIL_ENOSYS(); }
WEAK int lchown(const char *p,uid_t u,gid_t g) { (void)p;(void)u;(void)g; FAIL_ENOSYS(); }
/* FAT has no permissions/ownership/timestamps to change. SQLite's unix VFS calls
 * these best-effort, so report success (0) -- an error return would be spurious. */
WEAK int fchmod(int fd,mode_t m)               { (void)fd;(void)m; return 0; }
WEAK int fchown(int fd,uid_t u,gid_t g)        { (void)fd;(void)u;(void)g; return 0; }
WEAK int utimes(const char *p,const void *tv)  { (void)p;(void)tv; return 0; }
WEAK int symlink(const char *a,const char *b)  { (void)a;(void)b; FAIL_ENOSYS(); }
WEAK ssize_t readlink(const char *p,char *b,size_t s) { (void)p;(void)b;(void)s; FAIL_ENOSYS(); }
WEAK int lstat(const char *p, void *st) { (void)p;(void)st; FAIL_ENOSYS(); }
WEAK int flock(int fd,int op)     { (void)fd;(void)op; return 0; }  /* no locking: pretend it worked */
WEAK int fdatasync(int fd)        { (void)fd; return 0; }
WEAK int chroot(const char *p)    { (void)p; FAIL_ENOSYS(); }
WEAK long copy_file_range(int in,long *io,int out,long *oo,unsigned long n,unsigned f) { (void)in;(void)io;(void)out;(void)oo;(void)n;(void)f; FAIL_ENOSYS(); }
WEAK int getloadavg(double a[],int n) { (void)a;(void)n; return -1; }
WEAK int gethostname(char *n,size_t l) { if(n&&l){ n[0]='\0'; } return 0; }

/* no filesystem globbing */
WEAK int  glob(const char *p,int f,void *cb,void *g) { (void)p;(void)f;(void)cb;(void)g; return 1; }
WEAK void globfree(void *g) { (void)g; }

/* no resolver */
WEAK int getnameinfo(const void *sa,unsigned sl,char *h,unsigned hl,char *s,unsigned svl,int fl) { (void)sa;(void)sl;(void)h;(void)hl;(void)s;(void)svl;(void)fl; return -1; }
WEAK const char *gai_strerror(int e) { (void)e; return "getaddrinfo error"; }

/* --- memory: no mmap; the allocator uses malloc instead --- */
WEAK void *mmap(void *a,size_t l,int p,int f,int fd,long off) { (void)a;(void)l;(void)p;(void)f;(void)fd;(void)off; errno = ENOSYS; return (void*)-1; }
WEAK int   munmap(void *a,size_t l)          { (void)a;(void)l; return 0; }
WEAK int   mprotect(void *a,size_t l,int p)  { (void)a;(void)l;(void)p; return 0; }

/* --- fibers ---
 * The boost.context assembly only ships for 64-bit RISC-V; this is a 32-bit
 * part, so there's nothing to link. These do nothing: PHP's Fiber class will
 * crash if used, which it isn't. Needs a real rv32 context switch to support. */
WEAK void *make_fcontext(void *sp,size_t size,void (*fn)(void)) { (void)sp;(void)size;(void)fn; return NULL; }
WEAK void  jump_fcontext(void) { }
