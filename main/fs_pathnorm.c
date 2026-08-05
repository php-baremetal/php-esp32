/*
 * Path normalization shim for the FATFS VFS.
 *
 * ESP-IDF's FATFS VFS does NOT resolve "." and ".." path components: a path like
 * ".../Bootstrap/../../../../config" is passed to FatFs verbatim, and f_stat/f_opendir on it fail.
 * A lot of PHP/framework code builds paths that way (Laravel's LoadConfiguration uses
 * __DIR__."/../../../../config" for the framework's default config), so is_dir()/opendir() return
 * false and the app breaks -- even though the target directory exists.
 *
 * PHP's own virtual-cwd layer (VIRTUAL_DIR) would normalize paths, but it depends on a working
 * getcwd(), which this target doesn't have -- enabling it breaks stat() entirely. So instead we
 * collapse "." / ".." / "//" lexically here, in linker --wrap shims around the file syscalls, before
 * the VFS/FatFs ever sees the path. Clean paths (no "." or "..") pass straight through untouched.
 */
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#define PN_MAX 512

/* Collapse ".", ".." and "//" in a path, lexically (no filesystem access). Only "." and ".." as
 * whole segments are special -- a dotfile like ".env" is an ordinary segment. */
static void pn_normalize(const char *in, char *out, size_t outsz)
{
    const char *segs[80];
    int seglen[80];
    int n = 0;
    int absolute = (in[0] == '/');

    const char *p = in;
    while (*p) {
        while (*p == '/') {
            p++;
        }
        if (!*p) {
            break;
        }
        const char *start = p;
        while (*p && *p != '/') {
            p++;
        }
        int len = (int) (p - start);
        if (len == 1 && start[0] == '.') {
            /* "." -> drop */
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (n > 0) {
                n--;                 /* ".." -> pop the previous segment */
            }
        } else if (n < 80) {
            segs[n] = start;
            seglen[n] = len;
            n++;
        }
    }

    char *o = out;
    size_t rem = outsz;
    if (absolute && rem > 1) {
        *o++ = '/';
        rem--;
    }
    for (int i = 0; i < n && rem > 1; i++) {
        if (i > 0 && rem > 1) {
            *o++ = '/';
            rem--;
        }
        int len = seglen[i];
        if ((size_t) len >= rem) {
            len = (int) rem - 1;
        }
        memcpy(o, segs[i], len);
        o += len;
        rem -= len;
    }
    if (o == out && outsz > 1) {     /* empty result -> "/" (absolute) or "." (relative) */
        *o++ = absolute ? '/' : '.';
    }
    *o = '\0';
}

/* Fast path: only normalize when the path actually contains a "." or ".." segment or a "//". */
static const char *pn(const char *path, char *buf, size_t sz)
{
    if (!path || !path[0]) {
        return path;
    }
    if (!strstr(path, "/.") && !strstr(path, "//") &&
        !(path[0] == '.' && (path[1] == '/' || (path[1] == '.' && path[2] == '/')))) {
        return path;                 /* no "." / ".." / "//" -> untouched */
    }
    pn_normalize(path, buf, sz);
    return buf;
}

/* --- linker --wrap shims: normalize the path, then call the real syscall ------------------ */

extern int __real_stat(const char *path, struct stat *st);
int __wrap_stat(const char *path, struct stat *st)
{
    char b[PN_MAX];
    return __real_stat(pn(path, b, sizeof b), st);
}

/* The FATFS VFS's lstat is unimplemented/unreliable (it fails, breaking PHP's realpath, which
 * lstat()s every path component -- so realpath() returned false for everything). FatFs has no
 * symlinks, so lstat is equivalent to stat: route it there. */
int __wrap_lstat(const char *path, struct stat *st)
{
    char b[PN_MAX];
    return __real_stat(pn(path, b, sizeof b), st);
}

extern int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    char b[PN_MAX];
    return __real_open(pn(path, b, sizeof b), flags, mode);
}

extern DIR *__real_opendir(const char *path);
DIR *__wrap_opendir(const char *path)
{
    char b[PN_MAX];
    return __real_opendir(pn(path, b, sizeof b));
}

extern int __real_access(const char *path, int mode);
int __wrap_access(const char *path, int mode)
{
    char b[PN_MAX];
    return __real_access(pn(path, b, sizeof b), mode);
}

extern int __real_mkdir(const char *path, mode_t mode);
int __wrap_mkdir(const char *path, mode_t mode)
{
    char b[PN_MAX];
    return __real_mkdir(pn(path, b, sizeof b), mode);
}

extern int __real_unlink(const char *path);
int __wrap_unlink(const char *path)
{
    char b[PN_MAX];
    return __real_unlink(pn(path, b, sizeof b));
}

extern int __real_rmdir(const char *path);
int __wrap_rmdir(const char *path)
{
    char b[PN_MAX];
    return __real_rmdir(pn(path, b, sizeof b));
}

extern int __real_rename(const char *from, const char *to);
int __wrap_rename(const char *from, const char *to)
{
    char bf[PN_MAX], bt[PN_MAX];
    return __real_rename(pn(from, bf, sizeof bf), pn(to, bt, sizeof bt));
}
