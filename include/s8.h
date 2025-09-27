#ifndef LISP_S8_H
#define LISP_S8_H

#include "common.h"
#include "util.h"

#define s8(s) ( (s8) { (uint8_t*) s, countof(s) - 1 } )

typedef struct {
    uint8_t* ptr;
    size_t len;
} s8;

/// Returns the byte located at `index`.
///
/// # Safety
///
/// `index` must be less than `from.len`
uint8_t s8_index(s8, size_t index);

/// Returns `true` if the first string equals the second.
bool s8_equals(s8, s8);

/// Copies the contents of `from` into `to`.
///
/// # Safety
///
/// `to.len` must equal `from.len`.
void s8_copy(s8 to, s8 from);

/// Copies `to.len` bytes from `from` starting at `index`.
///
/// # Safety
///
/// `index + to.len` must be less than or equal to `from.len`.
void s8_copy_from(s8 to, s8 from, size_t index);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList s8_test_list;

#endif

#endif
