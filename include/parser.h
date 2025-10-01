#ifndef LISP_PARSER_H
#define LISP_PARSER_H

#include "common.h"
#include "lexer.h"
#include "parse-context.h"
#include "s8.h"

struct Parser {
    Lexer lexer;

    bool has_peeked;
    bool peeked_errored;
    Token peeked;
};

void parser_init(Parser* parser, FILE* file, FILE* prompt);
void parser_init_s8(Parser* parser, s8 input);
void parser_free(Parser* parser);

typedef struct {
    bool ok;
    union {
        SExpr* ok;
        ParseContext err;
    } as;
} ParseResult;

// This function parses the next `SExpr` in the character stream.
//
// It returns `false` if the character stream has ended and thus there are no
// more `SExpr`s to be parsed. When it returns `true`, the results of `result`
// are valid.
bool parser_next_sexpr(Vm* vm, Parser* parser, ParseResult* result);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList parser_test_list;

#endif

#endif
