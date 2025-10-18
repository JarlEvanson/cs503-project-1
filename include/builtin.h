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
    size_t arg_count,
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

bool lookup_builtin(s8 id, BuiltinDef* out);

#endif
