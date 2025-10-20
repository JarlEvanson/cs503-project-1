#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "common.h"
#include "eval-context.h"
#include "eval.h"
#include "gc.h"
#include "lexer.h"
#include "parse-context.h"
#include "parser.h"
#include "s8.h"
#include "sexpr.h"
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
        eval_context_test_list,
        eval_test_list,
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

void acquire_integration_test_data(char* path, s8* data) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s was not found\n", path);
        exit(EXIT_FAILURE);
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "file seeking failed\n");
        exit(EXIT_FAILURE);
    }

    long file_size = ftell(file);
    if (file_size == -1L) {
        fprintf(stderr, "file tell failed\n");
        exit(EXIT_FAILURE);
    }

    rewind(file);
    data->ptr = calloc(file_size, sizeof(char));
    data->len = (size_t) file_size;

    size_t bytes_read = fread(data->ptr, 1, data->len, file);
    if (bytes_read != data->len) {
        fprintf(stderr, "fread failed\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

void acquire_integration_test(char* path, s8* input, s8* output) {
    size_t end = strlen(path) - 1;
    path[end] = '.';
    path[end + 2] = '\0';

    path[end + 1] = 'i';
    acquire_integration_test_data(path, input);

    path[end + 1] = 'o';
    acquire_integration_test_data(path, output);
}

TestList acquire_integration_tests() {
    TestList tests;
    tests.tests = NULL;
    tests.test_count = 0;

    FILE* index_files[10] = { 0 };
    size_t sprint_test_count[countof(index_files)] = { 0 };
    char path[24] = { 0 };
    memcpy(path, "test/sprint-0/index.txt", 24);

    char* buffer = calloc(1, 4096 * 4);
    if (buffer == NULL) {
        fprintf(stderr, "memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < countof(index_files); i++) {
        path[12] = i + '0';
        index_files[i] = fopen(path, "rb");
        if (index_files[i] == NULL) continue;
        if (fgets(buffer, 4096 * 4 - 2, index_files[i]) == NULL)
            continue;

        do {
            int items_read =
                sscanf(buffer, "%*s");
            if (items_read != 0) {
                fprintf(stderr, "invalid index file format\n");
                exit(EXIT_FAILURE);
            }
            if (buffer[strlen(buffer) - 1] != '\n') {
                fprintf(stderr, "invalid index file format\n");
                exit(EXIT_FAILURE);
            }
            sprint_test_count[i] += 1;
        } while (fgets(buffer, 4096 * 4, index_files[i]) != NULL);
        if (tests.test_count + sprint_test_count[i] < tests.test_count) {
            fprintf(stderr, "test count overflow\n");
            exit(EXIT_FAILURE);
        }

        tests.test_count += sprint_test_count[i];
    }

    tests.tests =
        (TestDefinition*) calloc(tests.test_count, sizeof(TestDefinition));

    size_t test_index = 0;
    for (size_t f = 0; f < countof(index_files); f++) {
        if (index_files[f] == NULL) continue;
        memcpy(buffer, "test/sprint-0/", 14);
        buffer[12] = f + '0';

        rewind(index_files[f]);
        while (fgets(&buffer[0] + 14, 4096 * 4 - 14, index_files[f]) != NULL) {
            size_t test_name_len = strlen(buffer) - 14;
            tests.tests[test_index].name = (char*) calloc(test_name_len, 1);
            tests.tests[test_index].name = memcpy(
                (char*) tests.tests[test_index].name,
                &buffer[14],
                test_name_len - 1
            );
            tests.tests[test_index].sprint = f;

            tests.tests[test_index].is_unit_test = false;

            s8 input, output;
            acquire_integration_test(buffer, &input, &output);

            tests.tests[test_index].test.input = input.ptr;
            tests.tests[test_index].test.input_len = input.len;

            tests.tests[test_index].test.output = output.ptr;
            tests.tests[test_index].test.output_len = output.len;

            test_index += 1;
        }
    }

    for (size_t i = 0; i < countof(index_files); i++) {
        if (index_files[i] == NULL) continue;

        fclose(index_files[i]);
    }
    if (test_index != tests.test_count) {
        fprintf(stderr, "error: non-matching counts\n");
        exit(EXIT_FAILURE);
    }
    free(buffer);
    return tests;
}

bool sexpr_eq(SExpr* a, SExpr* b) {
    if (IS_NIL(a) && IS_NIL(b)) return true;
    if (IS_NIL(a) || IS_NIL(b)) return false;

    switch (EXTRACT_TYPE(a)) {
        case SEXPR_SYMBOL:
            return IS_SYMBOL(b) && s8_equals(EXTRACT_SYMBOL(a), EXTRACT_SYMBOL(b));
        case SEXPR_STRING:
            return IS_STRING(b) && s8_equals(EXTRACT_STRING(a), EXTRACT_STRING(b));
        case SEXPR_NUMBER:
            if (!IS_NUMBER(b)) return false;
            double val_0 = EXTRACT_NUMBER(a);
            double val_1 = EXTRACT_NUMBER(b);
            if (val_0 == val_1) return true;

            double precision = val_0 * val_1 * 0.000001;
            return fabs(val_0 - val_1) < precision;
        case SEXPR_CONS:
            return IS_CONS(b)
                && sexpr_eq(EXTRACT_CAR(a), EXTRACT_CAR(b))
                && sexpr_eq(EXTRACT_CDR(a), EXTRACT_CDR(b));
    }

    UNREACHABLE();
}

bool run_integration_test(Vm* vm, s8 input, s8 output) {
    Parser input_parser;
    Parser output_parser;

    parser_init_s8(&input_parser, input);
    parser_init_s8(&output_parser, output);

    bool result = false;

    size_t root_count = vm->gc.root_count;
    ParseResult input_parse;
    ParseResult output_parse;
    while (parser_next_sexpr(vm, &input_parser, &input_parse)) {
        ASSERT(root_count == vm->gc.root_count);
        if (!input_parse.ok) {
            fprintf(stderr, "input parsing failed\n");
            goto cleanup;
        }
        VM_ROOT(vm, &input_parse.as.ok);
        EvalResult input_result = eval(vm, input_parse.as.ok);
        if (!input_result.ok) {
            fprintf(stderr, "input evaluation failed\n");
            PRINT_SEXPR_RAW(input_parse.as.ok, 0); printf("\n");
            VM_UNROOT(vm, &input_parse.as.ok);
            eval_context_print(input_result.as.err);
            goto cleanup;
        }

        VM_UNROOT(vm, &input_parse.as.ok);
        ASSERT(root_count == vm->gc.root_count);
        VM_ROOT(vm, &input_result.as.ok);

        if (!parser_next_sexpr(vm, &output_parser, &output_parse)) {
            fprintf(stderr, "output parsing failed\n");
            VM_UNROOT(vm, &input_result.as.ok);
            goto cleanup;
        }
        if (!output_parse.ok) {
            fprintf(stderr, "output parsing failed\n");
            VM_UNROOT(vm, &input_result.as.ok);
            goto cleanup;
        }

        if (!sexpr_eq(input_result.as.ok, output_parse.as.ok)) {
            PRINT_SEXPR_RAW(input_result.as.ok, 0); printf("\n");
            PRINT_SEXPR_RAW(output_parse.as.ok, 0); printf("\n");
            fprintf(stderr, "input and output are not the same\n");
            VM_UNROOT(vm, &input_result.as.ok);
            goto cleanup;
        }

        VM_UNROOT(vm, &input_result.as.ok);
    }

    result = true;
cleanup:
    parser_free(&output_parser);
    parser_free(&input_parser);
    return result;
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
            Vm vm;
            if (!vm_init(&vm)) {
                fprintf(stderr, "vm initialization failed\n");
                exit(EXIT_FAILURE);
            }

            s8 input = (s8) { test.test.input, test.test.input_len };
            s8 output = (s8) { test.test.output, test.test.output_len };
            result = run_integration_test(
                &vm,
                input,
                output
            );

            vm_free(&vm);
        }

        printf("%s: %s\n", result ? "✅" : "❌", test.name);
        *(result ? &successes : &failures) += 1;

        if (!test.is_unit_test) {
            free(test.name);
            free(test.test.input);
            free(test.test.output);
            memset(&tests.tests[index], 0, sizeof(TestDefinition));
        }

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
