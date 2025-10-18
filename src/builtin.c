#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"
#include "common.h"
#include "eval-context.h"
#include "eval-impl.h"
#include "parser.h"
#include "sexpr.h"
#include "util.h"
#include "vm.h"

#define DEFINE_BUILTIN(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, true, func }

#define DEFINE_BUILTIN_VARIADIC(name, func) \
    (BuiltinDef) { s8(name), true, 0, true, func }

#define DEFINE_BUILTIN_NO_EVAL(name, arg_count, func) \
    (BuiltinDef) { s8(name), false, arg_count, false, func }

#define DEFINE_BUILTIN_NO_EVAL_VARIADIC(name, func) \
    (BuiltinDef) { s8(name), true, 0, false, func }

BuiltinDef builtin_def_list[] = {
};

bool lookup_builtin(s8 id, BuiltinDef* out) {
    for (size_t i = 0; i < countof(builtin_def_list); i++) {
        if (s8_equals(id, builtin_def_list[i].name)) {
            if (out != NULL) *out = builtin_def_list[i];
            return true;
        }
    }

    return false;
}
