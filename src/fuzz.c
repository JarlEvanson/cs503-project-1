#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "eval.h"
#include "parser.h"
#include "vm.h"

__AFL_FUZZ_INIT();

int main() {
    unsigned char* buffer;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    buffer = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(100000)) {
        Vm vm;
        Parser parser;

        if (!vm_init(&vm)) {
            fprintf(stderr, "failed to initialize VM\n");
            return EXIT_FAILURE;
        }
        s8 input;
        input.ptr = (uint8_t*) buffer;
        input.len = __AFL_FUZZ_TESTCASE_LEN;
        parser_init_s8(&parser, input);

        size_t root_count = vm.gc.root_count;
        ParseResult parse_result;
        while (parser_next_sexpr(&vm, &parser, &parse_result)) {
            ASSERT(root_count == vm.gc.root_count);
            if (!parse_result.ok) {
                parse_context_print(parse_result.as.err, &parser);
                continue;
            }

            PRINT_SEXPR(parse_result.as.ok);
            printf("\n");

            EvalResult eval_result = eval(&vm, parse_result.as.ok);
            ASSERT(root_count == vm.gc.root_count);
            if (!eval_result.ok) {
                eval_context_print(eval_result.as.err);
                continue;
            }

            PRINT_SEXPR(eval_result.as.ok); printf("\n");
        }

        parser_free(&parser);
        vm_free(&vm);
    }

    return EXIT_SUCCESS;
}
