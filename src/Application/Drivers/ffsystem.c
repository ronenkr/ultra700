#include "ff.h"
#include <stddef.h>
#include <stdint.h>
#include "systypes.h"
#include "memory.h"

void* ff_memalloc (UINT msize)
{
    if (msize == 0) return NULL;
    return malloc(msize);
}

void ff_memfree (void* mblock)
{
    if (mblock) free(mblock);
}

/* Synchronization not enabled (FF_FS_REENTRANT == 0), so no mutex hooks needed. */