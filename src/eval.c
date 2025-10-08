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
        goto cleanup;
    }

    for (size_t i = 0; i < builtin_def_count; i++) {
        BuiltinDef builtin = builtin_def_list[i];
        
        s8 sym = EXTRACT_SYMBOL(EXTRACT_CAR(sexpr));
        if (!s8_equals(sym, builtin.name))
            continue;

        *is_builtin = true;
        if (arg_count != builtin.arg_count && !builtin.variadic_args) {
            eval_context_erronous_arg_count(context, builtin.arg_count);
            goto cleanup;
        }

        SExpr* final_args;
        if (builtin.eval_args) {
            if (
                !eval_function_arguments(
                    vm,
                    context,
                    EXTRACT_CDR(sexpr),
                    &final_args
                )
            ) {
                goto cleanup;
            }
        } else {
            final_args = EXTRACT_CDR(sexpr);
        }

        if (builtin.func(vm, context, final_args, result)) {
            func_result = true;
            goto cleanup;
        }
        break;
    }

cleanup:
    if (func_result) eval_context_pop_frame(context);
    VM_UNROOT(vm, &sexpr);
    VM_UNROOT(vm, &context);
    return func_result;
}

bool eval_func(
    Vm* vm,
    EvalContext* context,
    SExpr* function_id,
    SExpr* function_def,
    SExpr* function_args,
    SExpr** result
) {
    if (!validate_func_def(vm, context, function_def)) {
        return false;
    }

    bool success = false;
    SExpr* evaluated_args = NULL;
    SExpr* tmp = NULL;

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &function_def);
    VM_ROOT(vm, &function_args);
    VM_ROOT(vm, &evaluated_args);

    eval_context_push_frame(vm, context, function_id);

    success = eval_function_arguments(
        vm,
        context,
        function_args,
        &evaluated_args
    );
    if (!success) goto cleanup;
    success = false; // Reset tracker.
    
    // Pair our evaluated arguments with their corresponding symbols
    // and enter them into the symbol table.
    SExpr* def_iter = EXTRACT_CAR(function_def);
    SExpr* value_iter = evaluated_args;

    VM_ROOT(vm, &def_iter);
    VM_ROOT(vm, &value_iter);
    while (!IS_NIL(def_iter)) {
        eval_context_add_symbol(
            vm,
            context,
            EXTRACT_CAR(def_iter),
            EXTRACT_CAR(value_iter)
        );

        def_iter = EXTRACT_CDR(def_iter);
        value_iter = EXTRACT_CDR(value_iter);
    }
    VM_UNROOT(vm, &value_iter);
    VM_UNROOT(vm, &def_iter);

    success = eval_internal(
        vm,
        context,
        EXTRACT_CAR(EXTRACT_CDR(function_def)),
        &tmp
    );
    if (success) {
        *result = tmp;
        eval_context_pop_frame(context);
    }
cleanup:
    VM_UNROOT(vm, &evaluated_args);
    VM_UNROOT(vm, &function_args);
    VM_UNROOT(vm, &function_def);
    VM_UNROOT(vm, &context);
    return success;
}

bool eval_internal(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &sexpr);

    bool success = false;
    if (IS_NIL(sexpr) || IS_STRING(sexpr) || IS_NUMBER(sexpr)) {
        *result = sexpr;
        success = true;
        goto cleanup;
    }

    if (IS_SYMBOL(sexpr)) {
        if (!eval_context_lookup(vm, context, sexpr, result)) {
            eval_context_symbol_lookup_failed(context, sexpr);
            goto cleanup;
        }

        success = true;
        goto cleanup;
    }

    if (IS_SYMBOL(EXTRACT_CAR(sexpr))) {
        bool is_builtin = false;
        if (eval_handle_builtin(vm, context, sexpr, result, &is_builtin)) {
            success = true;
            goto cleanup;
        } else if (is_builtin) {
            goto cleanup;
        }

        SExpr* function_def = NULL;
        if (env_lookup(&vm->funcs, EXTRACT_CAR(sexpr), &function_def)) {
            // Found user-defined function.
            success = eval_func(
                vm,
                context,
                EXTRACT_CAR(sexpr),
                function_def,
                EXTRACT_CDR(sexpr),
                result
            );
            goto cleanup;
        }

        // Try to look for a variable with a lambda.
    } else if (IS_CONS(EXTRACT_CAR(sexpr))) {
        SExpr* function_id = EXTRACT_CAR(sexpr);
        // Possibly a lambda function.
        if (
            IS_SYMBOL(EXTRACT_CAR(function_id))
            && s8_equals(
                EXTRACT_SYMBOL(EXTRACT_CAR(function_id)),
                s8("lambda")
            )
        ) {
            // Found lambda function.
            success = eval_func(
                vm,
                context,
                function_id,
                EXTRACT_CDR(function_id),
                EXTRACT_CDR(sexpr),
                result
            );
            goto cleanup;
        }
    }

    eval_context_illegal_call(context, sexpr);

cleanup:
    VM_UNROOT(vm, &sexpr);
    VM_UNROOT(vm, &context);
    return success;
}

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

bool validate_func_def(
    Vm* vm,
    EvalContext* context,
    SExpr* function_def
) {
    // Check that `function_def` is a non-NIL cons cell.
    if (IS_NIL(function_def) || !IS_CONS(function_def)) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    SExpr* cons_1 = EXTRACT_CDR(function_def);
    // Check that `function_def` has at least two arguments.
    if (IS_NIL(cons_1) || !IS_CONS(cons_1)) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    // Check that `function_def` has precisely two arguments.
    if (!IS_NIL(EXTRACT_CDR(cons_1))) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    /// Validate that the first argument is a list of symbols.
    SExpr* symbol_list = EXTRACT_CAR(function_def);
    if (!IS_CONS(symbol_list)) {
        eval_context_invalid_type(context, 0, symbol_list, SEXPR_CONS);
        return false;
    }

    size_t symbol_index = 0;
    SExpr* symbol_cons = symbol_list;
    while (!IS_NIL(symbol_cons)) {
        if (!IS_SYMBOL(EXTRACT_CAR(symbol_cons))) {
            eval_context_invalid_arg_def_type(
                context,
                symbol_index,
                EXTRACT_CAR(symbol_cons)
            );
            return false;
        }

        if (!IS_CONS(EXTRACT_CDR(symbol_cons))) {
            eval_context_dotted_arg_list(
                context,
                symbol_index,
                symbol_list
            );
            return false;
        }

        symbol_index += 1;
        symbol_cons = EXTRACT_CDR(symbol_cons);
    }

    return true;
}

bool eval_function_arguments(
    Vm* vm,
    EvalContext* context,
    SExpr* arguments,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    bool success = false;

    SExpr* args_list = NULL;
    SExpr* current = NULL;
    VM_ROOT(vm, &args_list);
    VM_ROOT(vm, &current);

    SExpr* arg_cons = arguments;
    VM_ROOT(vm, &arg_cons);
    while (!IS_NIL(arg_cons)) {
        SExpr* tmp = NULL;
        if (!eval_internal(vm, context, EXTRACT_CAR(arg_cons), &tmp)) {
            goto cleanup;
        }

        SExpr* cons = vm_alloc_cons(vm, tmp, NIL);
        if (args_list == NULL) {
            args_list = cons;
            current = args_list;
        } else {
            AS_CONS(current)->cdr = cons;
            current = AS_CONS(current)->cdr;
        }

        arg_cons = EXTRACT_CDR(arg_cons);
    }

    *result = args_list;
    success = true;

cleanup:
    VM_UNROOT(vm, &arg_cons);

    VM_UNROOT(vm, &current);
    VM_UNROOT(vm, &args_list);

    VM_UNROOT(vm, &context);
    return success;
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
