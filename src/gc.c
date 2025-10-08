#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "common.h"
#include "gc.h"
#include "util.h"

#define INITIAL_REGION_SIZE 4096

size_t gc_object_type(GcObject* object) {
    return object->flags;
}

bool gc_init(Gc* gc) {
    gc->collecting = false;
    if (!arena_init(&gc->active, INITIAL_REGION_SIZE)) {
        return false;
    }

    if (!arena_init(&gc->inactive, INITIAL_REGION_SIZE)) {
        arena_free(&gc->active);
        return false;
    }

    gc->roots = NULL;
    gc->root_count = 0;
    gc->root_capacity = 0;

    gc->types = NULL;
    gc->type_count = 0;
    gc->type_capacity = 0;

    return true;
}

void gc_free(Gc* gc) {
    gc->collecting = false;

    arena_free(&gc->active);
    arena_free(&gc->inactive);

    free(gc->roots);
    gc->roots = NULL;
    gc->root_count = 0;
    gc->root_capacity = 0;

    free(gc->types);
    gc->types = NULL;
    gc->type_count = 0;
    gc->type_capacity = 0;
}

size_t gc_add_type(
    Gc* gc,
    size_t align,
    GcObjectSize object_size,
    GcCopyObject copy_object,
    GcGetChildren get_children
) {
    size_t type_id = gc->type_count;
#ifdef DEBUG_LOG_GC
    printf("gc add type %zu\n", gc->type_count);
#endif
    if (gc->type_count >= gc->type_capacity) {
        if (!GROW(&gc->types, &gc->type_capacity, sizeof(GcType), 8)) {
            fprintf(stderr, "growing type array failed\n");
            exit(EXIT_FAILURE);
        }
    }

    ASSERT(align >= alignof(GcObject));
    gc->types[type_id].align = align;
    gc->types[type_id].object_size = object_size;
    gc->types[type_id].copy_object = copy_object;
    gc->types[type_id].get_children = get_children;

    gc->type_count += 1;
    return type_id;
}

#ifdef DEBUG_LOG_GC
void gc_root(Gc* gc, GcObject** root, const char* file, size_t line) {
    printf("gc root %p pointed at %p at %s:%zu\n", root, *root, file, line);
#else
void gc_root(Gc* gc, GcObject** root) {
#endif
    ASSERT(gc->collecting == false);
    if (gc->root_count >= gc->root_capacity) {
        if (!GROW(&gc->roots, &gc->root_capacity, sizeof(GcObject**), 128)) {
            fprintf(stderr, "growing root array failed\n");
            exit(EXIT_FAILURE);
        }
    }

    gc->roots[gc->root_count] = root;
    gc->root_count += 1;
}

#ifdef DEBUG_LOG_GC
void gc_unroot(Gc* gc, GcObject** root, const char* file, size_t line) {
    printf("gc unroot %p pointed at %p at %s:%zu\n", root, *root, file, line);
#else
void gc_unroot(Gc* gc, GcObject** root) {
#endif
    ASSERT(gc->collecting == false);
    size_t index = gc->root_count;
loop:
    ASSERT(index != 0, "gc_unroot must be called with a rooted object");
    index -= 1;

    if (gc->roots[index] == root) {
        gc->roots[index] = gc->roots[gc->root_count - 1];
        gc->root_count -= 1;
        return;
    }
    goto loop;
}

static void gc_arena_resize(Arena* arena) {
    size_t capacity = arena_capacity(arena);
    if (!grow_capacity(&capacity, sizeof(uint8_t), INITIAL_REGION_SIZE)) {
        goto handle_err;
    }

    arena_free(arena);
    if (arena_init(arena, capacity)) return;

    // Resize failed.
handle_err:
    fprintf(stderr, "resize of gc arena failed\n");
    exit(EXIT_FAILURE);
}

static void gc_clear_forwarding_object(Gc* gc, GcObject* object) {
    if (object->forward_ptr == NULL) return;
    object->forward_ptr = NULL;
#ifdef DEBUG_LOG_GC
    printf("gc clearing forward ptr %p\n", object);
#endif

    size_t type_id = gc_object_type(object);
    GcType type = gc->types[type_id];

    GcObject* position = NULL;
    while ((position = type.get_children(object, position)) != NULL) {
        gc_clear_forwarding_object(gc, position);
    }
}

static void gc_clear_forwarding(Gc* gc) {
    for (size_t i = 0; i < gc->root_count; i++) {
        if (*gc->roots[i] == NULL) continue;
        gc_clear_forwarding_object(gc, *gc->roots[i]);
    }
}

void validate_address(Gc* gc, void* address) {
    uintptr_t base = (uintptr_t) gc->active.base;
    uintptr_t end = (uintptr_t) gc->active.end;
    uintptr_t addr = (uintptr_t) address;

    ASSERT(base <= addr && addr <= end, "%p", address);
}

void gc_collect(Gc* gc) {
#ifdef DEBUG_LOG_GC
    printf("gc collect begin\n");
    printf("----------------------------\n");
#endif
    gc->collecting = true;

    // Save position in case we run out of memory.
reset_mark:
    if (setjmp(gc->collect_mark) != 0) {
        gc_arena_resize(&gc->inactive);

        gc_clear_forwarding(gc);
        goto reset_mark;
    }

    // Create a copy of each of the roots.
    //
    // We can't actually update the roots yet, since an allocation could fail.
    for (size_t index = 0; index < gc->root_count; index++) {
        GcObject** root = gc->roots[index];

        if (*root == NULL) continue;
#ifdef DEBUG_LOG_GC
        printf("gc copy root %p pointed at %p\n", root, *root);
#endif
        validate_address(gc, *root);
        gc_copy_object(gc, *root);
    }
    Arena swap = gc->active;
    gc->active = gc->inactive;
    gc->inactive = swap;

    // Assign the results of the copy to the roots.
    for (size_t index = 0; index < gc->root_count; index++) {
        GcObject** root = gc->roots[index];

        // Support NULL roots to make preparation easier.
        if (*root == NULL) continue;

        // If the forward pointer is NULL, there are multiple rooting of this
        // location and we've already updated the root.
        if ((*root)->forward_ptr == NULL) continue;
        *root = (*root)->forward_ptr;
        validate_address(gc, *root);
    }

    // The garbage collection has completed successfully.
    // Swap the arenas to prepare for additional allocation.

    // Reset the old arena
    arena_reset(&gc->inactive);
    gc->collecting = false;
#ifdef DEBUG_LOG_GC
    printf("----------------------------\n");
    printf("gc collect end\n");
#endif
}

#include "sexpr.h"

GcObject* gc_copy_object(Gc* gc, GcObject* object) {
    if (object->forward_ptr != NULL) {
        return object->forward_ptr;
    }

    size_t type_id = gc_object_type(object);
    GcType type = gc->types[type_id];
    GcObject* new_object = gc_alloc(gc, type_id, type.object_size(object));

    object->forward_ptr = new_object;
    type.copy_object(gc, object, new_object);

    return object->forward_ptr;
}

GcObject* gc_alloc(Gc* gc, size_t type_id, size_t size) {
#ifdef DEBUG_LOG_GC
    printf("gc alloc type %zu\n", type_id);
#endif
    ASSERT(type_id < gc->type_count);
    GcType type = gc->types[type_id];
    GcObject* object = (GcObject*) gc_alloc_untyped(gc, size, type.align);

    object->flags = type_id;
    object->forward_ptr = NULL;
    return object;
}

void* gc_alloc_untyped(Gc* gc, size_t size, size_t align) {
#ifdef DEBUG_LOG_GC
    printf("gc alloc %zu bytes with %zu align\n", size, align);
#endif

#ifdef DEBUG_STRESS_GC
    if (!gc->collecting) gc_collect(gc);
#endif

    Arena* arena = gc->collecting ? &gc->inactive : &gc->active;

    void* ptr = (void*) arena_alloc(arena, size, align, 1);
    if (ptr != NULL) goto handle_success;

    // Initial allocation attempt failed.
    //
    // If a collection is happening, we reset to the start of the GC process.
    if (gc->collecting) longjmp(gc->collect_mark, 1);

    gc_collect(gc);
    while ((ptr = arena_alloc(arena, size, align, 1)) == NULL) {
        Arena* growth_arena = gc->collecting ? &gc->active : &gc->inactive;
        gc_arena_resize(growth_arena);

        gc_collect(gc);
    }

handle_success:
    return ptr;
}

#ifdef ENABLE_TESTS

#include "test.h"

typedef struct {
    GcObject object;
    size_t len;
    size_t val;
} GcArray;

size_t gc_array_size(GcObject* object) {
    return sizeof(GcArray) + ((GcArray*) object)->len;
}

void gc_array_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    ((GcArray*) new_object)->len = ((GcArray*) object)->len;
    ((GcArray*) new_object)->val = ((GcArray*) object)->val;
}

GcObject* gc_array_get_children(GcObject* object, GcObject* position) {
    return NULL;
}

bool gc_handle_failed_alloc_during_collect() {
    Gc gc;
    if (!gc_init(&gc)) {
        return false;
    }

    bool result = false;

    size_t low_align = gc_add_type(
        &gc,
        alignof(GcArray),
        gc_array_size,
        gc_array_copy,
        gc_array_get_children
    );

    size_t align =
        INITIAL_REGION_SIZE > alignof(GcArray)
        ? INITIAL_REGION_SIZE
        : alignof(GcArray);
    size_t high_align = gc_add_type(
        &gc,
        align,
        gc_array_size,
        gc_array_copy,
        gc_array_get_children
    );

    GcObject* low[8];
    GcObject* high[8];

    for (size_t i = 0; i < countof(high); i++) {
        high[i] = gc_alloc(&gc, high_align, sizeof(GcArray));
        ((GcArray*) high[i])->len = 0;
        ((GcArray*) high[i])->val = i;
        GC_ROOT(&gc, &high[i]);
    }

    for (size_t i = 0; i < countof(low); i++) {
        low[i] = gc_alloc(&gc, low_align, sizeof(GcArray));
        ((GcArray*) low[i])->len = 0;
        ((GcArray*) low[i])->val = i;
        GC_ROOT(&gc, &low[i]);
    }

    for (size_t i = 0; i < countof(high); i++) {
        GC_UNROOT(&gc, &low[i]);
        GC_UNROOT(&gc, &high[i]);
    }

    for (size_t i = 0; i < countof(high); i++) {
        GC_ROOT(&gc, &low[i]);
        GC_ROOT(&gc, &high[i]);
    }

    for (size_t i = 0; i < countof(high); i++) {
        if (((GcArray*) low[i])->val != i) goto cleanup;
        if (((GcArray*) high[i])->val != i) goto cleanup;
    }

    gc_collect(&gc);
    result = true;
cleanup:
    gc_free(&gc);
    return result;
}

bool gc_support_redundant_rooting() {
    Gc gc;
    if (!gc_init(&gc)) {
        return false;
    }

    bool result = false;

    size_t type_id = gc_add_type(
        &gc,
        alignof(GcArray),
        gc_array_size,
        gc_array_copy,
        gc_array_get_children
    );

    GcArray* obj = (GcArray*) gc_alloc(&gc, type_id, sizeof(GcArray));
    obj->len = 0;
    obj->val = 0xD;

    GC_ROOT(&gc, &obj);
    GC_ROOT(&gc, &obj);
    GC_ROOT(&gc, &obj);

    gc_collect(&gc);
    if (obj->val != 0xD) goto cleanup;

    result = true;
cleanup:
    gc_free(&gc);
    return result;
}

TestDefinition gc_tests[] = {
    DEFINE_UNIT_TEST(gc_handle_failed_alloc_during_collect, 0),
    DEFINE_UNIT_TEST(gc_support_redundant_rooting, 0),
};

TestList gc_test_list = (TestList) {
    gc_tests,
    countof(gc_tests)
};

#endif
