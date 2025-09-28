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

int main() {
    TestList test_lists[] = {
        arena_test_list,
        gc_test_list,
        lexer_test_list,
        parse_context_test_list,
        parser_test_list,
        s8_test_list,
        vm_test_list,
    };
    size_t test_list_count = countof(test_lists);
    
    size_t test_count = 0;
    for (size_t i = 0; i < test_list_count; i++) {
        test_count += test_lists[i].test_count;
    }

    TestDefinition* tests =
        (TestDefinition*) calloc(test_count, sizeof(TestDefinition));
    size_t index = 0;
    for (size_t l_index = 0; l_index < test_list_count; l_index++) {
        TestList list = test_lists[l_index];
        for (size_t t_index = 0; t_index < list.test_count; t_index++) {
            tests[index] = list.tests[t_index];
            index += 1;
        }
    }

    qsort(tests, test_count, sizeof(TestDefinition), test_definition_compare);
    
    index = 0;
    size_t current_sprint = 0;
    size_t successes = 0;
    size_t failures = 0;
    while (index < test_count) {
        TestDefinition test = tests[index];
        if (test.sprint != current_sprint || index == 0) {
            if (test.sprint == 0) {
                printf("General Unit Tests\n");
            } else {
                printf("Sprint %zu\n", test.sprint);
            }
            printf("----------------------------------------\n");
            current_sprint = test.sprint;
        }

        bool result = test.test();
        printf("%s: %s\n", result ? "✅" : "❌", test.name);
        *(result ? &successes : &failures) += 1;

        index += 1;
        if (index < test_count && tests[index].sprint != current_sprint) {
            printf("\n");
        }
    }

    printf(
        "test results: %s. %zu passed; %zu failed\n",
        failures == 0 ? "ok" : "FAILED",
        successes,
        failures
    );

    free(tests);
    return EXIT_SUCCESS;
}
