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
    // The maximum stack depth allowed was reached.
    MAX_STACK_DEPTH_REACHED,
} ErrorType;

typedef struct EvalFrame EvalFrame;
struct EvalFrame {
    GcObject object;

    SExpr* function_id;

    bool valid_env;
    Environment env;

    EvalFrame* next;
};

struct EvalContext {
    GcObject object;

    bool has_error;

    ErrorType error;
    size_t arg_index;
    SExpr* sexpr;
    SExprType sexpr_type;

    EvalFrame* frame;
};

EvalContext* eval_context_alloc(Vm* vm) {
    EvalContext* context = (EvalContext*)
        gc_alloc(&vm->gc, EVAL_CONTEXT_TYPE_ID, sizeof(EvalContext));

    context->has_error = false;

    context->error = ARG_INVALID_TYPE;
    context->sexpr = NULL;
    context->sexpr_type = SEXPR_SYMBOL;

    context->frame = NULL;

    return context;
}

bool eval_context_is_ok(EvalContext* context) {
    return !context->has_error;
}

void eval_context_add_symbol(
    Vm* vm,
    EvalContext* context,
    SExpr* symbol,
    SExpr* value
) {
    EvalFrame* frame = context->frame;
    while (frame != NULL && !frame->valid_env) {
        frame = frame->next;
    }

    Environment* env;
    if (frame == NULL) {
        env = &vm->vars;
    } else {
        env = &frame->env;
    }

    env_set(vm, env, symbol, value);
}

void eval_context_disable_local_env(EvalContext* context) {
    ASSERT(context->frame != NULL);

    context->frame->valid_env = false;
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

    frame->valid_env = true;
    frame->env = env;

    frame->next = context->frame;
    context->frame = frame;

    VM_UNROOT(vm, &id);
    VM_UNROOT(vm, &context);
}

void eval_context_pop_frame(EvalContext* context) {
    if (context->frame == NULL) return;
    if (context->has_error) return; // Keep stack trace.

    context->frame = context->frame->next;
}

size_t eval_context_stack_depth(EvalContext* context) {
    size_t depth = 0;

    EvalFrame* frame = context->frame;
    while (frame != NULL) {
        depth += 1;
        frame = frame->next;
    }

    return depth;
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
    context->error = INVALID_ARG_DEF_TYPE;
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

void eval_context_max_stack_depth_reached(EvalContext* context) {
    context->has_error = true;
    context->error = MAX_STACK_DEPTH_REACHED;
}

void eval_context_print(const EvalContext* context) {
    if (context->has_error) {
        switch (context->error) {
            case ARG_INVALID_TYPE:
                printf("argument %zu `", context->arg_index);
                PRINT_SEXPR(context->sexpr);
                printf("` is not ");
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
                        printf("a cons cell");
                        break;
                }
                printf("\n");
                break;
            case COND_ARG_NOT_PAIR:
                printf("argument %zu `", context->arg_index);
                PRINT_SEXPR(context->sexpr);
                printf("` is not a pair\n");
                break;
            case DOTTED_ARG_LIST:
                printf(
                    "dotted argument list starts at %zu `",
                    context->arg_index
                );
                PRINT_SEXPR(context->sexpr);
                printf("`\n");
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
                printf("argument definition `");
                PRINT_SEXPR(context->sexpr);
                printf("` at %zu has invalid type\n", context->arg_index);
                break;
            case SYMBOL_LOOKUP_FAILED:
                printf("lookup of symbol `");
                PRINT_SEXPR(context->sexpr);
                printf("` failed\n");
                break;
            case MAX_STACK_DEPTH_REACHED:
                printf("max stack depth reached\n");
                break;
        }
    }

    printf("stack backtrace:\n");

    size_t frame_index = 0;
    EvalFrame* frame = context->frame;
    while (frame != NULL) {
        printf("%5zu: ", frame_index);
        PRINT_SEXPR(frame->function_id);
        printf("\n");

        frame_index += 1;
        frame = frame->next;
    }

    printf("%5zu: <script>\n", frame_index);
}

void eval_context_print_raw(const EvalContext* context) {
    printf("EvalContext {\n");
    printf("\thas_error: %s\n", context->has_error ? "true" : "false");
    printf("\targ_index: %zu\n", context->arg_index);
    printf("\tsexpr: ");
    PRINT_SEXPR_RAW(context->sexpr, 1);
    printf("\n");
    printf("\tsexpr_type: %u\n", context->sexpr_type);

    printf("\tFrames: [\n");
    EvalFrame* frame = context->frame;
    while (frame != NULL) {
        printf("\t\tFrame {\n");
        printf("\t\t\tfunction_id: ");
        PRINT_SEXPR_RAW(frame->function_id, 3);
        printf("\n\t\t\tvalid_env: %s", frame->valid_env ? "true" : "false");

        printf("\n\t\t\tenv: [");

        SExpr* symbols = EXTRACT_CAR(frame->env.list);
        SExpr* values = EXTRACT_CAR(EXTRACT_CDR(frame->env.list));
        while (!IS_NIL(symbols)) {
            SExpr* symbol = EXTRACT_CAR(symbols);
            SExpr* value = EXTRACT_CAR(values);

            printf("\n\t\t\t\t");
            PRINT_SEXPR(symbol);
            printf(" ");
            PRINT_SEXPR(value);

            symbols = EXTRACT_CDR(symbols);
            values = EXTRACT_CDR(values);
        }
        printf("\n\t\t\t]");
        printf("\n\t\t}\n");

        frame = frame->next;
    }

    printf("\t]\n}\n");
}

size_t eval_context_size(GcObject* object) {
    return sizeof(EvalContext);
}

void eval_context_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    EvalContext* new = (EvalContext*) new_object;
    EvalContext* old = (EvalContext*) object;

    new->has_error = old->has_error;

    new->error = old->error;
    new->arg_index = old->arg_index;
    if (old->sexpr != NULL) {
        new->sexpr = (SExpr*) gc_copy_object(gc, (GcObject*) old->sexpr);
    } else {
        new->sexpr = NULL;
    }
    new->sexpr_type = old->sexpr_type;

    if (old->frame != NULL) {
        new->frame = (EvalFrame*) gc_copy_object(gc, (GcObject*) old->frame);
    } else {
        new->frame = NULL;
    }
}

GcObject* eval_context_get_children(GcObject* object, GcObject* position) {
    GcObject* sexpr = (GcObject*) ((EvalContext*) object)->sexpr;
    GcObject* frame = (GcObject*) ((EvalContext*) object)->frame;

    if (position == NULL && sexpr != NULL) return sexpr;
    if (
        (position == NULL && sexpr == NULL)
        || (position == sexpr && frame != NULL)
    ) {
        return frame;
    }

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

    new_frame->valid_env = frame->valid_env;
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

void gc_add_eval_context(Gc* gc) {
    size_t type_id = gc_add_type(
        gc,
        alignof(EvalFrame),
        eval_frame_size,
        eval_frame_copy,
        eval_frame_get_children
    );
    ASSERT(type_id == EVAL_FRAME_TYPE_ID);

    type_id = gc_add_type(
        gc,
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

bool eval_context_symbol_manipulation() {
    Vm vm;
    if (!vm_init(&vm)) return false;

    EvalContext* context = eval_context_alloc(&vm);
    VM_ROOT(&vm, &context);

    bool result = false;
    SExpr* symbol = NULL;
    SExpr* value = NULL;
    VM_ROOT(&vm, &symbol);
    VM_ROOT(&vm, &value);

    symbol = vm_alloc_symbol(&vm, s8("Frame 0"));
    eval_context_push_frame(&vm, context, symbol);

    symbol = vm_alloc_symbol(&vm, s8("v"));
    value = vm_alloc_symbol(&vm, s8("test_0"));
    eval_context_add_symbol(
        &vm,
        context,
        symbol,
        value
    );

    symbol = vm_alloc_symbol(&vm, s8("Frame 1"));
    eval_context_push_frame(&vm, context, symbol);

    symbol = vm_alloc_symbol(&vm, s8("v"));

    value = vm_alloc_symbol(&vm, s8("test_1"));
    eval_context_add_symbol(
        &vm,
        context,
        symbol,
        value
    );

    SExpr* sexpr_result = NULL;
    symbol = vm_alloc_symbol(&vm, s8("v"));
    eval_context_lookup(&vm, context, symbol, &sexpr_result);
    if (!s8_equals(s8("test_1"), EXTRACT_SYMBOL(sexpr_result))) goto cleanup;

    eval_context_pop_frame(context);
    eval_context_lookup(&vm, context, symbol, &sexpr_result);
    if (!s8_equals(s8("test_0"), EXTRACT_SYMBOL(sexpr_result))) goto cleanup;

    result = true;
cleanup:
    VM_UNROOT(&vm, &value);
    VM_UNROOT(&vm, &symbol);
    VM_UNROOT(&vm, &context);
    vm_free(&vm);
    return result;
}

static TestDefinition eval_context_tests[] = {
    DEFINE_UNIT_TEST(eval_context_symbol_manipulation, 7),
};

TestList eval_context_test_list = (TestList) {
    eval_context_tests,
    countof(eval_context_tests)
};

#endif
