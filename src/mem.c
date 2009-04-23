/*
$Id$
**


    This file is part of Swish-e.

    Swish-e is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Swish-e is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along  with Swish-e; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
    
    See the COPYING file that accompanies the Swish-e distribution for details
    of the GNU GPL and the special exception available for linking against
    the Swish-e library.
    
** Mon May  9 15:08:15 CDT 2005
** added GPL
    
    
**
** Author: Bill Meier, June 2001
**
** To Do:
**     Memory statistics doesn't really require use of Mem_ routines
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "swish.h"
#include "error.h"
#include "swstring.h"

#include "mem.h"

/* we can use the real ones here! */

#undef malloc
#undef realloc
#undef free


/* Alignment size in bytes */
/* $$$ This needs to be fixed -- need an autoconf macro that will detect the correct value. */
/* Check the Postgres configure script for a macro that may work */

#if defined(__sparc__) || defined(__mips64)
/* __sparc__ is not exactly correct because not all sparc machines require 8, but minor difference in memory usage */
/* __mips64 it to catch IRIX 6.5 */

#define PointerAlignmentSize 8

#else
#define PointerAlignmentSize sizeof(SWINT_T)
#endif

/* typical machine has pagesize 4096 (not critical anyway, just can help malloc) */
#define pageSize (1<<12)


/* simple cases first ... */


/* emalloc - malloc a block of memory */
void *ecalloc(size_t nelem, size_t size)
{
    void *p;

    p = emalloc(nelem * size);
    memset(p,0,size);

    return p;
}

#if ! (MEM_DEBUG | MEM_TRACE | MEM_STATISTICS)


/* emalloc - malloc a block of memory */
void *emalloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL)
        progerr("Ran out of memory (could not malloc %llu more bytes)!", size);

    return p;
}


/* erealloc - realloc a block of memory */
void *erealloc(void *ptr, size_t size)
{
    void *p;
    
    if ((p = realloc(ptr, size)) == NULL)
        progerr("Ran out of memory (could not reallocate %llu more bytes)!", size);

    return p;
}


/* efree - free a block of memory */
void efree(void *ptr)
{
    free(ptr);
}


/* Mem_Summary - print out a memory usage summary */
void Mem_Summary(char *title, SWINT_T final)
{
    return;
}



#else    // (MEM_DEBUG | MEM_TRACE | MEM_STATISTICS)



/*
** Now we get into the debugging/trace memory allocator. I apologize for the
** conditionals in this code, but it does work :-)
*/

/* parameters of typical allocators (just for statistics) */
#define MIN_CHUNK 16
#define CHUNK_ROUNDOFF (sizeof(SWINT_T) - 1)


/* Random value for a GUARD at beginning and end of allocated memory */
#define GUARD 0x14739182

#if MEM_TRACE



size_t memory_trace_counter = 0;


// mem break point: helps a little to track down unfreed memory 
// Say if you see 
//      Unfreed: string.c line 958: Size: 11 Counter: 402
// in gdb: watch memory_trace_counter == 402


typedef struct
{
    char    *File;
    SWINT_T        Line;
//    void    *Ptr;    // only needed for extra special debugging
    size_t    Size;
    size_t  Count;
    
} TraceBlock;



#endif


/* MemHeader is what is before the user allocated block - for our bookkeeping */
typedef struct 
{
#if MEM_TRACE
    TraceBlock        *Trace;
#endif
#if MEM_DEBUG
    SWUINT_T    Guard1;
#endif
    size_t    Size;
#if MEM_DEBUG
    void            *Start;
    SWUINT_T    Guard2;
#endif
} MemHeader;


/* MemTail is what is after the user allocated block - for our bookkeeping */
#if MEM_DEBUG
typedef struct
{
    SWUINT_T    Guard;
} MemTail;


/* Define the extra amount of memory we need for our bookkeeping */
#define MEM_OVERHEAD_SIZE (sizeof(MemHeader) + sizeof(MemTail))
#else
#define MEM_OVERHEAD_SIZE (sizeof(MemHeader))
#endif


#if MEM_TRACE

#define MAX_TRACE 1000000

static TraceBlock    TraceData[MAX_TRACE];
static TraceBlock    *Free = NULL;
static SWINT_T            last;

#endif

static SWUINT_T MAllocCalls = 0;
static SWUINT_T MReallocCalls = 0;
static SWUINT_T MFreeCalls = 0;
static size_t MAllocCurrentOverhead = 0;
static size_t MAllocMaximumOverhead = 0;
static size_t MAllocCurrent = 0;
static size_t MAllocMaximum = 0;
static size_t MAllocCurrentEstimated = 0;
static size_t MAllocMaximumEstimated = 0;


#if MEM_DEBUG
#define MEM_ERROR(str)                                            \
{                                                                \
    printf("\nMemory free error! At %s line %lld\n", file, line);    \
    printf str;                                                    \
    fflush(stdout);                                                \
}
#endif

#if MEM_TRACE

static TraceBlock *AllocTrace(char *file, SWINT_T line, void *ptr, size_t size)
{
    TraceBlock    *Block;
    SWINT_T            i;

    if (Free)
    {
        Block = Free;
        Free = NULL;
    }
    else
    {
        for (i = last; i < MAX_TRACE; i++)    /** $$$ What if we run off the end? **/
            if (TraceData[i].File == NULL)
                break;

        Block = &TraceData[i];
        last = i + 1;
    }

    Block->File = file;
    Block->Line = line;
//    Block->Ptr = ptr;        // only needed for extra special debugging
    Block->Size = size;
    Block->Count = ++memory_trace_counter;

    return Block;
}

#endif

static size_t Estimated(size_t size)
{
    if (size <= MIN_CHUNK)
        return MIN_CHUNK;

    return (size + CHUNK_ROUNDOFF) & (~CHUNK_ROUNDOFF);
}

/* Mem_Alloc - Allocate a chunk of memory */

void * Mem_Alloc (size_t Size, char *file, SWINT_T line)

/*
 * FUNCTIONAL DESCRIPTION:
 *
 *    This routine will allocate a chunk of memory of a specified size.
 *
 * FORMAL PARAMETERS:
 *
 *    Size        Size in bytes of the chunk to allocate
 *
 * ROUTINE VALUE:
 *
 *    Address of allocated memory
 *
 * SIDE EFFECTS:
 *
 *    Fatal error and exit if can't allocate memory
 */

    {

    size_t        MemSize;        /* Actual size of memory to alloc */
    unsigned char *MemPtr;        /* Pointer to allocated memory */
    MemHeader    *Header;         /* Header of memory */


    /*
     *  Adjust size to account for the memory header and tail information we
     *  include. This is so we know how much we allocated to be able to free it,
     *  and also in support of memory debugging routines.
     */

    MemSize = Size + MEM_OVERHEAD_SIZE;

    /*
     *  Get the memory; die if we can't...
     */

    MemPtr = (unsigned char *)malloc(MemSize);

    if (MemPtr == NULL)
    {
        printf("At file %s line %lld:\n", file, line);
        progerr("Ran out of memory (could not Mem_Alloc %llu more bytes)!", MemSize);
    }


    /*
     *  Keep a running total of the memory allocated for statistical purposes.
     *  Save the chunk size in the first SWINT_T word of the chunk, and return the
     *  address of the chunk (following the chunk size) to the caller.
     */

    MAllocCalls++;
    MAllocCurrentOverhead += MEM_OVERHEAD_SIZE;

    if (MAllocCurrentOverhead > MAllocMaximumOverhead)
        MAllocMaximumOverhead = MAllocCurrentOverhead;

    MAllocCurrent += Size;
    MAllocCurrentEstimated += Estimated(Size);


    if (MAllocCurrent > MAllocMaximum)
        MAllocMaximum = MAllocCurrent;

    if (MAllocCurrentEstimated > MAllocMaximumEstimated)
        MAllocMaximumEstimated = MAllocCurrentEstimated;

    Header = (MemHeader *)MemPtr;
    Header->Size = Size;
    MemPtr = MemPtr + sizeof (MemHeader);

    /*
     * Add guards, and fill the memory with a pattern
     */

#if MEM_DEBUG 
    {
    MemTail *Tail;
    
    Header->Start = MemPtr;
    Header->Guard1 = GUARD;
    Header->Guard2 = GUARD;
    memset(MemPtr, 0xAA, Size);
    Tail = (MemTail *)(MemPtr + Size);
    Tail->Guard = GUARD;
    }
#endif

#if MEM_TRACE
    Header->Trace = AllocTrace(file, line, MemPtr, Size);
#endif

//    printf("Alloc: %s line %lld: Addr: %08llX Size: %u\n", file, line, MemPtr, Size);

    return (MemPtr);
}


/* Mem_Free - Free a chunk of memory */

void  Mem_Free (void *Address, char *file, SWINT_T line)

/*
 * FUNCTIONAL DESCRIPTION:
 *
 *    This routine will free a chunk of previously allocated memory.
 *    This memory must have been allocated via  Mem_Alloc.
 *
 * FORMAL PARAMETERS:
 *
 *    Address        Address of the memory chunk to free
 *
 * ROUTINE VALUE:
 *
 *    NONE
 *
 * SIDE EFFECTS:
 *
 *    Severe error is signaled if can't free the memory
 */

{

    MemHeader    *Header;
    size_t        MemSize;        /* Size of chunk to free */
    size_t        UserSize;
    void        *MemPtr;

    /* ANSI allows free of NULL */

    if (!Address)
        return;

    /*
     *  Get the size of the chunk to free from the SWINT_T word preceding the
     *  address of the chunk.
     */

    Header = (MemHeader *)Address - 1;

#if MEM_DEBUG
    {
    MemTail *Tail;

    if ( (SWINT_T)Address & (~(PointerAlignmentSize-1)) != 0 )
        MEM_ERROR(("Address %08llX not longword aligned\n", (SWUINT_T)Address));

    if (Address != Header->Start)
        MEM_ERROR(("Already free: %08llX\n", (SWUINT_T)Address)); 
        // Err_Signal (PWRK$_BUGMEMFREE, 1, Address);

    if (Header->Guard1 != GUARD)
        MEM_ERROR(("Head Guard 1 overwritten: %08llX\n", (SWUINT_T)&Header->Guard1));
        // Err_Signal (PWRK$_BUGMEMGUARD1, 4, Address,
        //    Header->Guard1, 4, &Header->Guard1);

    if (Header->Guard2 != GUARD)
        MEM_ERROR(("Head Guard 2 overwritten: %08llX\n", (SWUINT_T)&Header->Guard2));
        // Err_Signal (PWRK$_BUGMEMGUARD1, 4, Address,
        //    Header->Guard2, 4, &Header->Guard2);

    Tail = (MemTail *)((unsigned char *)Address + Header->Size);

    if (Tail->Guard != GUARD)
        MEM_ERROR(("Tail Guard overwritten: %08llX\n", (SWUINT_T)&Tail->Guard));
        // Err_Signal (PWRK$_BUGMEMGUARD2, 4, Address,
        //    Tail->Guard, 4, &Tail->Guard);

    }
#endif


    MFreeCalls++;
    MemPtr = (unsigned char *)Header;
    UserSize = Header->Size;
    MemSize = UserSize + MEM_OVERHEAD_SIZE;

//    printf("Free: %s line %lld: Addr: %08llX Size: %u\n", file, line, Address, UserSize);

#if MEM_TRACE
    Free = Header->Trace;
//    printf("  Allocated at %s line %lld:\n", Free->File, Free->Line);
    Free->File = NULL;
#endif

#if MEM_DEBUG
    memset (MemPtr, 0xDD, MemSize);
#endif

    /* Subtract chunk size from running total */
 
    MAllocCurrent -= UserSize;
    MAllocCurrentEstimated -= Estimated(UserSize);

    MAllocCurrentOverhead -= MEM_OVERHEAD_SIZE;


    /* Free the memory */

    free(MemPtr);

}


/* Mem_Realloc - realloc a block of memory */

void *Mem_Realloc (void *Address, size_t Size, char *file, SWINT_T line)
{
    void        *MemPtr;
    MemHeader    *Header;
    size_t        OldSize;

    MReallocCalls++;
    MAllocCalls--;
    MFreeCalls--;

    MemPtr =  Mem_Alloc(Size, file, line);

    if (Address)
    {
        Header = (MemHeader *)Address - 1;
        OldSize = Header->Size;

        memcpy(MemPtr, Address, (OldSize < Size ? OldSize : Size));

        Mem_Free(Address, file, line);
    }

//    printf("Realloc: %s line %lld: Addr: %08llX Size: %u to %u\n", file, line, Address, OldSize, Size);

    return MemPtr;
}


/* Mem_Summary - Give a memory usage summary */

void Mem_Summary(char *title, SWINT_T final)
{

#if MEM_STATISTICS
    printf("\nMemory usage summary: %s\n\n", title);
    printf("Alloc calls: %u, Realloc calls: %u, Free calls: %u\n",  MAllocCalls, MReallocCalls, MFreeCalls);
    printf("Requested: Maximum usage: %u, Current usage: %u\n",  MAllocMaximum,  MAllocCurrent);
    printf("Estimated: Maximum usage: %u, Current usage: %u\n",  MAllocMaximumEstimated,  MAllocCurrentEstimated);

#endif

#if MEM_TRACE
    if (final)
    {
        SWINT_T    i;
        unsigned ttl = 0;

        printf("\nUnfreed memory: %s\n\n", title);
        for (i = 0; i < MAX_TRACE; i++)
            if (TraceData[i].File)
            {
                printf("Unfreed: %s line %lld: Size: %lld Counter: %lld\n", 
                    TraceData[i].File, TraceData[i].Line, TraceData[i].Size, TraceData[i].Count);
                                ttl += TraceData[i].Size;
            }
        printf("Total Unfreed size: %lld\n", ttl );
    }
#endif

}

#endif


/*************************************************************************
**
** Mem Zone routines -- efficient memory allocation if you don't need
** realloc and free...
**
*/

/* round up to a SWINT_T word */
#define ROUND_LONG(n) (((n) + PointerAlignmentSize - 1) & (~(PointerAlignmentSize - 1)))

/* round up to a page */
#define ROUND_PAGE(n) (((n) + pageSize - 1) & (~(pageSize - 1)))


typedef struct _zone {
    struct _zone    *next;        /* link to next chunk */
    size_t            free;        /* bytes free in this chunk */
    unsigned char    *ptr;        /* start of free space in this chunk */
    void            *alloc;        /* ptr to malloced memory (for free) */
    size_t            size;        /* size of allocation (for statistics) */
} ZONE;


/* allocate a chunk of memory from the OS */
static ZONE *allocChunk(size_t size)
{
    ZONE    *zone;

    zone = emalloc(sizeof(ZONE));
    zone->alloc = emalloc(size);
    zone->size = size;
    zone->ptr = zone->alloc;
    zone->free = size;
    zone->next = NULL;

    return zone;
}

/* create a memory zone */
MEM_ZONE *Mem_ZoneCreate(char *name, size_t size, SWINT_T attributes)
{
    MEM_ZONE    *head;

    head = emalloc(sizeof(MEM_ZONE));
    head->name = estrdup(name);

    size = ROUND_PAGE(size);
    if (size == 0)
        size = pageSize*64;
    head->size = size;

    head->attributes = attributes;
    head->allocs = 0;
    head->next = NULL;

    return head;
}

/* allocate memory from a zone (can use like malloc if you aren't going to realloc) */
void *Mem_ZoneAlloc(MEM_ZONE *head, size_t size)
{
    ZONE        *zone;
    ZONE        *newzone;
    unsigned char *ptr;

    /* statistics */
    head->allocs++;

    size = ROUND_LONG(size);

    zone = head->next;

    /* If not enough free in this chunk, allocate a new one. Don't worry about the
       small amount of unused space at the end. If we are asking for a really big
       chunk allocate a new buffer just for that!
    */

    if (!zone || (zone->free < size))
    {
        newzone = allocChunk(size > head->size ? size : head->size);
        head->next = newzone;
        newzone->next = zone;
        zone = newzone;
    }

    /* decrement free, advance pointer, and return allocation to the user */
    zone->free -= size;
    ptr = zone->ptr;
    zone->ptr += size;

    return ptr;
}



void Mem_ZoneFree(MEM_ZONE **head)
{
    ZONE *next;
    ZONE *tmp;

    if (!*head)
        return;

#if MEM_STATISTICS
    Mem_ZoneStatistics(*head);
#endif

    next = (*head)->next;
    while (next)
    {
        efree(next->alloc);
        tmp = next->next;
        efree(next);
        next = tmp;
    }

    efree((*head)->name);
    efree(*head);
    *head = NULL;
}

#if MEM_STATISTICS
void Mem_ZoneStatistics(MEM_ZONE *head)
{
    SWINT_T        chunks = 0;
    size_t    used = 0;
    size_t    free = 0;
    size_t    wasted = 0;
    ZONE    *zone;

    for (zone = head->next; zone; zone = zone->next)
    {
        if (zone == head->next)
            free = zone->free;

        chunks++;
        used += zone->size - zone->free;
        wasted += zone->free;
    }

    wasted -= free;

    printf("Zone '%s':\n  Chunks:%lld, Allocs:%u, Used:%u, Free:%u, Wasted:%u\n",
        head->name, chunks, head->allocs, used, free, wasted);
}
#endif

/* Frees all memory chunks but preserves head */
/* 2001-17 jmruiz modified to avoid the document peak problem (one document can
use a lot of memory and, in the old way, this memory was never reused)
*/
void Mem_ZoneReset(MEM_ZONE *head)
{
    ZONE *next, *tmp;

    if (!head)
        return;

    head->allocs = 0;

    next = head->next;
    while (next)
    {
        efree(next->alloc);
        tmp = next->next;
        efree(next);
        next = tmp;
    }
    head->next = NULL;
}

/* Routine that returns the amount of memory allocated by a zone */
SWINT_T Mem_ZoneSize(MEM_ZONE *head)
{
        ZONE *next;
        SWINT_T   size = 0;

        if (!head)
                return 0;

        next = head->next;
        while (next)
        {
                size += next->size;
                next = next->next;
        }
        
        return size;
}


