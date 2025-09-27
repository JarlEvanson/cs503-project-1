#ifndef LISP_SEXPR_H
#define LISP_SEXPR_H

#include "common.h"
#include "gc.h"
#include "s8.h"

typedef enum {
    SEXPR_SYMBOL,
    SEXPR_STRING,
    SEXPR_NUMBER,
    SEXPR_CONS,
} SExprType;

typedef struct SExpr {
    GcObject object;
} SExpr;

typedef struct {
    SExpr header;
    size_t len;
    uint8_t bytes[];
} SExprSymbol;

typedef struct {
    SExpr header;
    size_t len;
    uint8_t bytes[];
} SExprString;

typedef struct {
    SExpr header;
    double number;
} SExprNumber;

typedef struct {
    SExpr header;
    SExpr* car;
    SExpr* cdr;
} SExprCons;

#define NIL ((SExpr*) NULL)

#define AS_SYMBOL(sexpr) \
    ((SExprSymbol*) sexpr_check_cast((SExpr*) (sexpr), SEXPR_SYMBOL))
#define AS_STRING(sexpr) \
    ((SExprString*) sexpr_check_cast((SExpr*) (sexpr), SEXPR_STRING))
#define AS_NUMBER(sexpr) \
    ((SExprNumber*) sexpr_check_cast((SExpr*) (sexpr), SEXPR_NUMBER))
#define AS_CONS(sexpr) \
    ((SExprCons*) sexpr_check_cast((SExpr*) (sexpr), SEXPR_CONS))

#define IS_NIL(sexpr) ((sexpr) == NIL)
#define IS_SYMBOL(sexpr) (sexpr_extract_type((SExpr*) (sexpr)) == SEXPR_SYMBOL)
#define IS_STRING(sexpr) (sexpr_extract_type((SExpr*) (sexpr)) == SEXPR_STRING)
#define IS_NUMBER(sexpr) (sexpr_extract_type((SExpr*) (sexpr)) == SEXPR_NUMBER)
#define IS_CONS(sexpr) (sexpr_extract_type((SExpr*) (sexpr)) == SEXPR_CONS)

#define EXTRACT_TYPE(sexpr) sexpr_extract_type((SExpr*) (sexpr))
#define EXTRACT_SYMBOL(sexpr) sexpr_s8((SExpr*) AS_SYMBOL(sexpr))
#define EXTRACT_STRING(sexpr) sexpr_s8((SExpr*) AS_STRING(sexpr))
#define EXTRACT_NUMBER(sexpr) (AS_NUMBER(sexpr)->number)
#define EXTRACT_CAR(sexpr) (AS_CONS(sexpr)->car)
#define EXTRACT_CDR(sexpr) (AS_CONS(sexpr)->cdr)

#define PRINT_SEXPR(sexpr) sexpr_print((SExpr*) (sexpr))

SExprType sexpr_extract_type(const SExpr* sexpr);
const SExpr* sexpr_check_cast(const SExpr* sexpr, SExprType type);
s8 sexpr_s8(const SExpr* sexpr);

void sexpr_print(const SExpr* sexpr);

size_t sexpr_symbol_size(GcObject* object);
void sexpr_symbol_copy(Gc* gc, GcObject* object, GcObject* new_object);
GcObject* sexpr_symbol_get_children(GcObject* object, GcObject* position);

size_t sexpr_string_size(GcObject* object);
void sexpr_string_copy(Gc* gc, GcObject* object, GcObject* new_object);
GcObject* sexpr_string_get_children(GcObject* object, GcObject* position);

size_t sexpr_number_size(GcObject* object);
void sexpr_number_copy(Gc* gc, GcObject* object, GcObject* new_object);
GcObject* sexpr_number_get_children(GcObject* object, GcObject* position);

size_t sexpr_cons_size(GcObject* object);
void sexpr_cons_copy(Gc* gc, GcObject* object, GcObject* new_object);
GcObject* sexpr_cons_get_children(GcObject* object, GcObject* position);

#endif
