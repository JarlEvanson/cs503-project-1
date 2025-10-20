#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval-context.h"
#include "eval-impl.h"
#include "parser.h"
#include "sexpr.h"
#include "util.h"
#include "vm.h"

#define DEFINE_BUILTIN(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, true, func }

#define DEFINE_BUILTIN_VARIADIC(name, func) \
    (BuiltinDef) { s8(name), true, 0, true, func }

#define DEFINE_BUILTIN_NO_EVAL(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, false, func }

#define DEFINE_BUILTIN_NO_EVAL_VARIADIC(name, func) \
    (BuiltinDef) { s8(name), true, 0, false, func }

static bool builtin_is_nil(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_NIL(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_is_symbol(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_SYMBOL(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_is_string(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_STRING(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_is_number(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_NUMBER(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_is_list(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_CONS(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_sexp_to_bool(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = !IS_NIL(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool two_numbers(EvalContext* context, SExpr* args) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));

    if (!IS_NUMBER(arg_0))
        eval_context_invalid_type(context, 0, arg_0, SEXPR_NUMBER);

    if (!IS_NUMBER(arg_1))
        eval_context_invalid_type(context, 1, arg_1, SEXPR_NUMBER);

    return eval_context_is_ok(context);
}

static bool builtin_add(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) + EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool builtin_sub(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) - EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool builtin_mul(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) * EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool builtin_div(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) / EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool builtin_mod(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));

    double dividend = EXTRACT_NUMBER(arg_0) / EXTRACT_NUMBER(arg_1);
    if (!isnormal(dividend)) {
        *result = vm_alloc_number(vm, dividend);
        return true;
    }

    double integral;
    double fractional = modf(dividend, &integral);
    *result = vm_alloc_number(vm, fractional * EXTRACT_NUMBER(arg_1));
    return true;
}

static bool builtin_lt(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    if (EXTRACT_NUMBER(arg_0) < EXTRACT_NUMBER(arg_1)) {
        *result = vm_alloc_symbol(vm, s8("t"));
    } else {
        *result = NIL;
    }
    return true;
}

static bool builtin_gt(
	Vm* vm,
	EvalContext* context,
	size_t arg_count,
	SExpr* args,
	SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    if (EXTRACT_NUMBER(arg_0) > EXTRACT_NUMBER(arg_1)) {
        *result = vm_alloc_symbol(vm, s8("t"));
    } else {
        *result = NIL;
    }
    return true;
}

static bool builtin_lte(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    if (EXTRACT_NUMBER(arg_0) <= EXTRACT_NUMBER(arg_1)) {
        *result = vm_alloc_symbol(vm, s8("t"));
    } else {
        *result = NIL;
    }
    return true;
}

static bool builtin_gte(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    if (EXTRACT_NUMBER(arg_0) >= EXTRACT_NUMBER(arg_1)) {
        *result = vm_alloc_symbol(vm, s8("t"));
    } else {
        *result = NIL;
    }
    return true;
}

static bool builtin_eq(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));

    bool eq = false;
    if (IS_SYMBOL(arg_0) && IS_SYMBOL(arg_1)) {
        eq = s8_equals(EXTRACT_SYMBOL(arg_0), EXTRACT_SYMBOL(arg_1));
    } else if (IS_STRING(arg_0) && IS_STRING(arg_1)) {
        eq = s8_equals(EXTRACT_STRING(arg_0), EXTRACT_STRING(arg_1));
    } else if (IS_NUMBER(arg_0) && IS_NUMBER(arg_1)) {
        double val_0 = EXTRACT_NUMBER(arg_0);
        double val_1 = EXTRACT_NUMBER(arg_1);

        double precision = val_0 * val_1 * 0.000001;
        eq = fabs(val_0 - val_1) < precision;
    } else if (IS_CONS(arg_0) || IS_CONS(arg_1)) {
        eval_context_illegal_call(context, args);
        return false;
    } else {
        eval_context_invalid_type(context, 1, arg_1, EXTRACT_TYPE(arg_0));
        return false;
    }

    *result = eq ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_neq(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    bool success = builtin_eq(vm, context, arg_count, args, result);
    if (!success) return false;

    *result = IS_NIL(*result) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool builtin_not(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    return builtin_is_nil(vm, context, arg_count, args, result);
}

static bool builtin_car(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg = EXTRACT_CAR(args);
    if (!IS_CONS(arg)) {
        eval_context_invalid_type(context, 0, arg, SEXPR_CONS);
        return false;
    }

    bool is_function =
        !IS_NIL(arg)
        && IS_SYMBOL(EXTRACT_CAR(arg))
        && s8_equals(EXTRACT_SYMBOL(EXTRACT_CAR(arg)), s8("'function"));
    if (is_function) {
        eval_context_invalid_type(context, 0, arg, SEXPR_CONS);
        return false;
    }

    *result = IS_NIL(arg) ? NIL : EXTRACT_CAR(arg);
    return true;
}

static bool builtin_cdr(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg = EXTRACT_CAR(args);
    if (!IS_CONS(arg)) {
        eval_context_invalid_type(context, 0, arg, SEXPR_CONS);
        return false;
    }

    bool is_function =
        !IS_NIL(arg)
        && IS_SYMBOL(EXTRACT_CAR(arg))
        && s8_equals(EXTRACT_SYMBOL(EXTRACT_CAR(arg)), s8("'function"));
    if (is_function) {
        eval_context_invalid_type(context, 0, arg, SEXPR_CONS);
        return false;
    }

    *result = IS_NIL(arg) ? NIL : EXTRACT_CDR(arg);
    return true;
}

static bool builtin_cons(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    *result =
        vm_alloc_cons(vm, EXTRACT_CAR(args), EXTRACT_CAR(EXTRACT_CDR(args)));
    return true;
}

static bool builtin_eval(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    return eval_internal(vm, context, EXTRACT_CAR(args), result);
}

static bool builtin_print(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    PRINT_SEXPR(EXTRACT_CAR(args)); printf("\n");
    *result = EXTRACT_CAR(args);
    return true;
}

static bool builtin_and(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &arg_1);

    SExpr* eval_result = NULL;
    bool success = false;
    if (!eval_internal(vm, context, arg_0, &eval_result)) {
        goto cleanup;
    }
    if (IS_NIL(eval_result)) {
        *result = NIL;
        goto cleanup;
    }

    success = eval_internal(vm, context, arg_1, &eval_result);
    if (success) *result = eval_result;

cleanup:
    VM_UNROOT(vm, &arg_1);
    VM_UNROOT(vm, &context);
    return success;
}

static bool builtin_or(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &arg_1);

    SExpr* eval_result = NULL;
    if (!eval_internal(vm, context, arg_0, &eval_result)) {
        *result = vm_alloc_symbol(vm, s8("t"));
        VM_UNROOT(vm, &arg_1);
        VM_UNROOT(vm, &context);
        return true;
    }

    VM_UNROOT(vm, &arg_1);
    VM_UNROOT(vm, &context);
    if (!IS_NIL(eval_result)) {
        *result = vm_alloc_symbol(vm, s8("t"));
        return true;
    }

    return eval_internal(vm, context, arg_1, result);
}

static bool builtin_if(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    SExpr* arg_2 = EXTRACT_CAR(EXTRACT_CDR(EXTRACT_CDR(args)));

    SExpr* eval_result = NULL;

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &arg_1);
    VM_ROOT(vm, &arg_2);
    bool success = eval_internal(vm, context, arg_0, &eval_result);
    VM_UNROOT(vm, &arg_1);
    VM_UNROOT(vm, &arg_2);
    VM_UNROOT(vm, &context);

    if (!success) return false;
    if (!IS_NIL(eval_result)) {
        success = eval_internal(vm, context, arg_1, &eval_result);
    } else {
        success = eval_internal(vm, context, arg_2, &eval_result);
    }

    if (success) *result = eval_result;
    return success;
}

static bool builtin_function(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    if (!IS_SYMBOL(EXTRACT_CAR(args))) {
        eval_context_invalid_type(context, 0, EXTRACT_CAR(args), SEXPR_SYMBOL);
        return false;
    }

    if (!env_lookup(&vm->funcs, EXTRACT_CAR(args), result)) {
        if (lookup_builtin(EXTRACT_SYMBOL(EXTRACT_CAR(args)), NULL)) {
            SExpr* struc = vm_alloc_cons(
                vm, 
                EXTRACT_CAR(args),
                vm_alloc_cons(vm, NIL, vm_alloc_cons(vm, NIL, NIL))
            );
            VM_ROOT(vm, &struc);
            *result =
                vm_alloc_cons(vm, vm_alloc_symbol(vm, s8("'function")), struc);
            VM_UNROOT(vm, &struc);
            return true;
        }

        eval_context_symbol_lookup_failed(context, EXTRACT_CAR(args));
        return false;
    }
    return true;
}

static bool builtin_lambda(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    if (!validate_function_def(context, args)) {
        return false;
    }

    VM_ROOT(vm, &args);

    SExpr* lambda_symbol = vm_alloc_symbol(vm, s8("lambda"));
    SExpr* id = vm_alloc_cons(vm, lambda_symbol, args);
    SExpr* tmp = vm_alloc_cons(vm, id, args);

    VM_UNROOT(vm, &args);

    VM_ROOT(vm, &tmp);
    SExpr* function_symbol = vm_alloc_symbol(vm, s8("'function"));
    VM_UNROOT(vm, &tmp);

    *result = vm_alloc_cons(vm, function_symbol, tmp);
    return true;
}

static bool builtin_let(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    eval_context_disable_local_env(context);

    SExpr* var_name = EXTRACT_CAR(args);
    SExpr* value = EXTRACT_CAR(EXTRACT_CDR(args));
    if (!IS_SYMBOL(var_name)) {
        eval_context_invalid_type(context, 0, var_name, SEXPR_SYMBOL);
        return false;
    }

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &var_name);
    VM_ROOT(vm, &value);

    bool success = false;
    SExpr* eval_result = NULL;
    if (!eval_internal(vm, context, value, &eval_result)) {
        goto cleanup;
    }

    eval_context_add_symbol(vm, context, var_name, eval_result);
    success = true;

cleanup:
    VM_UNROOT(vm, &value);
    VM_UNROOT(vm, &var_name);
    VM_UNROOT(vm, &context);
    return success;
}

static bool builtin_quote(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    *result = EXTRACT_CAR(args);
    return true;
}

static bool builtin_set(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    SExpr* var_name = EXTRACT_CAR(args);
    SExpr* value = EXTRACT_CAR(EXTRACT_CDR(args));
    if (!IS_SYMBOL(var_name)) {
        eval_context_invalid_type(context, 0, var_name, SEXPR_SYMBOL);
        return false;
    }

    VM_ROOT(vm, &var_name);
    VM_ROOT(vm, &value);

    bool success = false;
    SExpr* eval_result = NULL;
    if (!eval_internal(vm, context, value, &eval_result)) {
        goto cleanup;
    }

    env_set(vm, &vm->vars, var_name, eval_result);
    success = true;

cleanup:
    VM_UNROOT(vm, &value);
    VM_UNROOT(vm, &var_name);
    return success;
}

static bool builtin_begin(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    eval_context_disable_local_env(context);

    if (arg_count == 0) {
        eval_context_erronous_arg_count(context, 1);
        return false;
    }

    bool success = false;
    SExpr* arg_cons = args;

    VM_ROOT(vm, &context);
    VM_ROOT(vm, &arg_cons);
    while (!IS_NIL(arg_cons)) {
        success = eval_internal(vm, context, EXTRACT_CAR(arg_cons), result);
        if (!success) {
            goto cleanup;
        }

        arg_cons = EXTRACT_CDR(arg_cons);
    }

cleanup:
    VM_UNROOT(vm, &arg_cons);
    VM_UNROOT(vm, &context);
    return success;
}

static bool builtin_cond(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &args);

    size_t arg_index = 0;
    SExpr* eval_result = NULL;
    bool success = false;
    while (!IS_NIL(args)) {
        SExpr* pair = EXTRACT_CAR(args);
        if (IS_NIL(pair) || !IS_CONS(pair)) {
            eval_context_cond_arg_not_pair(context, arg_index, pair);
            goto cleanup;
        }
        if (IS_NIL(EXTRACT_CDR(pair)) || !IS_CONS(EXTRACT_CDR(pair))) {
            eval_context_cond_arg_not_pair(context, arg_index, pair);
            goto cleanup;
        }
        SExpr* arg_0 = EXTRACT_CAR(pair);

        if (!eval_internal(vm, context, arg_0, &eval_result)) {
            goto cleanup;
        }

        if (!IS_NIL(eval_result)) {
            success = eval_internal(
                vm,
                context,
                EXTRACT_CAR(EXTRACT_CDR(EXTRACT_CAR(args))),
                &eval_result
            );
            if (success) *result = eval_result;
            goto cleanup;
        }

        arg_index += 1;
        args = EXTRACT_CDR(args);
    }

    eval_context_illegal_call(context, args);
cleanup:
    VM_UNROOT(vm, &args);
    VM_UNROOT(vm, &context);
    return success;
}

static bool builtin_define(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    if (arg_count < 3) {
        eval_context_erronous_arg_count(context, 3);
        return false;
    }

    if (!IS_SYMBOL(EXTRACT_CAR(args))) {
        eval_context_invalid_type(context, 0, EXTRACT_CAR(args), SEXPR_SYMBOL);
        return false;
    }

    VM_ROOT(vm, &args);
    SExpr* body_begin = vm_alloc_symbol(vm, s8("begin"));
    SExpr* body =
        vm_alloc_cons(vm, body_begin, EXTRACT_CDR(EXTRACT_CDR(args)));
    SExpr* function_def = vm_alloc_cons(vm, body, NIL);
    function_def =
        vm_alloc_cons(vm, EXTRACT_CAR(EXTRACT_CDR(args)), function_def);

    if (!validate_function_def(context, function_def)) {
        return false;
    }

    function_def =
        vm_alloc_cons(vm, EXTRACT_CAR(args), function_def);

    VM_ROOT(vm, &function_def);
    SExpr* function_data_start = vm_alloc_symbol(vm, s8("'function"));
    VM_UNROOT(vm, &function_def);
    function_def = vm_alloc_cons(vm, function_data_start, function_def);
    VM_UNROOT(vm, &args);

    env_set(vm, &vm->funcs, EXTRACT_CAR(args), function_def);
    return true;
}

static bool builtin_funcall(
    Vm* vm,
    EvalContext* context,
    size_t arg_count,
    SExpr* args,
    SExpr** result
) {
    VM_ROOT(vm, &context);
    VM_ROOT(vm, &args);

    bool success = false;
    if (arg_count == 0) {
        eval_context_erronous_arg_count(context, 1);
        goto cleanup;
    }

    if (!IS_SYMBOL(EXTRACT_CAR(args))) {
        eval_context_invalid_type(context, 0, EXTRACT_CAR(args), SEXPR_SYMBOL);
        goto cleanup;
    }

    SExpr* func = NULL;
    if (!eval_context_lookup(vm, context, EXTRACT_CAR(args), &func)) {
        eval_context_symbol_lookup_failed(context, EXTRACT_CAR(args));
        goto cleanup;
    }

    bool is_function_struct =
        !IS_NIL(func)
        && IS_CONS(func)
        && IS_SYMBOL(EXTRACT_CAR(func))
        && s8_equals(EXTRACT_SYMBOL(EXTRACT_CAR(func)), s8("'function"));
    if (!is_function_struct) {
        eval_context_illegal_call(context, args);
        goto cleanup;
    }

    success = eval_func(
        vm,
        context,
        EXTRACT_CAR(EXTRACT_CDR(func)),
        EXTRACT_CDR(EXTRACT_CDR(func)),
        EXTRACT_CDR(args),
        result
    );

cleanup:
    VM_UNROOT(vm, &args);
    VM_UNROOT(vm, &context);
    return success;
}

BuiltinDef builtin_def_list[] = {
    DEFINE_BUILTIN("nil?", 1, builtin_is_nil),
    DEFINE_BUILTIN("symbol?", 1, builtin_is_symbol),
    DEFINE_BUILTIN("string?", 1, builtin_is_string),
    DEFINE_BUILTIN("number?", 1, builtin_is_number),
    DEFINE_BUILTIN("list?", 1, builtin_is_list),
    DEFINE_BUILTIN("sexp_to_bool", 1, builtin_sexp_to_bool),

    DEFINE_BUILTIN("add", 2, builtin_add),
    DEFINE_BUILTIN("sub", 2, builtin_sub),
    DEFINE_BUILTIN("mul", 2, builtin_mul),
    DEFINE_BUILTIN("div", 2, builtin_div),
    DEFINE_BUILTIN("mod", 2, builtin_mod),
    DEFINE_BUILTIN("+", 2, builtin_add),
    DEFINE_BUILTIN("-", 2, builtin_sub),
    DEFINE_BUILTIN("*", 2, builtin_mul),
    DEFINE_BUILTIN("/", 2, builtin_div),
    DEFINE_BUILTIN("%", 2, builtin_mod),

    DEFINE_BUILTIN("lt", 2, builtin_lt),
    DEFINE_BUILTIN("gt", 2, builtin_gt),
    DEFINE_BUILTIN("lte", 2, builtin_lte),
    DEFINE_BUILTIN("gte", 2, builtin_gte),
    DEFINE_BUILTIN("eq", 2, builtin_eq),
    DEFINE_BUILTIN("neq", 2, builtin_neq),
    DEFINE_BUILTIN("not", 1, builtin_not),
    DEFINE_BUILTIN("<", 2, builtin_lt),
    DEFINE_BUILTIN(">", 2, builtin_gt),
    DEFINE_BUILTIN("<=", 2, builtin_lte),
    DEFINE_BUILTIN(">=", 2, builtin_gte),
    DEFINE_BUILTIN("==", 2, builtin_eq),
    DEFINE_BUILTIN("!=", 2, builtin_neq),
    DEFINE_BUILTIN("!", 1, builtin_not),

    DEFINE_BUILTIN("car", 1, builtin_car),
    DEFINE_BUILTIN("cdr", 1, builtin_cdr),
    DEFINE_BUILTIN("cons", 2, builtin_cons),
    DEFINE_BUILTIN("eval", 1, builtin_eval),
    DEFINE_BUILTIN("print", 1, builtin_print),

    DEFINE_BUILTIN_NO_EVAL("and", 2, builtin_and),
    DEFINE_BUILTIN_NO_EVAL("or", 2, builtin_or),
    DEFINE_BUILTIN_NO_EVAL("if", 3, builtin_if),

    DEFINE_BUILTIN_NO_EVAL("function", 1, builtin_function),
    DEFINE_BUILTIN_NO_EVAL("lambda", 2, builtin_lambda),
    DEFINE_BUILTIN_NO_EVAL("let", 2, builtin_let),
    DEFINE_BUILTIN_NO_EVAL("quote", 1, builtin_quote),
    DEFINE_BUILTIN_NO_EVAL("set", 2, builtin_set),

    DEFINE_BUILTIN_NO_EVAL_VARIADIC("begin", builtin_begin),
    DEFINE_BUILTIN_NO_EVAL_VARIADIC("cond", builtin_cond),
    DEFINE_BUILTIN_NO_EVAL_VARIADIC("define", builtin_define),
    DEFINE_BUILTIN_NO_EVAL_VARIADIC("funcall", builtin_funcall),
};

bool lookup_builtin(s8 id, BuiltinDef* out) {
    for (size_t i = 0; i < countof(builtin_def_list); i++) {
        if (s8_equals(id, builtin_def_list[i].name)) {
            if (out != NULL) *out = builtin_def_list[i];
            return true;
        }
    }

    return false;
}
