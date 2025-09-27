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

typedef size_t (*GcObjectSize)(GcObject* object);
typedef void (*GcCopyObject)(
    Gc* gc,
    GcObject* object,
    GcObject* new_object
);
typedef GcObject* (*GcGetChildren)(GcObject* object, GcObject* position);
typedef void (*GcFinalizeObject)(Gc* gc, GcObject* object);

typedef struct {
    size_t align;
    GcObjectSize object_size;
    GcCopyObject copy_object;
    GcGetChildren get_children;
    GcFinalizeObject finalize_object;
} GcType;

typedef struct {
    GcObject* object;
    GcObject* weak_root;
} FinalizerData;

struct Gc {
    bool collecting;
    bool finalizing;

    Arena active;
    Arena inactive;

    GcObject*** roots;
    size_t root_count;
    size_t root_capacity;

    GcObject*** weak_roots;
    size_t weak_root_count;
    size_t weak_root_capacity;

    GcType* types;
    size_t type_count;
    size_t type_capacity;

    FinalizerData* finalizers;
    size_t finalizer_count;
    size_t finalizer_capacity;

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

size_t gc_add_type_with_finalizer(
    Gc* gc,
    size_t align,
    GcObjectSize object_size,
    GcCopyObject copy_object,
    GcGetChildren get_children,
    GcFinalizeObject finalize_object
);

/// Strong roots must not be added or removed during a garbage collection.
#define GC_ROOT(gc, root) gc_root((gc), (GcObject**) (root))
#define GC_UNROOT(gc, root) gc_unroot((gc), (GcObject**) (root))

#define GC_ROOT_WEAK(gc, root) gc_root_weak((gc), (GcObject**) (root))
#define GC_UNROOT_WEAK(gc, root) gc_unroot_weak((gc), (GcObject**) (root))

void gc_root(Gc* gc, GcObject** root);
void gc_unroot(Gc* gc, GcObject** root);

void gc_root_weak(Gc* gc, GcObject** weak_root);
void gc_unroot_weak(Gc* gc, GcObject** weak_root);

GcObject* gc_copy_object(Gc* gc, GcObject* object);

GcObject* gc_alloc(Gc* gc, size_t type_id, size_t size);
void* gc_alloc_untyped(Gc* gc, size_t size, size_t align);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList gc_list;

#endif

#endif
