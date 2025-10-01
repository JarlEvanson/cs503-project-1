#include <stdlib.h>

#include "common.h"
#include "arena.h"

#ifdef ENABLE_VALGRIND_SUPPORT

#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#endif

bool arena_init(Arena* arena, size_t capacity) {
    if (capacity == 0) {
        arena->base = NULL;
        arena->next = NULL;
        arena->end = NULL;
        return true;
    }

    arena->base = malloc(capacity);
    if (arena->base == NULL) {
        return false;
    }

    arena->next = arena->base;
    arena->end = arena->base + capacity;

#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_CREATE_MEMPOOL(arena->base, 0, false);
#endif

#ifdef DEBUG_CLEAR_ARENA

#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MAKE_MEM_UNDEFINED(arena->base, capacity);
#endif

    while (arena->next != arena->end) {
        *arena->next = 0;
        arena->next += 1;
    }

    arena->next = arena->base;
#endif

#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MAKE_MEM_NOACCESS(arena->base, capacity);
#endif
    return true;
}

void arena_free(Arena* arena) {
#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_DESTROY_MEMPOOL(arena->base);
#endif
    free(arena->base);

    arena->base = NULL;
    arena->next = NULL;
    arena->end = NULL;
}

size_t arena_capacity(Arena* arena) {
    return arena->end - arena->base;
}

uint8_t* arena_alloc(Arena* arena, size_t size, size_t align, size_t count) {
    size_t padding = (-(uintptr_t) arena->next) & (align - 1);
    size_t remaining = arena->end - arena->next;
    if (remaining < padding) {
        return NULL;
    }

    size_t available = (arena->end - arena->next) - padding;
    if (count > available / size) {
        return NULL;
    }

    uint8_t* ptr = arena->next + padding;
    arena->next += padding + count * size;
#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MEMPOOL_ALLOC(arena->base, ptr, count * size);
#endif
    return ptr;
}

void arena_reset(Arena* arena) {
#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MEMPOOL_TRIM(arena->base, arena->base, 0);
#endif

    arena->next = arena->base;

#ifdef DEBUG_CLEAR_ARENA

#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MAKE_MEM_UNDEFINED(arena->base, arena->end - arena->base);
#endif

    while (arena->next != arena->end) {
        *arena->next = 0;
        arena->next += 1;
    }

#ifdef ENABLE_VALGRIND_SUPPORT
    VALGRIND_MAKE_MEM_NOACCESS(arena->base, arena->end - arena->base);
#endif

    arena->next = arena->base;
#endif
}

#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

static bool arena_alloc_from_unaligned() {
    Arena arena;
    arena_init(&arena, 1024);

    size_t target_alignment = 128;

    uint8_t* ptr;
    do {
        ptr = arena_alloc(&arena, 1, 1, 1);
        if ((((uintptr_t) ptr) & (target_alignment - 1)) != 0) {
            break;
        }
    } while (1);

    ptr = arena_alloc(&arena, 1, target_alignment, 1);

    bool result = (((uintptr_t) ptr) & (target_alignment - 1)) == 0;

    arena_free(&arena);
    return result;
}

static bool arena_alloc_can_fill() {
    Arena arena;
    arena_init(&arena, 1024);

    bool result = false;

    uint8_t* ptr;
    for (size_t i = 0; i < 1024; i++) {
        ptr = arena_alloc(&arena, 1, 1, 1);
        if (ptr == NULL) goto cleanup;

        ptr[0] = i;
    }

    result = true;
cleanup:
    arena_free(&arena);
    return result;
}

static TestDefinition arena_tests[] = {
    DEFINE_UNIT_TEST(arena_alloc_from_unaligned, 0),
    DEFINE_UNIT_TEST(arena_alloc_can_fill, 0),
};

TestList arena_test_list = (TestList) {
    arena_tests,
    countof(arena_tests)
};

#endif
