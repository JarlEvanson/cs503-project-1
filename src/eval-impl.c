#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval-context.h"
#include "eval-impl.h"
#include "sexpr.h"
#include "vm.h"

bool validate_function_def(EvalContext* context, SExpr* def) {
    // Function definitions are of the form ((args...) body).
    // Functions can have zero arguments.

    // Validate that `def` is not NIL and is a cons cell.
    if (!IS_CONS(def) || IS_NIL(def)) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    // Validate that `def` has two values.
    if (IS_NIL(EXTRACT_CDR(def)) || !IS_CONS(EXTRACT_CDR(def))) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    // Check that `def` has precisely two values.
    if (!IS_NIL(EXTRACT_CDR(EXTRACT_CDR(def)))) {
        eval_context_erronous_arg_count(context, 2);
        return false;
    }

    // Validate that the first of the two values is a list of symbols.
    SExpr* symbol_list = EXTRACT_CAR(def);
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

bool validate_lambda_def(EvalContext* context, SExpr* lambda) {
    // Lambda definitions is of the form (lambda function_def).

    if (IS_NIL(lambda) || !IS_CONS(lambda)) {
        return false;
    }

    if (!IS_SYMBOL(EXTRACT_CAR(lambda))) {
        return false;
    }

    if (!s8_equals(EXTRACT_SYMBOL(EXTRACT_CAR(lambda)), s8("lambda"))) {
        return false;
    }

    return validate_function_def(context, EXTRACT_CDR(lambda));
}

size_t tab_count = 0;
void p() {
    for (size_t i = 0; i < tab_count; i++) { printf("\t"); }
}


bool eval_internal(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result
) {

#ifdef DEBUG_LOG_EVAL
    p(); PRINT_SEXPR(sexpr); printf("\n"); tab_count++;
#endif
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &sexpr);

    bool success = false;
    if (IS_NIL(sexpr) || IS_NUMBER(sexpr) || IS_STRING(sexpr)) {
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

    if (eval_context_stack_depth(context) >= 4096) {
        eval_context_max_stack_depth_reached(context);
        goto cleanup;
    }

    if (IS_SYMBOL(EXTRACT_CAR(sexpr))) {
        if (lookup_builtin(EXTRACT_SYMBOL(EXTRACT_CAR(sexpr)), NULL)) {
            success = eval_func(
                vm,
                context,
                EXTRACT_CAR(sexpr),
                NIL,
                EXTRACT_CDR(sexpr),
                result
            );
            goto cleanup;
        }

        SExpr* function_def = NULL;
        if (env_lookup(&vm->funcs, EXTRACT_CAR(sexpr), &function_def)) {
            success = eval_func(
                vm,
                context,
                EXTRACT_CAR(sexpr),
                EXTRACT_CDR(EXTRACT_CDR(function_def)),
                EXTRACT_CDR(sexpr),
                result
            );
            goto cleanup;
        }
    } else if (IS_CONS(EXTRACT_CAR(sexpr))) {
        // Possible direct call of lambda expression.
        if (validate_lambda_def(context, EXTRACT_CAR(sexpr))) {
            success = eval_func(
                vm,
                context,
                EXTRACT_CAR(sexpr),
                EXTRACT_CDR(EXTRACT_CAR(sexpr)),
                EXTRACT_CDR(sexpr),
                result
            );
            goto cleanup;
        }
    }

    eval_context_illegal_call(context, sexpr);

cleanup:
#ifdef DEBUG_LOG_EVAL
    tab_count--; p(); PRINT_SEXPR(sexpr); printf(": "); PRINT_SEXPR(*result); printf("\n");
#endif
    VM_UNROOT(vm, &sexpr);
    VM_UNROOT(vm, &context);
    return success;
}

bool eval_func(
    Vm* vm,
    EvalContext* context,
    SExpr* id,
    SExpr* def,
    SExpr* args,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &id);
    VM_ROOT(vm, &def);
    VM_ROOT(vm, &args);
    
    bool success = false;

    // This function must only be called with a symbol for an `id` or a lambda
    // expression for an `id`.
    ASSERT(IS_SYMBOL(id) || (IS_CONS(id) && !IS_NIL(id)));
    eval_context_push_frame(vm, context, id);

    size_t arg_count = 0;
    SExpr* arg = args;
    while (!IS_NIL(arg)) {
        if (!IS_CONS(arg)) {
            eval_context_dotted_arg_list(context, arg_count, args);
            goto cleanup;
        }

        arg_count += 1;
        arg = EXTRACT_CDR(arg);
    }

    bool builtin;
    bool eval_args;
    bool variadic;
    size_t var_count;

    bool (*func)(
        Vm* vm,
        EvalContext* context,
        size_t arg_count,
        SExpr* args,
        SExpr** result
    );

    // We only check for symbols and only to correctly handle builtin
    // functions.
    if (IS_SYMBOL(id)) {
        BuiltinDef builtin_def;
        if (lookup_builtin(EXTRACT_SYMBOL(id), &builtin_def)) {
            builtin = true;
            eval_args = builtin_def.eval_args;
            variadic = builtin_def.variadic_args;
            var_count = builtin_def.arg_count;
            func = builtin_def.func;
            goto prepare;
        }
    }

    builtin = false;
    eval_args = true;
    variadic = false;
    
    var_count = 0;
    SExpr* var = EXTRACT_CAR(def);
    while (!IS_NIL(var)) {
        if (!IS_CONS(var)) {
            eval_context_dotted_arg_list(context, var_count, EXTRACT_CAR(var));
            goto cleanup;
        }

        if (!IS_SYMBOL(EXTRACT_CAR(var))) {
            eval_context_invalid_arg_def_type(
                context,
                var_count,
                EXTRACT_CAR(var)
            );
            goto cleanup;
        }   

        var_count += 1;
        var = EXTRACT_CDR(var);
    }

prepare:
    if (var_count != arg_count && !variadic) {
        eval_context_erronous_arg_count(context, var_count);
        goto cleanup;
    }

    if (eval_args) {
        SExpr* arg_cons = args;
        VM_ROOT(vm, &arg_cons);
        
        SExpr* args_list = NIL;
        SExpr* current = arg_cons;
        VM_ROOT(vm, &args_list);
        VM_ROOT(vm, &current);
        
        while (!IS_NIL(arg_cons)) {
            SExpr* tmp = NULL;
            if (!eval_internal(vm, context, EXTRACT_CAR(arg_cons), &tmp)) {
                VM_UNROOT(vm, &current);
                VM_UNROOT(vm, &args_list);
                VM_UNROOT(vm, &arg_cons);
                goto cleanup;
            }

            SExpr* cons = vm_alloc_cons(vm, tmp, NIL);
            if (args_list == NIL) {
                args_list = cons;
                current = args_list;
            } else {
                AS_CONS(current)->cdr = cons;
                current = AS_CONS(current)->cdr;
            }

            arg_cons = EXTRACT_CDR(arg_cons);
        }

        args = args_list;

        VM_UNROOT(vm, &current);
        VM_UNROOT(vm, &args_list);
        VM_UNROOT(vm, &arg_cons);
    }

    if (!builtin) {
        SExpr* def_iter = EXTRACT_CAR(def);
        SExpr* value_iter = args;

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

        VM_UNROOT(vm, &def_iter);
        VM_UNROOT(vm, &value_iter);

        success =
            eval_internal(vm, context, EXTRACT_CAR(EXTRACT_CDR(def)), result);
    } else {
        success = func(vm, context, arg_count, args, result);
    }

cleanup:
    eval_context_pop_frame(context);

    VM_UNROOT(vm, &args);
    VM_UNROOT(vm, &def);
    VM_UNROOT(vm, &id);
    VM_UNROOT(vm, &context);
    return success;
}
