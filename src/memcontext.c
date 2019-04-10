#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "sleep_lock.h"
#include "memcontext.h"
#include "memutils.h"

__thread MemoryContext current_mcxt = NULL;

static inline void
malloc_free(void *ptr)
{
	free(ptr);
}

/* Append chunk at the end of list of chunks */
static inline void
mcxt_append_chunk(MemoryContext context, MemoryChunk chunk)
{
	MemoryChunk		lastchunk;

	mm_sleeplock_lock(&context->lock);
	lastchunk = context->lastchunk;
	if (lastchunk)
		lastchunk->next = chunk;

	context->lastchunk = chunk;
	chunk->prev = lastchunk;
	chunk->next = NULL;
	mm_sleeplock_unlock(&context->lock);
}

/* New memory context */
MemoryContext mcxt_new(MemoryContext parent)
{
	MemoryContext	new = mcxt_alloc_mem(parent,
		sizeof(struct MemoryContextData), true);
	if (new == NULL)
		return NULL;

	new->parent = parent;
	new->ptid = pthread_self();
	GetMemoryChunk(new)->chunk_type = mct_context;

	/* append to children */
	if (parent)
	{
		mm_sleeplock_lock(&parent->lock);
		if (parent->firstchild == NULL)
			parent->firstchild = new;
		else
		{
			MemoryContext	child = parent->firstchild;
			for (child = parent->firstchild; child->nextchild != NULL;
					child = child->nextchild);

			child->nextchild = new;
			new->prevchild = child;
		}
		mm_sleeplock_unlock(&parent->lock);
	}

	return new;
}

/* Change current memory context */
MemoryContext mcxt_switch_to(MemoryContext to)
{
	MemoryContext	old = current_mcxt;
	current_mcxt = to;
	return old;
}

/* Free all memory in memory context */
int mcxt_reset(MemoryContext context, bool recursive)
{
	int				res = 0;
	MemoryChunk		chunk = context->lastchunk,
					prev;

	if (context->ptid != pthread_self())
		return MCXT_THREAD_CONFLICT;

	/* we make it look like free */
	mm_sleeplock_lock(&context->lock);
	chunk = context->lastchunk;
	context->lastchunk = NULL;
	mm_sleeplock_unlock(&context->lock);

	if (recursive)
	{
		/* reset children if recursive */
		MemoryContext	child;
		for (child = context->firstchild; child != NULL; child = child->nextchild)
		{
			int r = mcxt_reset(child, true);
			if (r != 0)
				res = r;
		}
	}

	do
	{
		prev = chunk->prev;
		if (chunk->chunk_type == mct_context)
			mcxt_append_chunk(context, chunk);
		else
			malloc_free(chunk);
	}
	while ((chunk = prev) != NULL);

	return res;
}

/* Delete memory context, all its chunks and childs */
int mcxt_delete(MemoryContext context)
{
	int				res = 0;
	MemoryContext	child;

	assert(current_mcxt != context);

	if (context->ptid != pthread_self())
		return MCXT_THREAD_CONFLICT;

	if (context->prevchild)
		context->prevchild->nextchild = context->nextchild;
	else if (context->parent)
		context->parent->firstchild = context->nextchild;

	if (context->nextchild)
		context->nextchild->prevchild = context->prevchild;

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		int r = mcxt_delete(child);
		if (r != 0)
			res = r;
	}

	mcxt_reset(context, true);
	mcxt_free_mem(context->parent, (void *) context);

	return res;
}

/* Allocate memory in specified memory context */
void *mcxt_alloc_mem(MemoryContext context, size_t size, bool zero)
{
	MemoryChunk		chunk;

	if (context == NULL)
		return zero ? calloc(size, 1) : malloc(size);

	assert(context->ptid == pthread_self());

	chunk = malloc(MEMORY_CHUNK_SIZE + size);
	if (chunk == NULL)
		return NULL;

	if (zero)
		memset(chunk, 0, MEMORY_CHUNK_SIZE);

	chunk->context = context;
	chunk->chunk_type = mct_alloc;

	mcxt_append_chunk(context, chunk);
	return ChunkDataOffset(chunk);
}

/* Free memory in specified memory context */
void mcxt_free_mem(MemoryContext context, void *p)
{
	MemoryChunk chunk = GetMemoryChunk(p);

	assert(p != NULL);
	assert(p == (void *) MAXALIGN(p));
	assert(chunk->chunk_type > 0);

	/* first, deattach from chunks in context */
	if (chunk->next)
		chunk->next->prev = chunk->prev;

	if (chunk->prev)
		chunk->prev->next = chunk->next;

	if (context && chunk == context->lastchunk)
	{
		mm_sleeplock_lock(&context->lock);
		context->lastchunk = chunk->prev;
		mm_sleeplock_unlock(&context->lock);
	}

	malloc_free(chunk);
}
