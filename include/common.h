#ifndef LISP_COMMON_H
#define LISP_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enables Valgrind support for the garbage collector, in order to better check
// correctness.
// #define ENABLE_VALGRIND_SUPPORT

// Forces arenas to be zeroed on initialization and reset.
//
// Useful for debugging the garbage collector.
#define DEBUG_CLEAR_ARENA

#endif
