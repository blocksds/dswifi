// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

#include <stdlib.h>

#ifdef WIFI_USE_TCP_SGIP

void *sgIP_malloc(int size) __attribute__((weak));
void sgIP_free(void *ptr) __attribute__((weak));

//////////////////////////////////////////////////////////////////////////
// wifi heap allocator system

#    define WHEAP_RECORD_FLAG_INUSE  0
#    define WHEAP_RECORD_FLAG_UNUSED 1
#    define WHEAP_RECORD_FLAG_FREED  2

typedef struct WHEAP_RECORD
{
    struct WHEAP_RECORD *next;
    unsigned short flags, unused;
    int size;
} wHeapRecord;

#    ifdef SGIP_DEBUG
#        define WHEAP_FILL_START 0xAA
#        define WHEAP_FILL_END   0xBB
#        define WHEAP_PAD_START  4
#        define WHEAP_PAD_END    4
#        define WHEAP_DO_PAD
#    else
#        define WHEAP_PAD_START 0
#        define WHEAP_PAD_END   0
#        undef WHEAP_DO_PAD
#    endif
#    define WHEAP_RECORD_SIZE (sizeof(wHeapRecord))
#    define WHEAP_PAD_SIZE    ((WHEAP_PAD_START) + (WHEAP_PAD_END))
#    define WHEAP_SIZE_CUTOFF ((WHEAP_RECORD_SIZE) + 64)

int wHeapsize;
wHeapRecord *wHeapStart; // start of heap
wHeapRecord *wHeapFirst; // first free block

void wHeapAllocInit(int size)
{
    wHeapStart = (wHeapRecord *)malloc(size);
    if (!wHeapStart)
        return;

    wHeapFirst        = wHeapStart;
    wHeapStart->flags = WHEAP_RECORD_FLAG_UNUSED;
    wHeapStart->next  = 0;
    wHeapStart->size  = size - sizeof(wHeapRecord);
}

void *wHeapAlloc(int size)
{
    wHeapRecord *rec = wHeapFirst;
    void *voidptr;
    int n;
    size = (size + 3) & (~3);
    if (size == 0)
        size = 4;
    size += WHEAP_PAD_SIZE;

    if (!rec)
    {
        // should not happen given normal use.
        SGIP_DEBUG_MESSAGE(("wHeapAlloc: heap full!"));
        return 0;
    }
    while (rec->size < size)
    {
        if (!rec->next)
        {
            // cannot alloc
            SGIP_DEBUG_MESSAGE(("wHeapAlloc: heap too full!"));
            return 0;
        }
        if (rec->next->flags != WHEAP_RECORD_FLAG_INUSE)
        {
            // try to merge with next one
            rec->size += rec->next->size + WHEAP_RECORD_SIZE;
            rec->next = rec->next->next;
        }
        else
        {
            // skip ahead to more friendly waters
            rec = rec->next;
            while (rec->next)
            {
                if (rec->flags != WHEAP_RECORD_FLAG_INUSE)
                    break;
                rec = rec->next;
            }
            if (rec->flags == WHEAP_RECORD_FLAG_INUSE)
            {
                // no empty slots :(
                SGIP_DEBUG_MESSAGE(("wHeapAlloc: heap too full!"));
                return 0;
            }
        }
    }
    rec->flags = WHEAP_RECORD_FLAG_INUSE;
    n          = rec->size - size;
    voidptr    = ((char *)rec) + WHEAP_RECORD_SIZE + WHEAP_PAD_START;
    if (n < WHEAP_SIZE_CUTOFF)
    {
        // pad to include unused portion
        rec->unused = n;
    }
    else
    {
        // chop block into 2
        wHeapRecord *rec2;
        rec2        = (wHeapRecord *)(((char *)rec) + WHEAP_RECORD_SIZE + size);
        rec2->flags = WHEAP_RECORD_FLAG_UNUSED;
        rec2->size  = rec->size - size - WHEAP_RECORD_SIZE;
        rec->size   = size;
        rec2->next  = rec->next;
        rec->next   = rec2;
        rec->unused = 0;
    }
    if (rec == wHeapFirst)
    {
        while (wHeapFirst->next && wHeapFirst->flags == WHEAP_RECORD_FLAG_INUSE)
            wHeapFirst = wHeapFirst->next;
        if (wHeapFirst->flags == WHEAP_RECORD_FLAG_INUSE)
            wHeapFirst = 0;
    }
#    ifdef WHEAP_DO_PAD
    {
        int i;
        for (i = 0; i < WHEAP_PAD_START; i++)
        {
            (((unsigned char *)rec) + WHEAP_RECORD_SIZE)[i] = WHEAP_FILL_START;
        }
        for (i = 0; i < WHEAP_PAD_END; i++)
        {
            (((unsigned char *)rec) + WHEAP_RECORD_SIZE + size - WHEAP_PAD_END)[i] = WHEAP_FILL_END;
        }
    }
#    endif
    return voidptr;
}

void wHeapFree(void *data)
{
    wHeapRecord *rec = (wHeapRecord *)(((char *)data) - WHEAP_RECORD_SIZE - WHEAP_PAD_START);
#    ifdef WHEAP_DO_PAD
    {
        int size = rec->size - rec->unused;
        int i;
        for (i = 0; i < WHEAP_PAD_START; i++)
        {
            if ((((unsigned char *)rec) + WHEAP_RECORD_SIZE)[i] != WHEAP_FILL_START)
                break;
        }
        if (i != WHEAP_PAD_START)
        {
            // note heap error
            SGIP_DEBUG_MESSAGE(("wHeapFree: Corruption found before allocated data! 0x%X", data));
        }
        for (i = 0; i < WHEAP_PAD_END; i++)
        {
            if ((((unsigned char *)rec) + WHEAP_RECORD_SIZE + size - WHEAP_PAD_END)[i]
                != WHEAP_FILL_END)
                break;
        }
        if (i != WHEAP_PAD_END)
        {
            // note heap error
            SGIP_DEBUG_MESSAGE(("wHeapFree: Corruption found after allocated data! 0x%x", data));
        }
    }
#    endif
    if (rec->flags != WHEAP_RECORD_FLAG_INUSE)
    {
        // note heap error
        SGIP_DEBUG_MESSAGE(("wHeapFree: Data already freed! 0x%X", data));
    }
    rec->flags = WHEAP_RECORD_FLAG_FREED;
    if (rec < wHeapFirst || !wHeapFirst)
        wHeapFirst = rec; // reposition the "starting" pointer.
}

//////////////////////////////////////////////////////////////////////////

void *sgIP_malloc(int size)
{
    return wHeapAlloc(size);
}

void sgIP_free(void *ptr)
{
    wHeapFree(ptr);
}

#endif
