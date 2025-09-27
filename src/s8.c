#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "s8.h"

uint8_t s8_index(s8 s, size_t index) {
    ASSERT(index < s.len, "out of bounds index");

    return s.ptr[index];
}

bool s8_equals(s8 a, s8 b) {
    if (a.len != b.len) {
        return false;
    }

    size_t index = 0;
    while (index < a.len) {
        if (a.ptr[index] != b.ptr[index]) {
            return false;
        }

        index += 1;
    }

    return true;
}

void s8_copy(s8 to, s8 from) {
    ASSERT(to.len == from.len, "s8 length must be equal");
    for (size_t index = 0; index < to.len; index++) {
        to.ptr[index] = from.ptr[index];
    }
}

void s8_copy_from(s8 to, s8 from, size_t index) {
    ASSERT(index + to.len <= from.len, "copy target must not cause overrun");

    from.ptr += index;
    from.len = to.len;
    s8_copy(to, from);
}

#ifdef ENABLE_TESTS

#include "test.h"

bool s8_copy_works() {
    s8 a = s8("Hello");
    s8 b;

    b.ptr = malloc(a.len);
    b.len = a.len;
    if (b.ptr == NULL) {
        return false;
    }

    s8_copy(b, a);
    bool result = s8_equals(a, b);

    free(b.ptr);
    return result;
}

bool s8_copy_from_works() {
    s8 a = s8("Hello World!");
    s8 b;

    b.ptr = malloc(5);
    b.len = 5;
    if (b.ptr == NULL) {
        return false;
    }

    s8_copy_from(b, a, 6);
    bool result = s8_equals(b, s8("World"));

    free(b.ptr);
    return result;
}

TestDefinition s8_tests[] = {
    DEFINE_TEST(s8_copy_works, 0),
    DEFINE_TEST(s8_copy_from_works, 0),
};

TestList s8_test_list = (TestList) {
    s8_tests,
    countof(s8_tests)
};

#endif
