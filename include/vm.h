#ifndef LISP_VM_H
#define LISP_VM_H

#include "common.h"
#include "gc.h"
#include "s8.h"
#include "sexpr.h"

typedef struct {
    SExpr* list;
} Environment;

typedef struct {
    Gc gc;

    Environment vars;
    Environment funcs;
} Vm;

bool vm_init(Vm* vm);
void vm_free(Vm* vm);

#define VM_ROOT(vm, object) GC_ROOT(&(vm)->gc, (object))
#define VM_UNROOT(vm, object) GC_UNROOT(&(vm)->gc, (object))

SExpr* vm_alloc_symbol(Vm* vm, s8 symbol);
SExpr* vm_alloc_symbol_with_length(Vm* vm, size_t len);
SExpr* vm_alloc_string(Vm* vm, s8 string);
SExpr* vm_alloc_string_with_length(Vm* vm, size_t len);
SExpr* vm_alloc_number(Vm* vm, double number);
SExpr* vm_alloc_cons(Vm* vm, SExpr* car, SExpr* cdr);

void env_init(Vm* vm, Environment* env);
void env_free(Environment* env);

void env_set(Vm* vm, Environment* env, SExpr* symbol, SExpr* value);
bool env_lookup(Environment* env, SExpr* symbol, SExpr** value);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList vm_test_list;

#endif

#endif
