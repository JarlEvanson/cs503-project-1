#ifndef LISP_GC_H
#define LISP_GC_H

#include <setjmp.h>

#include "common.h"
#include "arena.h"

typedef struct Gc Gc;

typedef struct GcObject GcObject;
struct GcObject {
    size_t flags;
    GcObject* forward_ptr;
};

size_t gc_object_type(GcObject* object);

typedef size_t (*GcObjectSize)(GcObject* object);
typedef void (*GcCopyObject)(
    Gc* gc,
    GcObject* object,
    GcObject* new_object
);
typedef GcObject* (*GcGetChildren)(GcObject* object, GcObject* position);

typedef struct {
    size_t align;
    GcObjectSize object_size;
    GcCopyObject copy_object;
    GcGetChildren get_children;
} GcType;

struct Gc {
    bool collecting;

    Arena active;
    Arena inactive;

    GcObject*** roots;
    size_t root_count;
    size_t root_capacity;

    GcType* types;
    size_t type_count;
    size_t type_capacity;

    jmp_buf collect_mark;
};

bool gc_init(Gc* gc);
void gc_free(Gc* gc);

size_t gc_add_type(
    Gc* gc,
    size_t align,
    GcObjectSize object_size,
    GcCopyObject copy_object,
    GcGetChildren get_children
);

/// Strong roots must not be added or removed during a garbage collection.

#ifdef DEBUG_LOG_GC

#define GC_ROOT(gc, root) gc_root((gc), (GcObject**) (root), __FILE__, __LINE__)
#define GC_UNROOT(gc, root) gc_unroot((gc), (GcObject**) (root), __FILE__, __LINE__)

void gc_root(Gc* gc, GcObject** root, const char* file, size_t line);
void gc_unroot(Gc* gc, GcObject** root, const char* file, size_t line);

#else

#define GC_ROOT(gc, root) gc_root((gc), (GcObject**) (root))
#define GC_UNROOT(gc, root) gc_unroot((gc), (GcObject**) (root))

void gc_root(Gc* gc, GcObject** root);
void gc_unroot(Gc* gc, GcObject** root);

#endif

void gc_collect(Gc* gc);

GcObject* gc_copy_object(Gc* gc, GcObject* object);

GcObject* gc_alloc(Gc* gc, size_t type_id, size_t size);
void* gc_alloc_untyped(Gc* gc, size_t size, size_t align);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList gc_test_list;

#endif

#endif
