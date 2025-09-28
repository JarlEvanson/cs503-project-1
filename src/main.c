#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "vm.h"

void drive(Vm* vm, Parser* parser) {
    ParseResult parse_result;
    while (parser_next_sexpr(vm, parser, &parse_result)) {
        if (!parse_result.ok) {
            parse_context_print(parse_result.as.err, parser);
            continue;
        }

        PRINT_SEXPR(parse_result.as.ok);
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    Vm vm;
    Parser parser;

    if (argc == 1) {
        if (!vm_init(&vm)) {
            fprintf(stderr, "failed to initialize VM\n");
            return EXIT_FAILURE;
        }
        parser_init(&parser, stdin, stdout);

        drive(&vm, &parser);

        parser_free(&parser);
        vm_free(&vm);
    } else if (argc == 2) {
        FILE* file = fopen(argv[1], "rb");
        if (file == NULL) {
            fprintf(
                stderr,
                "failed to open file \"%s\": %s\n",
                argv[1],
                strerror(errno)
            );
            return EXIT_FAILURE;
        }

        if (!vm_init(&vm)) {
            fprintf(stderr, "failed to initialize VM\n");
            return EXIT_FAILURE;
        }
        parser_init(&parser, file, NULL);

        drive(&vm, &parser);

        fclose(file);
        parser_free(&parser);
        vm_free(&vm);
    } else {
        fprintf(stderr, "usage: lisp [path]\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
