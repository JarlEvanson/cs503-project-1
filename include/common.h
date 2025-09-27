#ifndef LISP_COMMON_H
#define LISP_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enables Valgrind support for the garbage collector, in order to better check
// correctness.
#define ENABLE_VALGRIND_SUPPORT

// Forces arenas to be zeroed on initialization and reset.
//
// Useful for debugging the garbage collector.
#define DEBUG_CLEAR_ARENA
// Forces garbage to be collected on every allocation, which increases the
// likelihood of any garbage collection bugs occurring.
#define DEBUG_STRESS_GC

// Enables logging of GC operations.
#define DEBUG_LOG_GC

#endif
