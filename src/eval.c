#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval.h"
#include "eval-context.h"
#include "sexpr.h"
#include "vm.h"

static size_t eval_get_arg_count(
    EvalContext* context,
    SExpr* args
) {
    size_t count = 0;

    SExpr* arg = args;
    while (!IS_NIL(arg)) {
        if (!IS_CONS(arg)) {
            eval_context_dotted_arg_list(context, count, args);
            return 0;
        }

        count += 1;
        arg = EXTRACT_CDR(arg);
    }

    return count;
}

bool eval_internal(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result
);

static bool eval_handle_builtin(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result,
    bool* is_builtin
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &sexpr);
    bool func_result = false;

    eval_context_push_frame(vm, context, EXTRACT_CAR(sexpr));

    size_t arg_count = eval_get_arg_count(context, EXTRACT_CDR(sexpr));
    if (!eval_context_is_ok(context)) {
        VM_UNROOT(vm, &sexpr);
        VM_UNROOT(vm, &context);
        return false;
    }

    for (size_t i = 0; i < builtin_def_count; i++) {
        BuiltinDef builtin = builtin_def_list[i];
        
        s8 sym = EXTRACT_SYMBOL(EXTRACT_CAR(sexpr));
        if (!s8_equals(sym, builtin.name))
            continue;

        *is_builtin = true;
        if (arg_count != builtin.arg_count && !builtin.variadic_args) {
            eval_context_erronous_arg_count(context, builtin.arg_count);
            VM_UNROOT(vm, &sexpr);
            VM_UNROOT(vm, &context);
            return false;
        }

        SExpr* final_args = 
            builtin.eval_args ? NULL : EXTRACT_CDR(sexpr);
        SExpr* current = NULL;
        if (builtin.eval_args) {
            VM_ROOT(vm, &final_args);
            VM_ROOT(vm, &current);

            SExpr* to_eval = EXTRACT_CDR(sexpr);
            VM_ROOT(vm, &to_eval);
            while (!IS_NIL(to_eval)) {
                SExpr* tmp = NULL;
                if (!eval_internal(
                    vm,
                    context,
                    EXTRACT_CAR(to_eval),
                    &tmp
                )) {
                    VM_UNROOT(vm, &to_eval);

                    VM_UNROOT(vm, &current);
                    VM_UNROOT(vm, &final_args);
                    goto cleanup;
                }

                SExpr* cons = vm_alloc_cons(vm, tmp, NIL);
                if (final_args == NULL) {
                    final_args = cons;
                    current = final_args;
                } else {
                    AS_CONS(current)->cdr = cons;
                    current = AS_CONS(current)->cdr;
                }

                to_eval = EXTRACT_CDR(to_eval);
            }

            VM_UNROOT(vm, &to_eval);

            VM_UNROOT(vm, &current);
            VM_UNROOT(vm, &final_args);
        }

        if (builtin.func(vm, context, final_args, result)) {
            func_result = true;
            goto cleanup;
        }
        break;
    }

cleanup:
    VM_UNROOT(vm, &sexpr);
    VM_UNROOT(vm, &context);
    return func_result;
}

bool eval_internal(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &sexpr);
    if (IS_NIL(sexpr) || IS_STRING(sexpr) || IS_NUMBER(sexpr)) {
        *result = sexpr;
        VM_UNROOT(vm, &context);
        VM_UNROOT(vm, &sexpr);
        return true;
    }

    if (IS_SYMBOL(sexpr)) {
        if (!eval_context_lookup(vm, context, sexpr, result)) {
            eval_context_symbol_lookup_failed(context, sexpr);
            
            VM_UNROOT(vm, &context);
            VM_UNROOT(vm, &sexpr);
            return false;
        }

        VM_UNROOT(vm, &context);
        VM_UNROOT(vm, &sexpr);
        return true;
    }

    SExpr* function_id = EXTRACT_CAR(sexpr);
    SExpr* function_def = NULL;
    if (IS_SYMBOL(function_id)) {
        bool is_builtin = false;
        if (eval_handle_builtin(vm, context, sexpr, result, &is_builtin)) {
            VM_UNROOT(vm, &context);
            VM_UNROOT(vm, &sexpr);
            return true;
        } else if (is_builtin) {
            VM_UNROOT(vm, &context);
            VM_UNROOT(vm, &sexpr);
            return false;
        }

        if (env_lookup(&vm->funcs, function_id, &function_def)) {
            // Found user-defined function.

            ASSERT(false);
        }

        // Try to look for a variable with a lambda.
        ASSERT(false);
    }

    eval_context_illegal_call(context, sexpr);

    VM_UNROOT(vm, &context);
    VM_UNROOT(vm, &sexpr);
    return false;
}

EvalResult eval(Vm* vm, SExpr* sexpr) {
    VM_ROOT(vm, &sexpr);

    EvalResult eval_result;
    eval_result.as.err = eval_context_alloc(vm);

    VM_ROOT(vm, &eval_result.as.err);

    eval_result.ok = eval_internal(
        vm,
        eval_result.as.err,
        sexpr,
        &eval_result.as.ok
    );

    VM_UNROOT(vm, &eval_result.as.err);
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
