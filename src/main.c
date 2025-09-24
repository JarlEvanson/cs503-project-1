#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc == 1) {

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

        fclose(file);
    } else {
        fprintf(stderr, "usage: lisp [path]\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
