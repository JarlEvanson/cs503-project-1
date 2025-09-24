#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

__AFL_FUZZ_INIT();

int main() {
    unsigned char* buffer;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    buffer = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(100000)) {
        size_t length = __AFL_FUZZ_TESTCASE_LEN;
        printf("Buffer: %p\nLength: %zu\n", buffer, length);
    }

    return EXIT_SUCCESS;
}
