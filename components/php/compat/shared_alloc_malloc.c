/*
 * A "shared memory" backend for Zend OPcache that just allocates a plain block from the heap.
 *
 * OPcache's real backends use mmap()/shmget() to share one segment across FPM worker PROCESSES.
 * This firmware runs a single process with the PHP engine kept alive across HTTP requests, so there
 * is nothing to share between processes -- we only need the compiled bytecode to persist in memory
 * from one request to the next. A normal allocation does exactly that: it lives for the whole
 * process. With USE_ZEND_ALLOC=0 and SPIRAM_MALLOC_ALWAYSINTERNAL=0, a large allocation lands in the
 * 32 MB PSRAM, which is where we want the cache.
 *
 * Enabled with -DUSE_MALLOC_SHM; registered in the handler table by patch 0007. Locking is a no-op
 * (see that patch): a single task serves one request at a time, so there is no contention.
 */
#include <stdlib.h>
#include <string.h>
#include "zend_shared_alloc.h"

#ifdef USE_MALLOC_SHM

#define SEG_ALIGN 64   /* generous alignment for the segment base (opcache aligns within it) */

static int create_segments(size_t requested_size, zend_shared_segment ***shared_segments_p,
                           int *shared_segments_count, const char **error_in)
{
    zend_shared_segment *shared_segment;
    size_t asize = (requested_size + SEG_ALIGN - 1) & ~((size_t) SEG_ALIGN - 1);

    void *p = aligned_alloc(SEG_ALIGN, asize);   /* lands in PSRAM given this build's malloc config */
    if (!p) {
        *error_in = "aligned_alloc";
        return ALLOC_FAILURE;
    }
    memset(p, 0, asize);

    *shared_segments_count = 1;
    *shared_segments_p = (zend_shared_segment **) calloc(1, sizeof(zend_shared_segment) + sizeof(void *));
    if (!*shared_segments_p) {
        free(p);
        *error_in = "calloc";
        return ALLOC_FAILURE;
    }
    shared_segment = (zend_shared_segment *) ((char *) (*shared_segments_p) + sizeof(void *));
    (*shared_segments_p)[0] = shared_segment;

    shared_segment->p = p;
    shared_segment->pos = 0;
    shared_segment->size = requested_size;
    return ALLOC_SUCCESS;
}

static int detach_segment(zend_shared_segment *shared_segment)
{
    free(shared_segment->p);
    return 0;
}

static size_t segment_type_size(void)
{
    return sizeof(zend_shared_segment);
}

const zend_shared_memory_handlers zend_alloc_malloc_handlers = {
    create_segments,
    detach_segment,
    segment_type_size
};

#endif /* USE_MALLOC_SHM */
