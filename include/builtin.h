#ifndef LISP_BUILTIN_H
#define LISP_BUILTIN_H

#include "common.h"
#include "eval-context.h"
#include "s8.h"
#include "sexpr.h"
#include "vm.h"

typedef bool (*BuiltinFunc)(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
);

typedef struct {
    s8 name;
    bool variadic_args;
    size_t arg_count;

    bool eval_args;

    BuiltinFunc func;
} BuiltinDef;

extern BuiltinDef builtin_def_list[];
extern size_t builtin_def_count;

#endif
