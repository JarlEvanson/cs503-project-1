#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "eval-context.h"
#include "gc.h"
#include "sexpr.h"
#include "vm.h"

#define EVAL_FRAME_TYPE_ID 5
#define EVAL_CONTEXT_TYPE_ID 6

typedef enum {
    // An argument has an invalid type.
    //
    // `arg_index`, `sexpr`, and `sexpr_type` are active.
    ARG_INVALID_TYPE,
    // An argument to `cond` is not a pair.
    //
    // `arg_index` and `sexpr` are active.
    COND_ARG_NOT_PAIR,
    // An argument list is dotted.
    //
    // `arg_index` and `sexpr` are active.
    DOTTED_ARG_LIST,
    // The number of arguments to a function is either less than or greater
    // than the defined number.
    //
    // `arg_index` is active and is the required number of arguments.
    ERRONOUS_ARG_COUNT,
    // The function call is illegal.
    //
    // `sexpr` is active.
    ILLEGAL_FUNC_CALL,
    // An argument defined in function is not a symbol.
    //
    // `arg_index` and `sexpr` are active.
    INVALID_ARG_DEF_TYPE,
    // Lookup of a function or a variable failed.
    //
    // `sexpr` is active.
    SYMBOL_LOOKUP_FAILED,
} ErrorType;

typedef struct EvalFrame EvalFrame;
struct EvalFrame {
    SExpr* function_id;

    Environment env;

    EvalFrame* next;
};

struct EvalContext {
    bool has_error;

    ErrorType error;
    size_t arg_index;
    SExpr* sexpr;
    SExprType sexpr_type;

    EvalFrame* frame;
};

EvalContext* eval_context_alloc(Vm* vm) {
    GcObject* object =
        gc_alloc(&vm->gc, EVAL_CONTEXT_TYPE_ID, sizeof(EvalContext));

    ((EvalContext*) object)->has_error = false;

    ((EvalContext*) object)->error = ARG_INVALID_TYPE;
    ((EvalContext*) object)->arg_index = 0;
    ((EvalContext*) object)->sexpr = NULL;
    ((EvalContext*) object)->sexpr_type = SEXPR_SYMBOL;

    ((EvalContext*) object)->frame = NULL;
    return (EvalContext*) object;
}

bool eval_context_is_ok(EvalContext* context) {
    return !context->has_error;
}

bool eval_context_lookup(
    Vm* vm,
    EvalContext* context,
    SExpr* symbol,
    SExpr** result
) {
    EvalFrame* frame = context->frame;
    while (frame != NULL) {
        if (env_lookup(&frame->env, symbol, result)) {
            return true;
        }

        frame = frame->next;
    }

    if (env_lookup(&vm->vars, symbol, result)) {
        return true;
    }

    return false;
}

void eval_context_push_frame(Vm* vm, EvalContext* context, SExpr* id) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &id);

    Environment env;
    env_init(vm, &env);

    VM_ROOT(vm, &env.list);
    EvalFrame* frame =
        (EvalFrame*) gc_alloc(&vm->gc, EVAL_FRAME_TYPE_ID, sizeof(EvalFrame));
    VM_UNROOT(vm, &env.list);

    frame->function_id = id;
    frame->env = env;

    frame->next = context->frame;
    context->frame = frame;

    VM_UNROOT(vm, &id);
    VM_UNROOT(vm, &context);
}

void eval_context_invalid_type(
    EvalContext* context,
    size_t arg_index,
    SExpr* arg,
    SExprType type
) {
    context->has_error = true;
    context->error = ARG_INVALID_TYPE;
    context->arg_index = arg_index;
    context->sexpr = arg;
    context->sexpr_type = type;
}

void eval_context_cond_arg_not_pair(
    EvalContext* context,
    size_t arg_index,
    SExpr* not_pair
) {
    context->has_error = true;
    context->error = COND_ARG_NOT_PAIR;
    context->arg_index = arg_index;
    context->sexpr = not_pair;
}

void eval_context_dotted_arg_list(
    EvalContext* context,
    size_t dotted_start,
    SExpr* arg_list
) {
    context->has_error = true;
    context->error = DOTTED_ARG_LIST;
    context->arg_index = dotted_start;
    context->sexpr = arg_list;
}

void eval_context_erronous_arg_count(
    EvalContext* context,
    size_t required_arg_count
) {
    context->has_error = true;
    context->error = ERRONOUS_ARG_COUNT;
    context->arg_index = required_arg_count;
}

void eval_context_illegal_call(
    EvalContext* context,
    SExpr* sexpr
) {
    context->has_error = true;
    context->error = ILLEGAL_FUNC_CALL;
    context->sexpr = sexpr;
}

void eval_context_invalid_arg_def_type(
    EvalContext* context,
    size_t arg_index,
    SExpr* arg_def
) {
    context->has_error = true;
    context->error = ERRONOUS_ARG_COUNT;
    context->arg_index = arg_index;
    context->sexpr = arg_def;
}

void eval_context_symbol_lookup_failed(
    EvalContext* context,
    SExpr* symbol
) {
    context->has_error = true;
    context->error = SYMBOL_LOOKUP_FAILED;
    context->sexpr = symbol;
}

void eval_context_print(const EvalContext* context) {
    ASSERT(context->has_error);

    switch (context->error) {
        case ARG_INVALID_TYPE:
            printf("argument %zu (`", context->arg_index);
            PRINT_SEXPR(context->sexpr);
            printf("`) is not ");
            switch (context->sexpr_type) {
                case SEXPR_SYMBOL:
                    printf("symbol");
                    break;
                case SEXPR_STRING:
                    printf("string");
                    break;
                case SEXPR_NUMBER:
                    printf("number");
                    break;
                case SEXPR_CONS:
                    printf("cons");
                    break;
            }
            printf("\n");
            break;
        case COND_ARG_NOT_PAIR:
            printf("argument %zu (`", context->arg_index);
            PRINT_SEXPR(context->sexpr);
            printf("`) is not a pair\n");
            break;
        case DOTTED_ARG_LIST:
            printf(
                "dotted argument list starts at %zu (`",
                context->arg_index
            );
            PRINT_SEXPR(context->sexpr);
            printf("`)\n");
            break;
        case ERRONOUS_ARG_COUNT:
            printf(
                "erronous argument count: %zu required arguments\n",
                context->arg_index
            );
            break;
        case ILLEGAL_FUNC_CALL:
            printf("illegal function call `");
            PRINT_SEXPR(context->sexpr);
            printf("`\n");
            break;
        case INVALID_ARG_DEF_TYPE:
            printf("argument definition (`");
            PRINT_SEXPR(context->sexpr);
            printf("at %zu has invalid type\n", context->arg_index);
            break;
        case SYMBOL_LOOKUP_FAILED:
            printf("lookup of symbol `");
            PRINT_SEXPR(context->sexpr);
            printf("` failed\n");
            break;
    }

    printf("stack backtrace:\n");

    size_t frame_index = 0;
    EvalFrame* frame = context->frame;
    while (frame != NULL) {
        printf("%5zu: ", frame_index);
        PRINT_SEXPR(frame->function_id);
        printf("\n");

        frame = frame->next;
    }

    printf("%5zu: <script>\n", frame_index);
}

size_t eval_context_size(GcObject* object) {
    return sizeof(EvalContext);
}

void eval_context_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    EvalContext* eval = (EvalContext*) object;
    EvalContext* new_eval = (EvalContext*) new_object;

    new_eval->has_error = eval->has_error;

    new_eval->error = eval->error;
    new_eval->arg_index = eval->arg_index;
    new_eval->sexpr = (SExpr*) gc_copy_object(gc, (GcObject*) eval->sexpr);
    new_eval->sexpr_type = eval->sexpr_type;

    new_eval->frame = (EvalFrame*) gc_copy_object(gc, (GcObject*) eval->frame);
}

GcObject* eval_context_get_children(GcObject* object, GcObject* position) {
    if (position == NULL) return (GcObject*) ((EvalContext*) object)->frame;
    return NULL;
}

size_t eval_frame_size(GcObject* object) {
    return sizeof(EvalFrame);
}

void eval_frame_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    EvalFrame* frame = (EvalFrame*) object;
    EvalFrame* new_frame = (EvalFrame*) new_object;

    new_frame->function_id =
        (SExpr*) gc_copy_object(gc, (GcObject*) frame->function_id);

    new_frame->env.list =
        (SExpr*) gc_copy_object(gc, (GcObject*) frame->env.list);

    if (frame->next != NULL) {
        new_frame->next = 
            (EvalFrame*) gc_copy_object(gc, (GcObject*) frame->next);
    } else {
        new_frame->next = NULL;
    }
}

GcObject* eval_frame_get_children(GcObject* object, GcObject* position) {
    GcObject* function_id = (GcObject*) ((EvalFrame*) object)->function_id;
    GcObject* env = (GcObject*) ((EvalFrame*) object)->env.list;
    GcObject* next = (GcObject*) ((EvalFrame*) object)->next;

    if (position == NULL && function_id != NULL)
        return function_id;

    if ((position == NULL && function_id == NULL) || position == function_id)
        return env;

    if (position != next) return (GcObject*) next;
    return NULL;
}

void vm_add_support(Vm* vm) {
    size_t type_id = gc_add_type(
        &vm->gc,
        alignof(EvalFrame),
        eval_frame_size,
        eval_frame_copy,
        eval_frame_get_children
    );
    ASSERT(type_id == EVAL_FRAME_TYPE_ID);

    type_id = gc_add_type(
        &vm->gc,
        alignof(EvalContext),
        eval_context_size,
        eval_context_copy,
        eval_context_get_children
    );
    ASSERT(type_id == EVAL_CONTEXT_TYPE_ID);
}

#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

static TestDefinition eval_context_tests[] = {

};

TestList eval_context_test_list = (TestList) {
    eval_context_tests,
    countof(eval_context_tests)
};

#endif
