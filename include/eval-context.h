#ifndef LISP_EVAL_CONTEXT_H
#define LISP_EVAL_CONTEXT_H

#include "common.h"
#include "sexpr.h"
#include "vm.h"

typedef struct EvalContext EvalContext;

EvalContext* eval_context_alloc(Vm* vm);
bool eval_context_is_ok(EvalContext* context);
void eval_context_add_symbol(
    Vm* vm,
    EvalContext* context,
    SExpr* symbol,
    SExpr* value
);
bool eval_context_lookup(
    Vm* vm,
    EvalContext* context,
    SExpr* symbol,
    SExpr** result
);

void eval_context_push_frame(Vm* vm, EvalContext* context, SExpr* id);
void eval_context_pop_frame(EvalContext* context);
size_t eval_context_stack_depth(EvalContext* context);

void eval_context_invalid_type(
    EvalContext* context,
    size_t arg_index,
    SExpr* arg,
    SExprType type
);
void eval_context_cond_arg_not_pair(
    EvalContext* context,
    size_t arg_index,
    SExpr* not_pair
);
void eval_context_dotted_arg_list(
    EvalContext* context,
    size_t dotted_start,
    SExpr* arg_list
);
void eval_context_erronous_arg_count(
    EvalContext* context,
    size_t required_arg_count
);
void eval_context_illegal_call(
    EvalContext* context,
    SExpr* sexpr
);
void eval_context_invalid_arg_def_type(
    EvalContext* context,
    size_t arg_index,
    SExpr* arg_def
);
void eval_context_symbol_lookup_failed(
    EvalContext* context,
    SExpr* sexpr
);
void eval_context_max_stack_depth_reached(EvalContext* context);

void eval_context_print(const EvalContext* context);
void eval_context_print_raw(const EvalContext* context);

void gc_add_eval_context(Gc* gc);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList eval_context_test_list;

#endif

#endif
