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

/* Append chunk at the end of list of chunks */
static inline void
mcxt_append_chunk(MemoryContext context, MemoryChunk chunk)
{
	MemoryChunk		lastchunk;

	lastchunk = context->lastchunk;
	if (lastchunk)
		lastchunk->next = chunk;

	context->lastchunk = chunk;
	chunk->prev = lastchunk;
	chunk->next = NULL;
}

static void mcxt_link_context(MemoryContext parent, MemoryContext new)
{
	mm_sleeplock_lock(&parent->lock);
	if (parent->firstchild == NULL)
		parent->firstchild = new;
	else
	{
		MemoryContext	child;
		for (child = parent->firstchild; child->nextchild != NULL;
				child = child->nextchild);

		child->nextchild = new;
		new->prevchild = child;
	}
	mm_sleeplock_unlock(&parent->lock);
}

/* New memory context */
MemoryContext mcxt_new(MemoryContext parent)
{
	MemoryChunk chunk = calloc(MEMORY_CHUNK_SIZE +
		sizeof(struct MemoryContextData), 1);
	MemoryContext new = ChunkDataOffset(chunk);

	if (new == NULL)
		return NULL;

	new->parent = parent;
	new->ptid = pthread_self();
	chunk->chunk_type = mct_context;

	/* append to parent's children */
	if (parent)
		mcxt_link_context(parent, new);

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
	MemoryChunk		chunk = context->lastchunk;

	if (context->ptid != pthread_self())
		return MCXT_THREAD_CONFLICT;

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

	while (chunk != NULL)
	{
		MemoryChunk prev = chunk->prev;
		free(chunk);
		chunk = prev;
	}
	context->lastchunk = NULL;

	return res;
}

int mcxt_chunks_count(MemoryContext context)
{
	int			count = 0;
	MemoryChunk chunk = context->lastchunk;
	while (chunk != NULL)
	{
		count++;
		chunk = chunk->prev;
	}
	return count;
}

static void mcxt_unlink_context(MemoryContext context)
{
	if (context->parent == NULL)
	{
		assert(context->prevchild == NULL);
		assert(context->nextchild == NULL);
		return;
	}

	mm_sleeplock_lock(&context->parent->lock);

	if (context->prevchild)
		context->prevchild->nextchild = context->nextchild;
	else
		context->parent->firstchild = context->nextchild;

	if (context->nextchild)
		context->nextchild->prevchild = context->prevchild;

	mm_sleeplock_unlock(&context->parent->lock);
}

/* Delete memory context, all its chunks and childs */
int mcxt_delete(MemoryContext context)
{
	int				res = 0;
	MemoryContext	child;

	assert(current_mcxt != context);
	assert(GetMemoryChunk(context)->chunk_type == mct_context);

	if (context->ptid != pthread_self())
		return MCXT_THREAD_CONFLICT;

	mcxt_unlink_context(context);

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		int r = mcxt_delete(child);
		if (r != 0)
			res = r;
	}

	mcxt_reset(context, false);
	free(GetMemoryChunk(context));

	return res;
}

/* Allocate memory in specified memory context */
void *mcxt_alloc_mem(MemoryContext context, size_t size, bool zero)
{
	MemoryChunk		chunk;

	assert(context);
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
	assert(chunk->chunk_type == mct_alloc);

	/* first, deattach from chunks in context */
	if (chunk->next)
		chunk->next->prev = chunk->prev;
	if (chunk->prev)
		chunk->prev->next = chunk->next;
	if (chunk == context->lastchunk)
		context->lastchunk = chunk->prev;

	free(chunk);
}
