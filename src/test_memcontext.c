#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <cmocka.h>
#include <pthread.h>
#include <malloc.h>

#include "sleep_lock.h"
#include "memcontext.h"
#include "memutils.h"

static volatile int free_called = 0;
void __wrap_malloc_free(void *ptr)
{
	free_called += (int) mock();
	(void) ptr;
}

static void test_consistency(void **state) {
	MemoryContext	top = mcxt_new(NULL),
					old,
					child1 = mcxt_new(top),
					child2 = mcxt_new(top);

	assert_ptr_equal(current_mcxt, NULL);
	old = mcxt_switch_to(top);
	assert_ptr_equal(old, NULL);
	assert_ptr_equal(current_mcxt, top);

	/* top */
	assert_ptr_equal(top->firstchild, child1);
	assert_ptr_equal(top->nextchild, NULL);
	assert_ptr_equal(top->prevchild, NULL);

	/* child */
	assert_ptr_equal(child1->parent, top);
	assert_ptr_equal(child1->firstchild, NULL);
	assert_ptr_equal(child1->nextchild, child2);
	assert_ptr_equal(child1->prevchild, NULL);
	assert_ptr_equal(child1->lastchunk, NULL);

	/* child2 */
	assert_ptr_equal(child2->parent, top);
	assert_ptr_equal(child2->firstchild, NULL);
	assert_ptr_equal(child2->nextchild, NULL);
	assert_ptr_equal(child2->prevchild, child1);
	assert_ptr_equal(child2->lastchunk, NULL);

	assert_ptr_equal(top->lastchunk->context, top);
	assert_ptr_equal(top->lastchunk->prev->context, top);
	assert_ptr_equal(top->lastchunk->chunk_type, mct_context);
	assert_ptr_equal(top->lastchunk->prev->chunk_type, mct_context);
	assert_ptr_equal(top->lastchunk, GetMemoryChunk(child2));
	assert_ptr_equal(top->lastchunk->next, NULL);
	assert_ptr_equal(top->lastchunk->prev, GetMemoryChunk(child1));
	assert_ptr_equal(top->lastchunk->prev->next, GetMemoryChunk(child2));
	assert_ptr_equal(top->lastchunk->prev->prev, NULL);

	old = mcxt_switch_to(old);
	assert_ptr_equal(old, top);
	assert_ptr_equal(current_mcxt, NULL);

	(void) state;	/* keep compiler quiet */
}

static void test_allocation(void **state) {
	MemoryChunk		chunk,
					chunk2,
					chunk3;
	MemoryContext	top = mcxt_new(NULL);
	char		   *block3;

	chunk = GetMemoryChunk(mcxt_alloc_mem(top, 100, false));
	assert_true(chunk->chunk_type == mct_alloc);
	assert_true(chunk->context == top);
	assert_true(malloc_usable_size(chunk) >= MEMORY_CHUNK_SIZE + 100);
	assert_ptr_equal(top->lastchunk, chunk);

	chunk2 = GetMemoryChunk(mcxt_alloc_mem(top, 130, false));
	assert_ptr_equal(top->lastchunk, chunk2);
	assert_ptr_equal(chunk2->prev, chunk);
	assert_ptr_equal(chunk2->next, NULL);
	assert_true(malloc_usable_size(chunk2) >= MEMORY_CHUNK_SIZE + 130);

	assert_ptr_equal(chunk->next, chunk2);
	assert_ptr_equal(chunk->prev, NULL);

	block3 = mcxt_alloc_mem(top, 10, true);
	chunk3 = GetMemoryChunk(block3);
	assert_ptr_equal(top->lastchunk, chunk3);
	assert_ptr_equal(chunk3->prev, chunk2);
	assert_ptr_equal(chunk3->next, NULL);
	assert_ptr_equal(chunk2->next, chunk3);
	assert_ptr_equal(chunk2->prev, chunk);

	for (int i = 0; i < 10; i++)
		assert_true(block3[i] == '\0');

	will_return(malloc_free, 1);
	mcxt_reset(top, false);
	assert_ptr_equal(top->lastchunk, NULL);
	assert_int_equal(free_called, 3);

	(void) state;	/* keep compiler quiet */
}

static void test_helpers(void **state) {
	(void) state;	/* keep compiler quiet */
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_consistency),
        cmocka_unit_test(test_helpers),
        cmocka_unit_test(test_allocation)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
