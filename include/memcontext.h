#ifndef MEMCONTEXT_H
#define MEMCONTEXT_H

typedef struct MemoryChunkData		*MemoryChunk;
typedef struct MemoryContextData	*MemoryContext;

enum {
	MCXT_THREAD_CONFLICT = 0x01
} mcxt_errors;

struct MemoryContextData
{
	mm_sleeplock_t	lock;
	pthread_t		ptid;
	MemoryContext	parent;
	MemoryContext	firstchild;
	MemoryContext	prevchild;
	MemoryContext	nextchild;

	MemoryChunk		lastchunk;
};

extern __thread MemoryContext current_mcxt;

MemoryContext mcxt_new(MemoryContext parent);
MemoryContext mcxt_switch_to(MemoryContext to);
int mcxt_reset(MemoryContext context, bool recursive);
int mcxt_delete(MemoryContext context);
void *mcxt_alloc_mem(MemoryContext context, size_t size, bool zero);
void mcxt_free_mem(MemoryContext context, void *p);
int mcxt_chunks_count(MemoryContext context);

static inline void *mcxt_alloc(size_t size)
{
	assert(current_mcxt != NULL);
	return mcxt_alloc_mem(current_mcxt, size, false);
}

static inline void *mcxt_alloc0(size_t size)
{
	assert(current_mcxt != NULL);
	return mcxt_alloc_mem(current_mcxt, size, true);
}

static inline void mcxt_free(void *p)
{
	MemoryContext context = *((MemoryContext *) ((char *) p - sizeof(void *)));
	mcxt_free_mem(context, p);
}

#endif
