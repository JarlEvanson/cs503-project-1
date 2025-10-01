#ifndef LISP_LEXER_H
#define LISP_LEXER_H

#include <stdio.h>

#include "common.h"
#include "parse-context.h"
#include "s8.h"
#include "vm.h"

typedef struct {
    FILE* file;
    uint8_t* input;
    size_t input_len;
    size_t input_capacity;

    FILE* prompt;
    size_t index;
} Lexer;

void lexer_init(Lexer* lexer, FILE* file, FILE* prompt);
void lexer_init_s8(Lexer* lexer, s8 input);
void lexer_free(Lexer* lexer);

typedef enum {
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_SINGLE_QUOTE,
    TOKEN_END,
} TokenType;

typedef struct {
    TokenType type;
    size_t index;
    size_t length;
} Token;

s8 token_s8(Lexer* lexer, Token token);
Token lexer_next_token(
    Vm* vm,
    Lexer* lexer,
    ParseContext* context,
    bool* had_error
);

#ifdef ENABLE_TESTS

#include "test.h"

extern TestList lexer_test_list;

#endif

#endif
