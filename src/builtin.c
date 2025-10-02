#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval-context.h"
#include "parser.h"
#include "sexpr.h"
#include "util.h"
#include "vm.h"

#define DEFINE_BUILTIN(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, true, func }

#define DEFINE_BUILTIN_NO_EVAL(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, false, func }

#define DEFINE_BUILTIN_NO_EVAL_VARIADIC(name, func) \
    (BuiltinDef) { s8(name), true, 0, false, func }

static bool is_nil(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = IS_NIL(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool is_generic(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result,
    SExprType type
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = EXTRACT_TYPE(arg_0) == type 
        ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool is_symbol(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    return is_generic(vm, context, args, result, SEXPR_SYMBOL);
}

static bool is_string(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    return is_generic(vm, context, args, result, SEXPR_STRING);
}

static bool is_number(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    return is_generic(vm, context, args, result, SEXPR_NUMBER);
}

static bool is_list(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    return is_generic(vm, context, args, result, SEXPR_CONS);
}

static bool sexp_to_bool(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    *result = !IS_NIL(arg_0) ? vm_alloc_symbol(vm, s8("t")) : NIL;
    return true;
}

static bool cons(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_cons(vm, arg_0, arg_1);
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

static bool sexpr_add(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) + EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool sexpr_sub(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) - EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool sexpr_mul(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) * EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool sexpr_div(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
    if (!two_numbers(context, args)) return false;

    SExpr* arg_0 = EXTRACT_CAR(args);
    SExpr* arg_1 = EXTRACT_CAR(EXTRACT_CDR(args));
    *result = vm_alloc_number(
        vm,
        EXTRACT_NUMBER(arg_0) / EXTRACT_NUMBER(arg_1)
    );
    return true;
}

static bool sexpr_mod(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
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

static bool sexpr_lt(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
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

static bool sexpr_gt(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
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

static bool sexpr_lte(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
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

static bool sexpr_gte(Vm* vm, EvalContext* context, SExpr* args, SExpr** result) {
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

static bool sexpr_eq(
    Vm* vm,
    EvalContext* context,
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

static bool sexpr_not(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    return is_nil(vm, context, args, result);
}

static bool sexpr_car(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    if (!IS_CONS(arg_0)) {
        eval_context_invalid_type(context, 0, arg_0, SEXPR_CONS);
        return false;
    }

    *result = IS_NIL(arg_0) ? NIL : EXTRACT_CAR(arg_0);
    return true;
}

static bool sexpr_car(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    SExpr* arg_0 = EXTRACT_CAR(args);
    if (!IS_CONS(arg_0)) {
        eval_context_invalid_type(context, 0, arg_0, SEXPR_CONS);
        return false;
    }

    *result = IS_NIL(arg_0) ? NIL : EXTRACT_CDR(arg_0);
    return true;
}

static bool sexpr_quote(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    *result = EXTRACT_CAR(args);
    return true;
}

static bool sexpr_lambda(
    Vm* vm,
    EvalContext* context,
    SExpr* args,
    SExpr** result
) {
    VM_ROOT(vm, &args);
    SExpr* lambda = vm_alloc_symbol(vm, s8("lambda"));
    VM_UNROOT(vm, &args);
    *result = vm_alloc_cons(vm, lambda, args);
    return true;
}

BuiltinDef builtin_def_list[] = {
    DEFINE_BUILTIN("nil?", 1, is_nil),
    DEFINE_BUILTIN("symbol?", 1, is_symbol),
    DEFINE_BUILTIN("string?", 1, is_string),
    DEFINE_BUILTIN("number?", 1, is_number),
    DEFINE_BUILTIN("list?", 1, is_list),
    DEFINE_BUILTIN("sexp_to_bool", 1, sexp_to_bool),
    DEFINE_BUILTIN("cons", 2, cons),
    DEFINE_BUILTIN("add", 2, sexpr_add),
    DEFINE_BUILTIN("sub", 2, sexpr_sub),
    DEFINE_BUILTIN("mul", 2, sexpr_mul),
    DEFINE_BUILTIN("div", 2, sexpr_div),
    DEFINE_BUILTIN("mod", 2, sexpr_mod),
    DEFINE_BUILTIN("+", 2, sexpr_add),
    DEFINE_BUILTIN("-", 2, sexpr_sub),
    DEFINE_BUILTIN("*", 2, sexpr_mul),
    DEFINE_BUILTIN("/", 2, sexpr_div),
    DEFINE_BUILTIN("%", 2, sexpr_mod),
    DEFINE_BUILTIN("lt", 2, sexpr_lt),
    DEFINE_BUILTIN("gt", 2, sexpr_gt),
    DEFINE_BUILTIN("lte", 2, sexpr_lte),
    DEFINE_BUILTIN("gte", 2, sexpr_gte),
    DEFINE_BUILTIN("eq", 2, sexpr_eq),
    DEFINE_BUILTIN("not", 1, sexpr_not),
    DEFINE_BUILTIN("<", 2, sexpr_lt),
    DEFINE_BUILTIN(">", 2, sexpr_gt),
    DEFINE_BUILTIN("<=", 2, sexpr_lte),
    DEFINE_BUILTIN(">=", 2, sexpr_gte),
    DEFINE_BUILTIN("==", 2, sexpr_eq),
    DEFINE_BUILTIN("!", 1, sexpr_not),
    DEFINE_BUILTIN("car", 1, sexpr_car),
    DEFINE_BUILTIN("cdr", 1, sexpr_cdr),
    DEFINE_BUILTIN_NO_EVAL("quote", 1, sexpr_quote),
    DEFINE_BUILTIN_NO_EVAL("lambda", 2, sexpr_lambda),
};

size_t builtin_def_count = countof(builtin_def_list);
