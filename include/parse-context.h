#ifndef LISP_PARSE_CONTEXT_H
#define LISP_PARSE_CONTEXT_H

#include "common.h"
#include "gc.h"
#include "vm.h"

typedef enum {
    INVALID_ESCAPE,
    INVALID_SUFFIX,
    INVALID_UTF8,
    MISSING_SEXPR,
    UNTERMINATED_LIST,
    UNTERMINATED_STRING,
} ParseErrorType;

typedef struct ParseErrorNode ParseErrorNode;
struct ParseErrorNode {
    GcObject object;

    ParseErrorType type;
    size_t index;
    size_t length;

    ParseErrorNode* next;
};

typedef ParseErrorNode* ParseContext;

size_t parse_context_error_count(ParseContext context);
void parse_context_add_error(
    Vm* vm,
    ParseContext* context,
    ParseErrorType type,
    size_t index,
    size_t length
);

size_t parse_context_size(GcObject* object);
void parse_context_copy(Gc* gc, GcObject* object, GcObject* new_object);
GcObject* parse_context_get_children(GcObject* object, GcObject* position);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList parse_context_test_list;

#endif

#endif
