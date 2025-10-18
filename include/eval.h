#ifndef LISP_EVAL_H
#define LISP_EVAL_H

#include "common.h"
#include "eval-context.h"
#include "sexpr.h"
#include "vm.h"

typedef struct {
    bool ok;
    union {
        SExpr* ok;
        EvalContext* err;
    } as;
} EvalResult;

EvalResult eval(Vm* vm, SExpr* sexpr);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList eval_test_list;

#endif

#endif
