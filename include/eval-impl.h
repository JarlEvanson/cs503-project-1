#ifndef LISP_EVAL_IMPL_H
#define LISP_EVAL_IMPL_H

bool validate_function_def(EvalContext* context, SExpr* def);

bool eval_internal(
    Vm* vm,
    EvalContext* context,
    SExpr* sexpr,
    SExpr** result
);

bool eval_func(
    Vm* vm,
    EvalContext* context,
    SExpr* id,
    SExpr* def,
    SExpr* args,
    SExpr** result
);

#endif
