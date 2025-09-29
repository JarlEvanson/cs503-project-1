#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "eval-context.h"
#include "gc.h"
#include "parse-context.h"
#include "sexpr.h"
#include "vm.h"

bool vm_init(Vm* vm) {
    if (!gc_init(&vm->gc)) {
        return false;
    }

    gc_add_type(
        &vm->gc,
        alignof(SExprSymbol),
        sexpr_symbol_size,
        sexpr_symbol_copy,
        sexpr_symbol_get_children
    );

    gc_add_type(
        &vm->gc,
        alignof(SExprString),
        sexpr_string_size,
        sexpr_string_copy,
        sexpr_string_get_children
    );

    gc_add_type(
        &vm->gc,
        alignof(SExprNumber),
        sexpr_number_size,
        sexpr_number_copy,
        sexpr_number_get_children
    );

    gc_add_type(
        &vm->gc,
        alignof(SExprCons),
        sexpr_cons_size,
        sexpr_cons_copy,
        sexpr_cons_get_children
    );

    gc_add_type(
        &vm->gc,
        alignof(ParseErrorNode),
        parse_context_size,
        parse_context_copy,
        parse_context_get_children
    );

    vm_add_support(vm);

    env_init(vm, &vm->vars);
    env_init(vm, &vm->funcs);
    return true;
}

void vm_free(Vm* vm) {
    env_free(vm, &vm->funcs);
    env_free(vm, &vm->vars);

    gc_free(&vm->gc);
}

SExpr* vm_alloc_symbol(Vm* vm, s8 symbol) {
    SExpr* sym = vm_alloc_symbol_with_length(vm, symbol.len);
    s8_copy(EXTRACT_SYMBOL(sym), symbol);
    return sym;
}

SExpr* vm_alloc_symbol_with_length(Vm* vm, size_t len) {
    size_t total_size = offsetof(SExprSymbol, bytes) + len;
    SExpr* sym = (SExpr*) gc_alloc(&vm->gc, SEXPR_SYMBOL, total_size);

    AS_SYMBOL(sym)->len = len;
    return sym;
}

SExpr* vm_alloc_string(Vm* vm, s8 string) {
    SExpr* str = vm_alloc_string_with_length(vm, string.len);
    s8_copy(EXTRACT_STRING(str), string);
    return str;
}

SExpr* vm_alloc_string_with_length(Vm* vm, size_t len) {
    size_t total_size = offsetof(SExprString, bytes) + len;
    SExpr* str = (SExpr*) gc_alloc(&vm->gc, SEXPR_STRING, total_size);

    AS_STRING(str)->len = len;
    return str;
}

SExpr* vm_alloc_number(Vm* vm, double number) {
    SExpr* num = (SExpr*) gc_alloc(&vm->gc, SEXPR_NUMBER, sizeof(SExprNumber));

    AS_NUMBER(num)->number = number;
    return num;
}

SExpr* vm_alloc_cons(Vm* vm, SExpr* car, SExpr* cdr) {
    VM_ROOT(vm, &car);
    VM_ROOT(vm, &cdr);

    SExpr* cons = (SExpr*) gc_alloc(&vm->gc, SEXPR_CONS, sizeof(SExprCons));

    VM_UNROOT(vm, &cdr);
    VM_UNROOT(vm, &car);

    AS_CONS(cons)->car = car;
    AS_CONS(cons)->cdr = cdr;
    return cons;
}

void env_init(Vm* vm, Environment* env) {
    SExpr* values = vm_alloc_cons(vm, NIL, NIL);
    env->list = vm_alloc_cons(vm, NIL, values);

    VM_ROOT(vm, &env->list);
}

void env_free(Vm* vm, Environment* env) {
    VM_UNROOT(vm, &env->list);
    env->list = NULL;
}

void env_set(Vm* vm, Environment* env, SExpr* symbol, SExpr* value) {
    VM_ROOT(vm, &symbol);

    SExpr* value_cons = vm_alloc_cons(
        vm,
        value,
        EXTRACT_CAR(EXTRACT_CDR(env->list))
    );

    VM_UNROOT(vm, &symbol);
    VM_ROOT(vm, &value_cons);

    SExpr* symbol_cons = vm_alloc_cons(
        vm,
        symbol,
        EXTRACT_CAR(env->list)
    );

    VM_UNROOT(vm, &value_cons);

    AS_CONS(env->list)->car = symbol_cons;
    AS_CONS(AS_CONS(env->list)->cdr)->car = value_cons;
}

bool env_lookup(Environment* env, SExpr* symbol, SExpr** value) {
    SExpr* symbols = EXTRACT_CAR(env->list);
    SExpr* values = EXTRACT_CAR(EXTRACT_CDR(env->list));

    while (!IS_NIL(symbols)) {
        SExpr* test_symbol = EXTRACT_CAR(symbols);
        if (s8_equals(EXTRACT_SYMBOL(test_symbol), EXTRACT_SYMBOL(symbol))) {
            *value = EXTRACT_CAR(values);
            return true;
        }

        symbols = EXTRACT_CDR(symbols);
        values = EXTRACT_CDR(values);
    }

    return false;
}

#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

bool vm_env_set_lookup_basic() {
    Vm vm;
    if (!vm_init(&vm)) {
        return false;
    }

    bool result = false;

    SExpr* symbol = NULL;
    VM_ROOT(&vm, &symbol);

    SExpr* value = NULL;
    VM_ROOT(&vm, &value);

    symbol = vm_alloc_symbol(&vm, s8("test"));
    value = vm_alloc_number(&vm, 1.0);

    env_set(&vm, &vm.vars, symbol, value);

    bool found = env_lookup(&vm.vars, symbol, &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 1.0) {
        goto cleanup;
    }

    result = true;
cleanup:
    vm_free(&vm);
    return result;
}

bool vm_env_set_lookup_override() {
    Vm vm;
    if (!vm_init(&vm)) {
        return false;
    }

    bool result = false;

    SExpr* symbol = NULL;
    VM_ROOT(&vm, &symbol);

    SExpr* value = NULL;
    VM_ROOT(&vm, &value);

    symbol = vm_alloc_symbol(&vm, s8("test"));
    value = vm_alloc_number(&vm, 1.0);

    env_set(&vm, &vm.vars, symbol, value);

    bool found = env_lookup(&vm.vars, symbol, &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 1.0) {
        goto cleanup;
    }

    value = vm_alloc_number(&vm, 2.0);

    env_set(&vm, &vm.vars, symbol, value);

    found = env_lookup(&vm.vars, symbol, &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 2.0) {
        goto cleanup;
    }

    result = true;
cleanup:
    vm_free(&vm);
    return result;
}

bool vm_env_set_lookup_multi_support() {
    Vm vm;
    if (!vm_init(&vm)) {
        return false;
    }

    bool result = false;

    SExpr* symbol = NULL;
    VM_ROOT(&vm, &symbol);

    SExpr* value = NULL;
    VM_ROOT(&vm, &value);

    symbol = vm_alloc_symbol(&vm, s8("test"));
    value = vm_alloc_number(&vm, 1.0);

    env_set(&vm, &vm.vars, symbol, value);

    bool found = env_lookup(&vm.vars, symbol, &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 1.0) {
        goto cleanup;
    }

    symbol = vm_alloc_symbol(&vm, s8("toads"));
    value = vm_alloc_number(&vm, 2.0);

    env_set(&vm, &vm.vars, symbol, value);

    found = env_lookup(&vm.vars, symbol, &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 2.0) {
        goto cleanup;
    }

    found = env_lookup(&vm.vars, vm_alloc_symbol(&vm, s8("test")), &value);
    if (!found || !IS_NUMBER(value) || AS_NUMBER(value)->number != 1.0) {
        goto cleanup;
    }

    result = true;
cleanup:
    vm_free(&vm);
    return result;
}

TestDefinition vm_tests[] = {
    DEFINE_TEST(vm_env_set_lookup_basic, 5),
    DEFINE_TEST(vm_env_set_lookup_override, 5),
    DEFINE_TEST(vm_env_set_lookup_multi_support, 5),
};

TestList vm_test_list = (TestList) {
    vm_tests,
    countof(vm_tests)
};

#endif
