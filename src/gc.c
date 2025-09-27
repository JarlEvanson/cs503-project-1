#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "common.h"
#include "gc.h"
#include "util.h"

#define INITIAL_REGION_SIZE 4096

bool gc_init(Gc* gc) {
    gc->collecting = false;
    gc->finalizing = false;
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
    
    gc->weak_roots = NULL;
    gc->weak_root_count = 0;
    gc->weak_root_capacity = 0;

    gc->types = NULL;
    gc->type_count = 0;
    gc->type_capacity = 0;

    gc->finalizers = NULL;
    gc->finalizer_count = 0;
    gc->finalizer_capacity = 0;

    return true;
}

void gc_free(Gc* gc) {
    gc->collecting = false;
    gc->finalizing = false;
    
    arena_free(&gc->active);
    arena_free(&gc->inactive);

    free(gc->roots);
    gc->roots = NULL;
    gc->root_count = 0;
    gc->root_capacity = 0;

    free(gc->weak_roots);
    gc->weak_roots = NULL;
    gc->weak_root_count = 0;
    gc->weak_root_capacity = 0;

    free(gc->types);
    gc->types = NULL;
    gc->type_count = 0;
    gc->type_capacity = 0;

    free(gc->finalizers);
    gc->finalizers = NULL;
    gc->finalizer_count = 0;
    gc->finalizer_capacity = 0;
}

size_t gc_add_type(
    Gc* gc,
    size_t align,
    GcObjectSize object_size,
    GcCopyObject copy_object,
    GcGetChildren get_children
) {
    return gc_add_type_with_finalizer(
        gc,
        align,
        object_size,
        copy_object,
        get_children,
        NULL
    );
}

size_t gc_add_type_with_finalizer(
    Gc* gc,
    size_t align,
    GcObjectSize object_size,
    GcCopyObject copy_object,
    GcGetChildren get_children,
    GcFinalizeObject finalize_object
) {
    size_t type_id = gc->type_count++;
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
    gc->types[type_id].finalize_object = finalize_object;

    return type_id;
}

void gc_root(Gc* gc, GcObject** root) {
#ifdef DEBUG_LOG_GC
    printf("gc root %p\n", root);
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

void gc_unroot(Gc* gc, GcObject** root) {
#ifdef DEBUG_LOG_GC
    printf("gc unroot %p\n", root);
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

void gc_root_weak(Gc* gc, GcObject** weak_root) {
#ifdef DEBUG_LOG_GC
    printf("gc root weak %p\n", weak_root);
#endif
    if (gc->weak_root_count >= gc->weak_root_capacity) {
        if (!GROW(
            &gc->weak_roots,
            &gc->weak_root_capacity,
            sizeof(GcObject**),
            128
        )) {
            fprintf(stderr, "growing root array failed\n");
            exit(EXIT_FAILURE);
        }
    }

    gc->weak_roots[gc->weak_root_count] = weak_root;
    gc->weak_root_count += 1;
}

void gc_unroot_weak(Gc* gc, GcObject** weak_root) {
#ifdef DEBUG_LOG_GC
    printf("gc unroot weak %p\n", weak_root);
#endif
    ASSERT(gc->collecting == false);
    size_t index = gc->weak_root_count;
loop:
    ASSERT(index != 0, "gc_unroot_weak must be called with a rooted object");
    index -= 1;

    if (gc->weak_roots[index] == weak_root) {
        gc->weak_roots[index] = gc->weak_roots[gc->weak_root_count - 1];
        gc->weak_root_count -= 1;
        return;
    }
    goto loop;
}

void gc_handle_finalizers(Gc* gc) {
#ifdef DEBUG_LOG_GC
    printf("gc handle finalizers begin\n");
    printf("----------------------------\n");
#endif
    gc->finalizing = true;

    // At this point, finalizers can be in one of two states.
    //
    // 1. data->object != NULL && data->weak_root != NULL
    // 2. data->object != NULL && data->weak_root == NULL
    //
    // For case 1, the object is still fully alive.
    // For case 2, we need to run the finalizer.
    for (size_t index = 0; index < gc->finalizer_count; index++) {
        if (
            gc->finalizers[index].object != NULL
            && gc->finalizers[index].weak_root != NULL
        ) {
            continue;
        }

        GcObject* object = gc->finalizers[index].object;
        
        GC_ROOT(gc, &object);
        gc->types[object->flags].finalize_object(gc, object);
        GC_UNROOT(gc, &object);

        for (size_t s_index = 0; s_index < gc->finalizer_count; s_index++) {
            if (gc->finalizers[s_index].object == object) {
                gc->finalizers[s_index].object = NULL;
                gc->finalizers[s_index].weak_root = object;
                GC_ROOT_WEAK(gc, &gc->finalizers[s_index].weak_root);
                goto cont;
            }
        }

        UNREACHABLE();
cont:
    }

    gc->finalizing = false;
#ifdef DEBUG_LOG_GC
    printf("----------------------------\n");
    printf("gc handle finalizers end\n");
#endif
}

void gc_clear_forwarding_object(GcObject* object) {
    ASSERT(false);
}

void gc_clear_forwarding(Gc* gc) {
    for (size_t i = 0; i < gc->root_count; i++) {
        if (*gc->roots[i] == NULL) continue;
        gc_clear_forwarding_object(*gc->roots[i]);
    }

    for (size_t i = 0; i < gc->finalizer_count; i++) {
        FinalizerData* data = &gc->finalizers[i];

        if (data->object != NULL) {
            gc_clear_forwarding_object(data->object);
        }
    }
}

void gc_collect(Gc* gc) {
#ifdef DEBUG_LOG_GC
    printf("gc collect begin\n");
    printf("----------------------------\n");
#endif
    gc->collecting = true;

    if (arena_capacity(&gc->inactive) != arena_capacity(&gc->active)) {
        arena_free(&gc->inactive);
        if (!arena_init(&gc->inactive, arena_capacity(&gc->active))) {
            fprintf(stderr, "growing gc arena failed\n");
            exit(EXIT_FAILURE);
        }
    }

    // Swap arenas
    Arena swap = gc->active;
    gc->active = gc->inactive;
    gc->inactive = swap;

save_retry:
    if (setjmp(gc->collect_mark) != 0) {
        // An error occurred when copying objects.
        gc_clear_forwarding(gc);

        size_t capacity = arena_capacity(&gc->active);
        if (!grow_capacity(&capacity, sizeof(uint8_t), 4096)) {
            fprintf(stderr, "growing gc array failed\n");
            exit(EXIT_FAILURE);
        }

        arena_free(&gc->active);
        if (!arena_init(&gc->active, capacity)) {
            fprintf(stderr, "growing gc array failed\n");
            exit(EXIT_FAILURE);
        }

        goto save_retry;
    }

    // Approach
    //
    // 1. Copy roots
    // 2. Drop weak roots
    // 3. Copy finalizers
    //
    // 4. Set roots
    // 5. Set weak roots
    // 6. Set finalizers
    //
    // If an allocation fails, clear forwarding pointers

    // Step 1
    size_t index;
    for (index = 0; index < gc->root_count; index++) {
        GcObject** root = gc->roots[index];
        
        // Skip any roots that are NULL.
        if (*root == NULL) continue;
#ifdef DEBUG_LOG_GC
        printf("gc copy root %p\n", root);
#endif
        gc_copy_object(gc, *root);
    }

    // Step 2
    index = 0;
    while (index < gc->weak_root_count) {
        GcObject** weak_root = gc->weak_roots[index];

        // Skip any roots that are NULL.
        if (*weak_root == NULL) continue;
        if ((*weak_root)->forward_ptr == NULL) {
#ifdef DEBUG_LOG_GC
            printf("gc drop weak root %p\n", weak_root);
#endif
            *weak_root = NULL;
            gc->weak_roots[index] = gc->weak_roots[gc->weak_root_count - 1];
            gc->weak_root_count -= 1;
        } else {
            index += 1;
        }
    }

    // Step 3
    for (index = 0; index < gc->finalizer_count; index++) {
        FinalizerData* data = &gc->finalizers[index];

        if (data->object != NULL) {
#ifdef DEBUG_LOG_GC
            printf("gc finalizer copy %p\n", data->object);
#endif
            data->object = gc_copy_object(gc, data->object);
        }
    }

    // Step 4
    for (index = 0; index < gc->root_count; index++) {
        GcObject** root = gc->roots[index];

        if (*root == NULL) continue;
        *root = gc_copy_object(gc, *root);
    }

    // Step 5
    for (index = 0; index < gc->weak_root_count; index++) {
        GcObject** weak_root = gc->weak_roots[index];

        // Skip any roots that are NULL.
        if (*weak_root == NULL) continue;
#ifdef DEBUG_LOG_GC
        printf("gc maintain weak root %p\n", weak_root);
#endif
        *weak_root = (*weak_root)->forward_ptr;
    }

    // Step 6
    index = 0;
    while (index < gc->finalizer_count) {
        FinalizerData* data = &gc->finalizers[index];

        if (data->object != NULL) {
            data->object = gc_copy_object(gc, data->object);
        } else if (data->weak_root != NULL) {
#ifdef DEBUG_LOG_GC
            printf("gc finalizer escaped %p\n", data->weak_root);
#endif
            data->object = data->weak_root;
        } else {
#ifdef DEBUG_LOG_GC
            printf("gc finalizer finished\n");
#endif
            gc->finalizers[index] = gc->finalizers[gc->finalizer_count - 1];
            gc->finalizer_count -= 1;
        }
    }

    /*
    size_t index;
    for (index = 0; index < gc->root_count; index++) {
        GcObject** root = gc->roots[index];
        
        // Skip any roots that are NULL.
        if (*root == NULL) continue;
#ifdef DEBUG_LOG_GC
        printf("gc copy root %p\n", root);
#endif
        gc_copy_object(gc, *root);
    }

    index = 0;
    while (index < gc->weak_root_count) {
        GcObject** weak_root = gc->weak_roots[index];

        // Skip any roots that are NULL.
        if (*weak_root == NULL) continue;
        if ((*weak_root)->forward_ptr != NULL) {
#ifdef DEBUG_LOG_GC
            printf("gc maintain weak root %p\n", weak_root);
#endif
            *weak_root = (*weak_root)->forward_ptr;
            index += 1;
        } else {
#ifdef DEBUG_LOG_GC
            printf("gc drop weak root %p\n", weak_root);
#endif
            *weak_root = NULL;
            gc->weak_roots[index] = gc->weak_roots[gc->weak_root_count - 1];
            gc->weak_root_count -= 1;
        }
    }

    // At this point, finalizers can be in one of four states.
    //
    // 1. data->object != NULL && data->weak_root != NULL
    //      This means that the object has outside roots and did not have to
    //      survive a finalization during this collection.
    // 2. data->object != NULL && data->weak_root == NULL
    //      This means that the object has no outside roots and thus should be
    //      finalized.
    // 3. data->object == NULL && data->weak_root != NULL
    //      This means that the object experienced a finalization between this
    //      collection and the last and survived it.
    // 4. data->object == NULL && data->weak_root == NULL
    //      This means that the object experienced a finalization between this
    //      collection and the last and did not survive it.
    //
    // For cases 1 and 2, we need to keep the object alive (run gc_copy_object).
    // For case 3, we need to reinitialize data->object to reset its state.
    // For case 4, we simply remove it from the finalizer list.
    for (index = 0; index < gc->finalizer_count; index++) {
        FinalizerData* data = &gc->finalizers[index];

        if (data->object != NULL) {
            // Handles cases 1 and 2.
            //
            // data->object != NULL is shared only by cases 1 and 2.
#ifdef DEBUG_LOG_GC
            printf("gc finalizer copy %p\n", data->object);
#endif
            data->object = gc_copy_object(gc, data->object);
        } else if (data->weak_root != NULL) {
            // Handles case 3.
#ifdef DEBUG_LOG_GC
            printf("gc finalizer escaped %p\n", data->weak_root);
#endif
            data->object = data->weak_root;
        } else {
            // Handles case 4.
#ifdef DEBUG_LOG_GC
            printf("gc finalizer finished\n");
#endif
            gc->finalizers[index] = gc->finalizers[gc->finalizer_count - 1];
            gc->finalizer_count -= 1;
        }
    }
    */

    arena_reset(&gc->inactive);
    gc->collecting = false;
#ifdef DEBUG_LOG_GC
    printf("----------------------------\n");
    printf("gc collect end\n");
#endif

    // Guard to ensure that only one finalization can happen at once.
    if (!gc->finalizing) gc_handle_finalizers(gc);
}

GcObject* gc_copy_object(Gc* gc, GcObject* object) {
    if (object->forward_ptr != NULL) {
        return object->forward_ptr;
    }

    size_t type_id = object->flags;
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

    GcType type = gc->types[type_id];
    size_t align = type.align;
    GcObject* object = (GcObject*) gc_alloc_untyped(gc, size, align);
    if (!gc->collecting && type.finalize_object != NULL) {
        // Non-copying allocation (add finalizer).
        if (gc->finalizer_count >= gc->finalizer_capacity) {
            if (!GROW(
                &gc->finalizers,
                &gc->finalizer_capacity,
                sizeof(GcObject**),
                128
            )) {
                fprintf(stderr, "growing finalizer array failed\n");
                exit(EXIT_FAILURE);
            }
        }

        gc->finalizers[gc->finalizer_count].object = object;
        gc->finalizers[gc->finalizer_count].weak_root = object;
        GC_ROOT_WEAK(gc, &gc->finalizers[gc->finalizer_count].weak_root);
        gc->finalizer_count += 1;
    }
    
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

    void* ptr = (void*) arena_alloc(&gc->active, size, align, 1);
    if (ptr != NULL) goto handle_success;
    if (ptr == NULL && gc->collecting) longjmp(gc->collect_mark, 1);

    gc_collect(gc);
    while ((ptr = arena_alloc(&gc->active, size, align, 1)) == NULL) {
        size_t capacity = (gc->inactive.end - gc->inactive.base);
        if (!grow_capacity(&capacity, sizeof(uint8_t), 4096)) {
            goto arena_realloc_failed;
        }

        // Grow inactive `Arena`.
        arena_free(&gc->inactive);
        if (!arena_init(&gc->inactive, capacity)) goto arena_realloc_failed;

        // Copy from the active `Arena` to the inactive `Arena and swap
        // arenas.
        gc_collect(gc);

        /// Grow previously active `Arena`.
        arena_free(&gc->inactive);
        if (!arena_init(&gc->inactive, capacity)) goto arena_realloc_failed;
    }

handle_success:
    return ptr;

arena_realloc_failed:
    fprintf(stderr, "growing gc arena failed\n");
    exit(EXIT_FAILURE);
}

#ifdef ENABLE_TESTS

#include "test.h"

typedef struct {
    GcObject object;
    size_t len;
} GcArray;

size_t gc_array_size(GcObject* object) {
    return sizeof(GcArray) + ((GcArray*) object)->len;
}

void gc_array_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    ((GcArray*) new_object)->len = ((GcArray*) object)->len;
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
        GC_ROOT(&gc, &high[i]);
    }

    for (size_t i = 0; i < countof(low); i++) {
        low[i] = gc_alloc(&gc, low_align, sizeof(GcArray));
        ((GcArray*) low[i])->len = 0;
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

    gc_collect(&gc);
    result = true;
cleanup:
    gc_free(&gc);
    return result;
}

typedef struct {
    GcObject object;
    GcObject** saver;
    size_t saved;
} Escaper;

size_t escaper_size(GcObject* object) {
    return sizeof(Escaper);
}

void escaper_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    ((Escaper*) new_object)->saver = ((Escaper*) object)->saver;
    ((Escaper*) new_object)->saved = ((Escaper*) object)->saved;
}

GcObject* escaper_get_children(GcObject* object, GcObject* position) {
    return NULL;
}

void escaper_finalize(Gc* gc, GcObject* object) {
    if (((Escaper*) object)->saved == 5) {
        *((Escaper*) object)->saver = NULL;
        return;
    }

    *((Escaper*) object)->saver = object;
    GC_ROOT(gc, ((Escaper*) object)->saver);
    ((Escaper*) object)->saved += 1;
}

bool gc_handle_escaping_finalizer() {
    Gc gc;
    if (!gc_init(&gc)) {
        return false;
    }

    bool result = false;

    size_t type_id = gc_add_type_with_finalizer(
        &gc,
        alignof(Escaper),
        escaper_size,
        escaper_copy,
        escaper_get_children,
        escaper_finalize
    );

    Escaper* saver = (Escaper*) alignof(Escaper);
    Escaper* escaper = (Escaper*) gc_alloc(&gc, type_id, sizeof(Escaper));
    escaper->saver = (GcObject**) &saver;
    escaper->saved = 0;

    gc_collect(&gc);
    while (saver != NULL) {
        gc_collect(&gc);
        GC_UNROOT(&gc, &saver);
        gc_collect(&gc);
    }

    result = true;
cleanup:
    gc_free(&gc);
    return result;
}

TestDefinition gc_tests[] = {
    DEFINE_TEST(gc_handle_failed_alloc_during_collect, 0),
    DEFINE_TEST(gc_handle_escaping_finalizer, 0),
};

TestList gc_list = (TestList) {
    gc_tests,
    countof(gc_tests)
};

#endif
