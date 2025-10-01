#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "common.h"
#include "gc.h"
#include "lexer.h"
#include "parse-context.h"
#include "parser.h"
#include "s8.h"
#include "test.h"
#include "util.h"
#include "vm.h"

int test_definition_compare(const void* obj_0, const void* obj_1) {
    TestDefinition* t_0 = (TestDefinition*) obj_0;
    TestDefinition* t_1 = (TestDefinition*) obj_1;

    if (t_0->sprint < t_1->sprint) {
        return -1;
    } else if (t_0->sprint > t_1->sprint) {
        return 1;
    }

    return strcmp(t_0->name, t_1->name);
}

TestList acquire_unit_tests() {
    TestList unit_test_lists[] = {
        arena_test_list,
        gc_test_list,
        lexer_test_list,
        parse_context_test_list,
        parser_test_list,
        s8_test_list,
        vm_test_list,
    };
    size_t unit_test_list_count = countof(unit_test_lists);

    size_t unit_test_count = 0;
    for (size_t i = 0; i < unit_test_list_count; i++) {
        unit_test_count += unit_test_lists[i].test_count;
    }

    TestList list;
    list.tests =
        (TestDefinition*) calloc(unit_test_count, sizeof(TestDefinition));
    list.test_count = unit_test_count;

    size_t index = 0;
    for (size_t l_index = 0; l_index < unit_test_list_count; l_index++) {
        TestList unit_test_list = unit_test_lists[l_index];
        for (
            size_t t_index = 0;
            t_index < unit_test_list.test_count;
            t_index++
        ) {
            list.tests[index] = unit_test_list.tests[t_index];
            index += 1;
        }
    }

    return list;
}

TestList acquire_integration_tests() {
    TestList tests;

    tests.tests = NULL;
    tests.test_count = 0;

    return tests;
}

int main() {
    TestList unit_tests = acquire_unit_tests();
    TestList integration_tests = acquire_integration_tests();

    TestList tests;
    tests.test_count = unit_tests.test_count + integration_tests.test_count;
    tests.tests = 
        (TestDefinition*) calloc(tests.test_count, sizeof(TestDefinition));

    size_t index = 0;
    for (size_t i = 0; i < unit_tests.test_count; i++) {
        tests.tests[index] = unit_tests.tests[i];
        index += 1;
    }
    for (size_t i = 0; i < integration_tests.test_count; i++) {
        tests.tests[index] = integration_tests.tests[i];
        index += 1;
    }
    free(unit_tests.tests);
    free(integration_tests.tests);

    qsort(
        tests.tests,
        tests.test_count,
        sizeof(TestDefinition),
        test_definition_compare
    );
    
    index = 0;
    size_t current_sprint = 0;
    size_t successes = 0;
    size_t failures = 0;
    while (index < tests.test_count) {
        TestDefinition test = tests.tests[index];
        if (test.sprint != current_sprint || index == 0) {
            if (test.sprint == 0) {
                printf("General Unit Tests\n");
            } else {
                printf("Sprint %zu\n", test.sprint);
            }
            printf("----------------------------------------\n");
            current_sprint = test.sprint;
        }

        bool result;
        if (test.is_unit_test) {
            result = test.test.unit_test();
        } else {
            UNREACHABLE();
        }

        printf("%s: %s\n", result ? "✅" : "❌", test.name);
        *(result ? &successes : &failures) += 1;

        index += 1;
        if (
            index < tests.test_count
            && tests.tests[index].sprint != current_sprint
        ) {
            printf("\n");
        }
    }

    printf(
        "test results: %s. %zu passed; %zu failed\n",
        failures == 0 ? "ok" : "FAILED",
        successes,
        failures
    );

    free(tests.tests);
    return EXIT_SUCCESS;
}
