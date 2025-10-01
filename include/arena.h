#ifndef LISP_ARENA_H
#define LISP_ARENA_H

#include "common.h"

typedef struct {
    uint8_t* base;
    uint8_t* next;
    uint8_t* end;
} Arena;

bool arena_init(Arena* arena, size_t capacity);
void arena_free(Arena* arena);

size_t arena_capacity(Arena* arena);

uint8_t* arena_alloc(Arena* arena, size_t size, size_t align, size_t count);
void arena_reset(Arena* arena);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList arena_test_list;

#endif

#endif
