#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval-context.h"
#include "eval-impl.h"
#include "eval.h"
#include "sexpr.h"
#include "vm.h"

EvalResult eval(Vm* vm, SExpr* sexpr) {
    SExpr* result = NULL;
    EvalContext* context = NULL;

    VM_ROOT(vm, &sexpr);
    VM_ROOT(vm, &result);
    VM_ROOT(vm, &context);

    context = eval_context_alloc(vm);
    eval_internal(vm, context, sexpr, &result);

    EvalResult eval_result;
    if (eval_context_is_ok(context)) {
        eval_result.ok = true;
        eval_result.as.ok = result;
    } else {
        eval_result.ok = false;
        eval_result.as.err = context;
    }

    VM_UNROOT(vm, &result);
    VM_UNROOT(vm, &context);
    VM_UNROOT(vm, &sexpr);
    return eval_result;
}


#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

TestDefinition eval_tests[] = {

};

TestList eval_test_list = (TestList) {
    eval_tests,
    countof(eval_tests)
};

#endif
