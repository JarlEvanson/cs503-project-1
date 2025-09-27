#ifndef LISP_COMMON_H
#define LISP_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enables Valgrind support for any custom memory allocators in order to better
// ensure correctness.
// #define ENABLE_VALGRIND_SUPPORT

// Forces arenas to be zeroed on initialization and reset.
//
// Useful for debugging the garbage collector.
// #define DEBUG_CLEAR_ARENA
// Forces garbage to be collected on every allocation, which increases the
// likelihood of any garbage collection bugs occurring.
// #define DEBUG_STRESS_GC

// Enables logging of GC operations.
// #define DEBUG_LOG_GC

#ifdef ENABLE_TEST_MODE

// Unconditionally enable options that improve the ability of testing to catch
// errors.

// Force clearing to better detect various errors.
#define DEBUG_CLEAR_ARENA
// Force collection to better detect various bugs.
#define DEBUG_STRESS_GC

#endif

#endif
