#ifndef MEMUTILS_H
#define MEMUTILS_H

#define MAXIMUM_ALIGNOF 8
#define TYPEALIGN(ALIGNVAL,LEN)  \
	(((uintptr_t) (LEN) + ((ALIGNVAL) - 1)) & ~((uintptr_t) ((ALIGNVAL) - 1)))

#define MAXALIGN(LEN)	TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

typedef enum {
	mct_alloc		= 0x01,
	mct_context		= 0x02
} MemoryChunkType;

struct MemoryChunkData
{
	MemoryChunkType chunk_type;
	MemoryChunk		prev;
	MemoryChunk		next;
	MemoryContext	context;
};

#define MEMORY_CHUNK_SIZE (MAXALIGN(sizeof(struct MemoryChunkData)))
#define GetMemoryChunk(p) ((MemoryChunk)((char *)(p) - MEMORY_CHUNK_SIZE))
#define ChunkDataOffset(c) ((void *)((char *)(c) + MEMORY_CHUNK_SIZE))

#endif
