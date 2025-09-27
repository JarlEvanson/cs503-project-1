#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "gc.h"
#include "s8.h"
#include "sexpr.h"

SExprType sexpr_extract_type(const SExpr* sexpr) {
    if (IS_NIL(sexpr)) return SEXPR_CONS;
    size_t type_id = gc_object_type((GcObject*) sexpr);
    ASSERT(
        type_id == SEXPR_SYMBOL
        || type_id == SEXPR_STRING
        || type_id == SEXPR_NUMBER
        || type_id == SEXPR_CONS,
        "invalid type id associated with sexpr"
    );

    return (SExprType) type_id;
}

const SExpr* sexpr_check_cast(const SExpr* sexpr, SExprType type) {
    ASSERT(EXTRACT_TYPE(sexpr) == type, "invalid SExpr cast");
    return sexpr;
}

s8 sexpr_s8(const SExpr* sexpr) {
    s8 s;

    switch (EXTRACT_TYPE(sexpr)) {
        case SEXPR_SYMBOL:
            s.ptr = ((SExprSymbol*) sexpr)->bytes;
            s.len = ((SExprSymbol*) sexpr)->len;
            return s;
        case SEXPR_STRING:
            s.ptr = ((SExprSymbol*) sexpr)->bytes;
            s.len = ((SExprSymbol*) sexpr)->len;
            return s;
        case SEXPR_NUMBER:
        case SEXPR_CONS:
            break;
    }

    UNREACHABLE("invalid SExpr from which to extract an s8");
}

void sexpr_print(const SExpr* sexpr) {
    s8 s;

    switch (EXTRACT_TYPE(sexpr)) {
        case SEXPR_SYMBOL:
            s = EXTRACT_SYMBOL(sexpr);
            fwrite(s.ptr, s.len, sizeof(uint8_t), stdout);
            break;
        case SEXPR_STRING:
            s = EXTRACT_STRING(sexpr);
            fwrite(s.ptr, s.len, sizeof(uint8_t), stdout);
            break;
        case SEXPR_NUMBER:
            printf("%f", EXTRACT_NUMBER(sexpr));
            break;
        case SEXPR_CONS:
            printf("(");

            while (!IS_NIL(sexpr)) {
                sexpr_print(EXTRACT_CAR(sexpr));

                if (!IS_CONS(EXTRACT_CDR(sexpr))) {
                    printf(" . ");
                    PRINT_SEXPR(EXTRACT_CDR(sexpr));
                    break;
                }

                if (!IS_NIL(EXTRACT_CDR(sexpr))) {
                    printf(" ");
                }

                sexpr = EXTRACT_CDR(sexpr);
            }

            printf(")");
            break;
    }
}

void print_tabs(size_t tab_count) {
    for (size_t i = 0; i < tab_count; i++) {
        printf("\t");
    }
}

void print_escaped_s8(s8 s) {
    fwrite(s.ptr, s.len, 1, stdout);
}

void sexpr_print_raw(const SExpr* sexpr, size_t tab_count) {
    if (IS_NIL(sexpr)) {
        printf("NIL");
        return;
    }

    // Type
    printf("SExpr {\n");
    print_tabs(tab_count + 1);
    switch (EXTRACT_TYPE(sexpr)) {
        case SEXPR_SYMBOL:
            printf("type: SYMBOL\n");
            break;
        case SEXPR_STRING:
            printf("type: STRING\n");
            break;
        case SEXPR_NUMBER:
            printf("type: NUMBER\n");
            break;
        case SEXPR_CONS:
            printf("type: CONS\n");
            break;
    }

    // Forward pointer
    print_tabs(tab_count + 1);
    printf("forward_ptr: %p\n", sexpr->object.forward_ptr);

    // Type specific information
    print_tabs(tab_count + 1);
    switch (EXTRACT_TYPE(sexpr)) {
        case SEXPR_SYMBOL:
            printf("symbol: ");
            print_escaped_s8(EXTRACT_SYMBOL(sexpr));
            break;
        case SEXPR_STRING:
            printf("string: \"");
            print_escaped_s8(EXTRACT_STRING(sexpr));
            printf("\"");
            break;
        case SEXPR_NUMBER:
            printf("number: %f", EXTRACT_NUMBER(sexpr));
            break;
        case SEXPR_CONS:
            printf("car: %p ", EXTRACT_CAR(sexpr));
            sexpr_print_raw(EXTRACT_CAR(sexpr), tab_count + 1);
            printf("\n");

            print_tabs(tab_count + 1);
            printf("cdr: %p ", EXTRACT_CDR(sexpr));
            sexpr_print_raw(EXTRACT_CDR(sexpr), tab_count + 1);
    }
    printf("\n");
    print_tabs(tab_count);
    printf("}");
}

size_t sexpr_symbol_size(GcObject* object) {
    return offsetof(SExprSymbol, bytes) + AS_SYMBOL(object)->len;
}

void sexpr_symbol_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    AS_SYMBOL(new_object)->len = AS_SYMBOL(object)->len;
    s8_copy(EXTRACT_SYMBOL(new_object), EXTRACT_SYMBOL(object));
}

GcObject* sexpr_symbol_get_children(GcObject* object, GcObject* position) {
    return NULL;
}

size_t sexpr_string_size(GcObject* object) {
    return offsetof(SExprString, bytes) + AS_STRING(object)->len;
}

void sexpr_string_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    AS_STRING(new_object)->len = AS_STRING(object)->len;
    s8_copy(EXTRACT_STRING(new_object), EXTRACT_STRING(object));
}

GcObject* sexpr_string_get_children(GcObject* object, GcObject* position) {
    return NULL;
}

size_t sexpr_number_size(GcObject* object) {
    return sizeof(SExprNumber);
}

void sexpr_number_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    AS_NUMBER(new_object)->number = EXTRACT_NUMBER(object);
}

GcObject* sexpr_number_get_children(GcObject* object, GcObject* position) {
    return NULL;
}

size_t sexpr_cons_size(GcObject* object) {
    return sizeof(SExprCons);
}

void sexpr_cons_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    if (!IS_NIL(EXTRACT_CAR(object))) {
        AS_CONS(new_object)->car =
            (SExpr*) gc_copy_object(gc, (GcObject*) EXTRACT_CAR(object));
    } else {
        AS_CONS(new_object)->car = NIL;
    }

    if (!IS_NIL(EXTRACT_CDR(object))) {
        AS_CONS(new_object)->cdr =
            (SExpr*) gc_copy_object(gc, (GcObject*) EXTRACT_CDR(object));
    } else {
        AS_CONS(new_object)->cdr = NIL;
    }
}

GcObject* sexpr_cons_get_children(GcObject* object, GcObject* position) {
    GcObject* car = (GcObject*) EXTRACT_CAR(object);
    GcObject* cdr = (GcObject*) EXTRACT_CDR(object);
    if (position == NULL) {
        if (car != NULL) return car;
        if (cdr != NULL) return cdr;
    } else if (position == car) {
        if (cdr != NULL) return cdr;
    }
    
    return NULL;
}
