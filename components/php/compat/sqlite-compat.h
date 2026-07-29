/*
 * Force-included into sqlite-amalgamation/sqlite3.c only (see the php component's
 * CMakeLists.txt). Two things need neutralizing before SQLite's code is seen:
 *
 *  - The project defines HAVE_INT32_T / HAVE_UINT32_T / HAVE_INT64_T /
 *    HAVE_UINT64_T globally (they stop timelib from re-typedef'ing those types).
 *    SQLite uses the same macros to pick ITS integer typedefs, which would make
 *    u32 = uint32_t = unsigned long on this ILP32 target and clash with SQLite's
 *    own <unsigned int> public API. Undefine them so SQLite falls back to its
 *    default (u32 = unsigned int).
 *
 *  - newlib doesn't declare lstat(), so the unix VFS won't compile. FATFS has no
 *    symlinks, so map SQLite's single use of it onto stat() (same signature).
 */
#undef HAVE_INT32_T
#undef HAVE_UINT32_T
#undef HAVE_INT64_T
#undef HAVE_UINT64_T

#define lstat stat

/*
 * SQLite has struct members that exist only under SQLITE_DEBUG and are touched
 * only by asserts and #ifndef NDEBUG code. We don't set SQLITE_DEBUG, so those
 * members are absent, and any live reference to them fails to compile. Two things
 * are needed to make the amalgamation's release build consistent:
 *
 *  - NDEBUG: turns off SQLite's own #ifndef NDEBUG debug code (VVA_ONLY, ...).
 *  - a real no-op assert(): ESP-IDF's assert.h, under NDEBUG but with
 *    CONFIG_COMPILER_ASSERT_NDEBUG_EVALUATE, expands assert(e) to ((void)(e)),
 *    which STILL evaluates e -- touching those absent members. Pull in assert.h
 *    now (it is #pragma once, so SQLite's later include is a no-op) and replace
 *    assert with a genuine no-op.
 */
#ifndef NDEBUG
#define NDEBUG 1
#endif

#include <assert.h>
#undef assert
#define assert(X) ((void)0)
