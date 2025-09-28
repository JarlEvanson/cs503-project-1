#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "gc.h"
#include "parse-context.h"
#include "parser.h"
#include "vm.h"

#define PARSE_ERROR_NODE_VM_TYPE_ID 4

void parse_context_print(ParseContext context, Parser* parser) {
    ParseErrorNode* current = context;
    while (current != NULL) {
        switch (current->type) {
            case INVALID_ESCAPE:
                printf("invalid escape");
                break;
            case INVALID_SUFFIX:
                printf("invalid suffix");
                break;
            case INVALID_UTF8:
                printf("invalid UTF-8");
                break;
            case MISSING_SEXPR:
                printf("missing S-Expression");
                break;
            case UNTERMINATED_LIST:
                printf("unterminated list");
                break;
            case UNTERMINATED_STRING:
                printf("unterminated string");
                break;
        }

        printf(
            ": (%zu-%zu) `",
            current->index,
            current->index + current->length
        );
        fwrite(
            &parser->lexer.input[current->index],
            current->length,
            1,
            stdout
        );
        printf("`\n");

        current = current->next;
    }
}

size_t parse_context_error_count(ParseContext context) {
    size_t count = 0;

    ParseErrorNode* current = context;
    while (current != NULL) {
        count += 1;
        current = current->next;
    }

    return count;
}

void parse_context_add_error(
    Vm* vm,
    ParseContext* context,
    ParseErrorType type,
    size_t index,
    size_t length
) {
    VM_ROOT(vm, context);

    ParseErrorNode* new_error = (ParseErrorNode*) gc_alloc(
        &vm->gc,
        PARSE_ERROR_NODE_VM_TYPE_ID,
        sizeof(ParseErrorNode)
    );

    new_error->type = type;
    new_error->index = index;
    new_error->length = length;
    new_error->next = NULL;

    ParseErrorNode** current = context;
    while (*current != NULL) current = &(*current)->next;
    *current = new_error;

    VM_UNROOT(vm, context);
}

size_t parse_context_size(GcObject* object) {
    return sizeof(ParseErrorNode);
}

void parse_context_copy(Gc* gc, GcObject* object, GcObject* new_object) {
    ((ParseErrorNode*) new_object)->type = ((ParseErrorNode*) object)->type;
    ((ParseErrorNode*) new_object)->index = ((ParseErrorNode*) object)->index;
    ((ParseErrorNode*) new_object)->length =
        ((ParseErrorNode*) object)->length;

    ParseErrorNode* next = ((ParseErrorNode*) object)->next;
    if (next == NULL) {
        ((ParseErrorNode*) new_object)->next = NULL;
    } else {
        ((ParseErrorNode*) new_object)->next = (ParseErrorNode*)
            gc_copy_object(
                gc,
                (GcObject*) next
            );
    }
}

GcObject* parse_context_get_children(GcObject* object, GcObject* position) {
    GcObject* child = (GcObject*) ((ParseErrorNode*) object)->next;

    if (position == NULL) return child;
    return NULL;
}

#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

static bool parse_context_chaining() {
    Vm vm;
    if (!vm_init(&vm)) return false;

    bool result = false;

    ParseContext context = NULL;
    parse_context_add_error(
        &vm,
        &context,
        INVALID_ESCAPE,
        0,
        1
    );

    parse_context_add_error(
        &vm,
        &context,
        INVALID_UTF8,
        1,
        2
    );

    parse_context_add_error(
        &vm,
        &context,
        INVALID_SUFFIX,
        2,
        3
    );

    if (context == NULL) goto cleanup;

    result = true;
cleanup:
    vm_free(&vm);
    return result;
}

static bool parse_context_counting() {
    Vm vm;
    if (!vm_init(&vm)) return false;

    bool result = false;

    ParseContext context = NULL;
    parse_context_add_error(
        &vm,
        &context,
        INVALID_ESCAPE,
        0,
        1
    );

    parse_context_add_error(
        &vm,
        &context,
        INVALID_UTF8,
        1,
        2
    );

    parse_context_add_error(
        &vm,
        &context,
        INVALID_SUFFIX,
        2,
        3
    );

    if (parse_context_error_count(context) != 3) goto cleanup;

    result = true;
cleanup:
    vm_free(&vm);
    return result;
}

TestDefinition parse_context_tests[] = {
    DEFINE_TEST(parse_context_chaining, 0),
    DEFINE_TEST(parse_context_counting, 0),
};

TestList parse_context_test_list = (TestList) {
    parse_context_tests,
    countof(parse_context_tests)
};

#endif
