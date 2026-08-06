/*
 * A readdir()-based glob() for the FATFS VFS.
 *
 * PHP is built with HAVE_GLOB, so glob() maps to the C library's glob(). picolibc ships one, but it
 * fails on the ESP-IDF FATFS VFS (returns an error), so glob() returns false in PHP -- which breaks
 * code that expects an array. Symfony hits this hard: it discovers service classes with
 * glob('.../src/...'), and a false result crashes container compilation
 * (array_fill_keys(false, ...)).
 *
 * We --wrap glob()/globfree() (see main/CMakeLists.txt) and implement them ourselves with
 * opendir()/readdir()/fnmatch() + stat() -- all of which work on this target (opendir/stat go through
 * the fs_pathnorm shims). It fills picolibc's glob_t, so PHP's dir.c reads it as usual.
 *
 * Supported: '*', '?' and '[...]' wildcards in any path component (matched per component, so '*'
 * doesn't cross '/'), GLOB_MARK, GLOB_NOSORT, GLOB_NOCHECK, and one level of GLOB_BRACE '{a,b}'
 * expansion. That covers what PHP/Symfony ask for here; the rarely-used flags are ignored.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <glob.h>

/* picolibc keeps glob()'s return codes behind a BSD-visibility guard; define them if the header
 * didn't expose them (their values are fixed by the API). */
#ifndef GLOB_NOSPACE
#define GLOB_NOSPACE (-1)
#endif
#ifndef GLOB_NOMATCH
#define GLOB_NOMATCH (-3)
#endif

#define GL_MAXPATH 512
#define GL_MAXCOMP 40

/* growable list of matched paths */
struct gl_list {
    char  **v;
    size_t  n, cap;
};

static int gl_add(struct gl_list *g, const char *path)
{
    if (g->n + 1 >= g->cap) {
        size_t nc = g->cap ? g->cap * 2 : 16;
        char **nv = realloc(g->v, nc * sizeof(char *));
        if (!nv) {
            return -1;
        }
        g->v = nv;
        g->cap = nc;
    }
    g->v[g->n] = strdup(path);
    if (!g->v[g->n]) {
        return -1;
    }
    g->n++;
    return 0;
}

static int has_magic(const char *s)
{
    return strpbrk(s, "*?[") != NULL;
}

/* Recurse from an existing directory `base` through the remaining pattern components. */
static int gl_walk(const char *base, char *const *comps, int ncomp, int idx, int mark, struct gl_list *out)
{
    if (idx >= ncomp) {
        return gl_add(out, base);
    }
    const char *comp = comps[idx];
    int last = (idx == ncomp - 1);
    char path[GL_MAXPATH];
    struct stat st;

    if (!has_magic(comp)) {
        snprintf(path, sizeof path, "%s/%s", base, comp);
        if (stat(path, &st) != 0) {
            return 0;   /* literal component doesn't exist */
        }
        if (last) {
            if (mark && S_ISDIR(st.st_mode)) {
                strncat(path, "/", sizeof path - strlen(path) - 1);
            }
            return gl_add(out, path);
        }
        return S_ISDIR(st.st_mode) ? gl_walk(path, comps, ncomp, idx + 1, mark, out) : 0;
    }

    DIR *d = opendir(base);
    if (!d) {
        return 0;
    }
    struct dirent *e;
    int rc = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        if (fnmatch(comp, e->d_name, 0) != 0) {
            continue;
        }
        snprintf(path, sizeof path, "%s/%s", base, e->d_name);
        if (last) {
            if (mark && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncat(path, "/", sizeof path - strlen(path) - 1);
            }
            if (gl_add(out, path) != 0) { rc = -1; break; }
        } else if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (gl_walk(path, comps, ncomp, idx + 1, mark, out) != 0) { rc = -1; break; }
        }
    }
    closedir(d);
    return rc;
}

/* One concrete (brace-free) pattern -> matches. */
static int gl_one(const char *pattern, int mark, struct gl_list *out)
{
    const char *magic = strpbrk(pattern, "*?[");
    if (!magic) {
        struct stat st;
        if (stat(pattern, &st) != 0) {
            return 0;
        }
        char p[GL_MAXPATH];
        snprintf(p, sizeof p, "%s", pattern);
        if (mark && S_ISDIR(st.st_mode)) {
            strncat(p, "/", sizeof p - strlen(p) - 1);
        }
        return gl_add(out, p);
    }

    /* fixed base = up to the last '/' before the first wildcard */
    const char *slash = magic;
    while (slash > pattern && *slash != '/') {
        slash--;
    }
    char base[GL_MAXPATH];
    int baselen = (int) (slash - pattern);
    if (baselen <= 0) {
        base[0] = '/';
        base[1] = '\0';
    } else {
        if (baselen >= (int) sizeof base) {
            baselen = sizeof base - 1;
        }
        memcpy(base, pattern, baselen);
        base[baselen] = '\0';
    }

    char restbuf[GL_MAXPATH];
    snprintf(restbuf, sizeof restbuf, "%s", (*slash == '/') ? slash + 1 : slash);
    char *comps[GL_MAXCOMP];
    int nc = 0;
    for (char *tok = strtok(restbuf, "/"); tok && nc < GL_MAXCOMP; tok = strtok(NULL, "/")) {
        comps[nc++] = tok;
    }
    return gl_walk(base, comps, nc, 0, mark, out);
}

/* Expand one outermost {a,b,c} then recurse; no braces -> straight to gl_one. */
static int gl_expand(const char *pattern, int mark, struct gl_list *out)
{
    const char *lb = strchr(pattern, '{');
    if (!lb) {
        return gl_one(pattern, mark, out);
    }
    const char *rb = NULL;
    int depth = 0;
    for (const char *p = lb; *p; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            if (--depth == 0) { rb = p; break; }
        }
    }
    if (!rb) {
        return gl_one(pattern, mark, out);   /* unbalanced -> treat literally */
    }
    /* for each top-level comma-separated option, build prefix+option+suffix and recurse */
    const char *opt = lb + 1;
    depth = 0;
    for (const char *p = lb + 1; p <= rb; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}' && depth > 0) {
            depth--;
        } else if ((*p == ',' && depth == 0) || p == rb) {
            char buf[GL_MAXPATH];
            int pre = (int) (lb - pattern);
            int mid = (int) (p - opt);
            snprintf(buf, sizeof buf, "%.*s%.*s%s", pre, pattern, mid, opt, rb + 1);
            if (gl_expand(buf, mark, out) < 0) {
                return -1;
            }
            opt = p + 1;
        }
    }
    return 0;
}

static int gl_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *) a, *(const char *const *) b);
}

int __wrap_glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
    (void) errfunc;
    struct gl_list g = { 0 };
    int mark = (flags & GLOB_MARK) ? 1 : 0;

    if (gl_expand(pattern, mark, &g) < 0) {
        for (size_t i = 0; i < g.n; i++) {
            free(g.v[i]);
        }
        free(g.v);
        return GLOB_NOSPACE;
    }

    if (g.n == 0) {
        if (flags & GLOB_NOCHECK) {
            gl_add(&g, pattern);   /* return the pattern itself */
        } else {
            free(g.v);
            pglob->gl_pathc = 0;
            pglob->gl_pathv = NULL;
            pglob->gl_offs = 0;
            /* Return an EMPTY SUCCESS, not GLOB_NOMATCH. PHP's ext/standard/dir.c only maps
             * GLOB_NOMATCH to an empty array when that macro is visible at its compile time, and
             * picolibc keeps it behind a BSD-visibility guard -- so there it treats any non-zero
             * return (including GLOB_NOMATCH) as an error and glob() yields false. A 0 result with
             * gl_pathc == 0 is handled as an empty array either way (Symfony globs optional dirs
             * that don't exist, and expects [] not false). */
            return 0;
        }
    }

    if (!(flags & GLOB_NOSORT) && g.n > 1) {
        qsort(g.v, g.n, sizeof(char *), gl_cmp);
    }

    char **pv = malloc((g.n + 1) * sizeof(char *));
    if (!pv) {
        for (size_t i = 0; i < g.n; i++) {
            free(g.v[i]);
        }
        free(g.v);
        return GLOB_NOSPACE;
    }
    for (size_t i = 0; i < g.n; i++) {
        pv[i] = g.v[i];
    }
    pv[g.n] = NULL;
    free(g.v);

    pglob->gl_pathc = (int) g.n;
    pglob->gl_matchc = (int) g.n;
    pglob->gl_offs = 0;
    pglob->gl_flags = flags;
    pglob->gl_pathv = pv;
    return 0;
}

void __wrap_globfree(glob_t *pglob)
{
    if (!pglob || !pglob->gl_pathv) {
        return;
    }
    for (int i = 0; i < pglob->gl_pathc; i++) {
        free(pglob->gl_pathv[i]);
    }
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}
