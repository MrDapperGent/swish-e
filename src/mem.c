/*
$Id$
**
** This program and library is free software; you can redistribute it and/or
** modify it under the terms of the GNU (Library) General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU (Library) General Public License for more details.
**
** You should have received a copy of the GNU (Library) General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** Author: Bill Meier, June 2001
**
** To Do:
**     Memory statistics doesn't really require use of Mem_ routines
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "error.h"
#include "swish.h"

#include "mem.h"

/* we can use the real ones here! */

#undef malloc
#undef realloc
#undef free

/* simple cases first ... */

#if ! (MEM_DEBUG | MEM_TRACE | MEM_STATISTICS)


/* emalloc - malloc a block of memory */
void *emalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		progerr("Ran out of memory (could not allocate enough)!");

	return p;
}


/* erealloc - realloc a block of memory */
void *erealloc(void *ptr, size_t size)
{
	void *p;
	
	if ((p = realloc(ptr, size)) == NULL)
		progerr("Ran out of memory (could not reallocate enough)!");

	return p;
}


/* efree - free a block of memory */
void efree(void *ptr)
{
	free(ptr);
}


/* Mem_Summary - print out a memory usage summary */
void Mem_Summary(char *title, int final)
{
	return;
}



#else	// (MEM_DEBUG | MEM_TRACE | MEM_STATISTICS)



/*
** Now we get into the debugging/trace memory allocator. I apologize for the
** conditionals in this code, but it does work :-)
*/

/* parameters of typical allocators (just for statistics) */
#define MIN_CHUNK 16
#define CHUNK_ROUNDOFF (sizeof(long) - 1)


/* Random value for a GUARD at beginning and end of allocated memory */
#define GUARD 0x14739182

#if MEM_TRACE

typedef struct
{
	char	*File;
	int		Line;
//	void	*Ptr;	// only needed for extra special debugging
	size_t	Size;
} TraceBlock;

#endif


/* MemHeader is what is before the user allocated block - for our bookkeeping */
typedef struct 
{
#if MEM_TRACE
	TraceBlock		*Trace;
#endif
#if MEM_DEBUG
    unsigned long	Guard1;
#endif
    size_t	Size;
#if MEM_DEBUG
    void			*Start;
    unsigned long	Guard2;
#endif
} MemHeader;


/* MemTail is what is after the user allocated block - for our bookkeeping */
#if MEM_DEBUG
typedef struct
{
    unsigned long	Guard;
} MemTail;


/* Define the extra amount of memory we need for our bookkeeping */
#define MEM_OVERHEAD_SIZE (sizeof(MemHeader) + sizeof(MemTail))
#else
#define MEM_OVERHEAD_SIZE (sizeof(MemHeader))
#endif


#if MEM_TRACE

#define MAX_TRACE 1000000

static TraceBlock	TraceData[MAX_TRACE];
static TraceBlock	*Free = NULL;
static int			last;

#endif

static unsigned long MAllocCalls = 0;
static unsigned long MReallocCalls = 0;
static unsigned long MFreeCalls = 0;
static unsigned long MAllocCurrentOverhead = 0;
static unsigned long MAllocMaximumOverhead = 0;
static unsigned long MAllocCurrent = 0;
static unsigned long MAllocMaximum = 0;
static unsigned long MAllocCurrentEstimated = 0;
static unsigned long MAllocMaximumEstimated = 0;


#if MEM_DEBUG
#define MEM_ERROR(str)											\
{																\
	printf("\nMemory free error! At %s line %d\n", file, line);	\
	printf str;													\
	fflush(stdout);												\
}
#endif

#if MEM_TRACE

static TraceBlock *AllocTrace(char *file, int line, void *ptr, size_t size)
{
	TraceBlock	*Block;
	int			i;

	if (Free)
	{
		Block = Free;
		Free = NULL;
	}
	else
	{
		for (i = last; i < MAX_TRACE; i++)	/** $$$ What if we run off the end? **/
			if (TraceData[i].File == NULL)
				break;

		Block = &TraceData[i];
		last = i + 1;
	}

	Block->File = file;
	Block->Line = line;
//	Block->Ptr = ptr;		// only needed for extra special debugging
	Block->Size = size;

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

void * Mem_Alloc (size_t Size, char *file, int line)

/*
 * FUNCTIONAL DESCRIPTION:
 *
 *	This routine will allocate a chunk of memory of a specified size.
 *
 * FORMAL PARAMETERS:
 *
 *	Size		Size in bytes of the chunk to allocate
 *
 * ROUTINE VALUE:
 *
 *	Address of allocated memory
 *
 * SIDE EFFECTS:
 *
 *	Severe error is signaled if can't allocate memory
 */

    {

    size_t		MemSize;		/* Actual size of memory to alloc */
    unsigned char *MemPtr;		/* Pointer to allocated memory */
    MemHeader	*Header; 		/* Header of memory */


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
		progerr("Ran out of memory (could not allocate enough)!");


    /*
     *  Keep a running total of the memory allocated for statistical purposes.
     *  Save the chunk size in the first long word of the chunk, and return the
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

//	printf("Alloc: %s line %d: Addr: %08X Size: %d\n", file, line, MemPtr, Size);

    return (MemPtr);
}


/* Mem_Free - Free a chunk of memory */

void  Mem_Free (void *Address, char *file, int line)

/*
 * FUNCTIONAL DESCRIPTION:
 *
 *	This routine will free a chunk of previously allocated memory.
 *	This memory must have been allocated via  Mem_Alloc.
 *
 * FORMAL PARAMETERS:
 *
 *	Address		Address of the memory chunk to free
 *
 * ROUTINE VALUE:
 *
 *	NONE
 *
 * SIDE EFFECTS:
 *
 *	Severe error is signaled if can't free the memory
 */

{

    MemHeader	*Header;
    size_t		MemSize;		/* Size of chunk to free */
    size_t		UserSize;
    void		*MemPtr;

	/* ANSI allows free of NULL */

	if (!Address)
		return;

    /*
     *  Get the size of the chunk to free from the long word preceding the
     *  address of the chunk.
     */

    Header = (MemHeader *)Address - 1;

#if MEM_DEBUG
    {
    MemTail *Tail;

	if (Address == (void *)0xAAAAAAAA)
		MEM_ERROR(("Address %08X is uninitialized memory\n", Address));

    if (Address != Header->Start)
		MEM_ERROR(("Already free: %0X\n", Address)); 
		// Err_Signal (PWRK$_BUGMEMFREE, 1, Address);

    if (Header->Guard1 != GUARD)
		MEM_ERROR(("Head Guard 1 overwritten: %08X\n", &Header->Guard1));
		// Err_Signal (PWRK$_BUGMEMGUARD1, 4, Address,
		//	Header->Guard1, 4, &Header->Guard1);

    if (Header->Guard2 != GUARD)
		MEM_ERROR(("Head Guard 2 overwritten: %08X\n", &Header->Guard2));
		// Err_Signal (PWRK$_BUGMEMGUARD1, 4, Address,
		//	Header->Guard2, 4, &Header->Guard2);

    Tail = (MemTail *)((unsigned char *)Address + Header->Size);

    if (Tail->Guard != GUARD)
		MEM_ERROR(("Tail Guard overwritten: %08X\n", &Tail->Guard));
		// Err_Signal (PWRK$_BUGMEMGUARD2, 4, Address,
		//	Tail->Guard, 4, &Tail->Guard);

    }
#endif


    MFreeCalls++;
    MemPtr = (unsigned char *)Header;
    UserSize = Header->Size;
    MemSize = UserSize + MEM_OVERHEAD_SIZE;

//	printf("Free: %s line %d: Addr: %08X Size: %d\n", file, line, Address, UserSize);

#if MEM_TRACE
	Free = Header->Trace;
//	printf("  Allocated at %s line %d:\n", Free->File, Free->Line);
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

void *Mem_Realloc (void *Address, size_t Size, char *file, int line)
{
	void		*MemPtr;
	MemHeader	*Header;
	size_t		OldSize;

	MReallocCalls++;
	MAllocCalls--;
	MFreeCalls--;

	MemPtr =  Mem_Alloc(Size, file, line);

	Header = (MemHeader *)Address - 1;
	OldSize = Header->Size;

	memcpy(MemPtr, Address, (OldSize < Size ? OldSize : Size));

	Mem_Free(Address, file, line);

//	printf("Realloc: %s line %d: Addr: %08X Size: %d to %d\n", file, line, Address, OldSize, Size);

	return MemPtr;
}


/* Mem_Summary - Give a memory usage summary */

void Mem_Summary(char *title, int final)
{

#if MEM_STATISTICS
	printf("\nMemory usage summary: %s\n\n", title);
	printf("Alloc calls: %d, Realloc calls: %d, Free calls: %d\n",  MAllocCalls, MReallocCalls, MFreeCalls);
	printf("Requested: Maximum usage: %d, Current usage: %d\n",  MAllocMaximum,  MAllocCurrent);
	printf("Estimated: Maximum usage: %d, Current usage: %d\n",  MAllocMaximumEstimated,  MAllocCurrentEstimated);

#endif

#if MEM_TRACE
	if (final)
	{
		int	i;

		printf("\nUnfreed memory: %s\n\n", title);
		for (i = 0; i < MAX_TRACE; i++)
			if (TraceData[i].File)
				printf("Unfreed: %s line %d: Size: %d\n", 
					TraceData[i].File, TraceData[i].Line, TraceData[i].Size);
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

#define longSize (sizeof(long))

/* typical machine has pagesize 4096 (not critical anyway, just can help malloc) */
#define pageSize (1<<12)

/* round up to a long word */
#define ROUND_LONG(n) (((n) + longSize - 1) & (~(longSize - 1)))

/* round up to a page */
#define ROUND_PAGE(n) (((n) + pageSize - 1) & (~(pageSize - 1)))


/* allocate a chunk of memory from the OS */
static MEM_ZONE *allocChunk(size_t size)
{
	MEM_ZONE	*zone;

	zone = emalloc(sizeof(MEM_ZONE));
	zone->alloc = emalloc(size);
	zone->ptr = zone->alloc;
	zone->free = size;
	zone->next = NULL;

	return zone;
}

/* create a memory zone */
MEM_ZONE_HEAD *Mem_ZoneCreate(size_t size)
{
	MEM_ZONE_HEAD	*head;

	head = emalloc(sizeof(MEM_ZONE_HEAD));
	head->size = ROUND_PAGE(size);
	head->next = allocChunk(size);

	return head;
}

/* allocate memory from a zone (can use like malloc if you aren't going to realloc) */
void *Mem_ZoneAlloc(MEM_ZONE_HEAD *head, size_t size)
{
	MEM_ZONE	*zone;
	MEM_ZONE	*newzone;
	unsigned char *ptr;

	size = ROUND_LONG(size);

	zone = head->next;

	/* If not enough free in this chunk, allocate a new one. Don't worry about the
	   small amount of unused space at the end. If we are asking for a really big
	   chunk be sure we allocate enough for that! */

	if (zone->free < size)
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


void Mem_ZoneFree(MEM_ZONE_HEAD *head)
{
	MEM_ZONE *next;
	MEM_ZONE *tmp;

	if (!head)
		return;

	next = head->next;
	while (next)
	{
		efree(next->alloc);
		tmp = next->next;
		efree(next);
		next = tmp;
	}

	efree(head);
}

