#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "parse-context.h"
#include "parser.h"

void parser_init(Parser* parser, FILE* file, FILE* prompt) {
    lexer_init(&parser->lexer, file, prompt);

    parser->has_peeked = false;
}

void parser_init_s8(Parser* parser, s8 input) {
    lexer_init_s8(&parser->lexer, input);

    parser->has_peeked = false;
}

void parser_free(Parser* parser) {
    lexer_free(&parser->lexer);

    parser->has_peeked = false;
}

static Token parser_peek_token(
    Vm* vm,
    Parser* parser,
    ParseContext* context,
    bool* had_error
) {
    if (parser->has_peeked) {
        *had_error = parser->peeked_errored;
        return parser->peeked;
    }

    parser->peeked = lexer_next_token(
        vm,
        &parser->lexer,
        context,
        &parser->peeked_errored
    );
    parser->has_peeked = true;
    return parser->peeked;
}

static Token parser_next_token(
    Vm* vm,
    Parser* parser,
    ParseContext* context,
    bool* had_error
) {
    Token token = parser_peek_token(vm, parser, context, had_error);
    parser->has_peeked = false;
    return token;
}

static SExpr* parser_parse_symbol(
    Vm* vm,
    Parser* parser,
    ParseContext* context
) {
    bool had_error;
    Token token = parser_next_token(vm, parser, context, &had_error);
    ASSERT(token.type == TOKEN_SYMBOL);
    return vm_alloc_symbol(
        vm,
        had_error ?
            s8("had error when parsing symbol")
            : token_s8(&parser->lexer, token)
    );
}

static SExpr* parser_parse_string(
    Vm* vm,
    Parser* parser,
    ParseContext* context
) {
    bool had_error;
    Token token = parser_next_token(vm, parser, context, &had_error);
    ASSERT(token.type == TOKEN_STRING);
    if (had_error)
        return vm_alloc_string(vm, s8("had error when parsing string"));

    s8 token_str = token_s8(&parser->lexer, token);
    size_t resolved_byte_count = 0;
    for (size_t index = 1; index < token_str.len - 1;) {
        if (s8_index(token_str, index) == 0x5C) { // Backslash
            uint8_t byte;
            uint32_t val;
            switch (s8_index(token_str, index + 1)) {
                case 0x22: // Double quotation mark
                case 0x27: // Single quotation mark
                case 0x30: // Zero (escape for a NUL character)
                case 0x5C: // Backslash
                case 0x6E: // n (escape for a newline)
                case 0x72: // r (escape for a carriage return)
                case 0x74: // t (escape for a tab)
                    // Valid single byte escape
                    index += 2;
                    resolved_byte_count += 1;
                    break;
                case 0x75: // u (escape for an arbitrary unicode codepoint)
                    val = 0;

                    index += 3;
                    while (s8_index(token_str, index) != 0x7D) { // Right curly
                        byte = s8_index(token_str, index);
                        val = val << 4;
                        if (0x30 <= byte && byte <= 0x39) {
                            val |= byte - 0x30;
                        } else if (0x41 <= byte && byte <= 0x46) {
                            val |= byte - 0x41 + 10;
                        } else {
                            val |= byte - 0x61 + 10;
                        }
                        index += 1;
                    }

                    index += 1;
                    resolved_byte_count += 1;
                    if (val > 0x7F) resolved_byte_count += 1;
                    if (val > 0x7FF) resolved_byte_count += 1;
                    if (val > 0xFFF) resolved_byte_count += 1;
                    break;
                case 0x78: // x (escape for an 8-bit unicode codepoint)
                    val = 0;

                    byte = s8_index(token_str, index + 2);
                    val = val << 4;
                    if (0x30 <= byte && byte <= 0x39) {
                        val |= byte - 0x30;
                    } else if (0x41 <= byte && byte <= 0x46) {
                        val |= byte - 0x41 + 10;
                    } else {
                        val |= byte - 0x61 + 10;
                    }

                    byte = s8_index(token_str, index + 3);
                    val = val << 4;
                    if (0x30 <= byte && byte <= 0x39) {
                        val |= byte - 0x30;
                    } else if (0x41 <= byte && byte <= 0x46) {
                        val |= byte - 0x41 + 10;
                    } else {
                        val |= byte - 0x61 + 10;
                    }

                    index += 4;
                    resolved_byte_count += 1;
                    if (val > 0x7F) resolved_byte_count += 1;
                    break;
            }
        } else {
            index += 1;
            resolved_byte_count += 1;
        }
    }

    SExpr* str = vm_alloc_string_with_length(vm, resolved_byte_count);
    s8 str_s8 = EXTRACT_STRING(str);

    size_t put_index = 0;
    for (size_t get_index = 1; get_index < token_str.len - 1;) {
        if (s8_index(token_str, get_index) == 0x5C) { // Backslash
            uint8_t byte;
            uint32_t val;
            switch (s8_index(token_str, get_index + 1)) {
                case 0x22: // Double quotation mark
                    val = 0x22;
                    get_index += 2;
                    break;
                case 0x27: // Single quotation mark
                    val = 0x27;
                    get_index += 2;
                    break;
                case 0x30: // Zero (escape for a NUL character)
                    val = 0;
                    get_index += 2;
                    break;
                case 0x5C: // Backslash
                    val = 0x5C;
                    get_index += 2;
                    break;
                case 0x6E: // n (escape for a newline)
                    val = 0x0A;
                    get_index += 2;
                    break;
                case 0x72: // r (escape for a carriage return)
                    val = 0x0D;
                    get_index += 2;
                    break;
                case 0x74: // t (escape for a tab)
                    val = 0x09;
                    get_index += 2;
                    break;
                case 0x75: // u (escape for an arbitrary unicode codepoint)
                    val = 0;

                    get_index += 3;
                    while (s8_index(token_str, get_index) != 0x7D) { // Right curly
                        byte = s8_index(token_str, get_index);
                        val = val << 4;
                        if (0x30 <= byte && byte <= 0x39) {
                            val |= byte - 0x30;
                        } else if (0x41 <= byte && byte <= 0x46) {
                            val |= byte - 0x41 + 10;
                        } else {
                            val |= byte - 0x61 + 10;
                        }
                        get_index += 1;
                    }

                    get_index += 1;
                    break;
                case 0x78: // x (escape for an 8-bit unicode codepoint)
                    byte = s8_index(token_str, get_index + 2);
                    if (0x30 <= byte && byte <= 0x39) {
                        val = byte - 0x30;
                    } else if (0x41 <= byte && byte <= 0x46) {
                        val = byte - 0x41 + 10;
                    } else {
                        val = byte - 0x61 + 10;
                    }

                    byte = s8_index(token_str, get_index + 3);
                    val = val << 4;
                    if (0x30 <= byte && byte <= 0x39) {
                        val |= byte - 0x30;
                    } else if (0x41 <= byte && byte <= 0x46) {
                        val |= byte - 0x41 + 10;
                    } else {
                        val |= byte - 0x61 + 10;
                    }

                    get_index += 4;
                    break;
            }

            uint8_t codepoint_width = 1;
            if (val > 0x7F) codepoint_width += 1;
            if (val > 0x7FF) codepoint_width += 1;
            if (val > 0xFFF) codepoint_width += 1;

            switch (codepoint_width) {
                case 1:
                    str_s8.ptr[put_index] = val;
                    break;
                case 2:
                    str_s8.ptr[put_index] = 0xC | (val >> 6);
                    break;
                case 3:
                    str_s8.ptr[put_index] = 0xE | (val >> 12);
                    break;
                case 4:
                    str_s8.ptr[put_index] = 0xF | (val >> 18);
                    break;
            }

            size_t index = 1;
            while (index < codepoint_width) {
                str_s8.ptr[put_index] = (val >> (codepoint_width * 6 - index * 6)) & 0x3F;

                index += 1;
            }

            put_index += codepoint_width;
        } else {
            str_s8.ptr[put_index] = s8_index(token_str, get_index);
            get_index += 1;
            put_index += 1;
        }
    }

    return str;
}

static SExpr* parser_parse_number(
    Vm* vm,
    Parser* parser,
    ParseContext* context
) {
    bool had_error;
    Token token = parser_next_token(vm, parser, context, &had_error);
    ASSERT(token.type == TOKEN_NUMBER);
    if (had_error) return vm_alloc_number(vm, INFINITY);

    s8 tmp;
    tmp.ptr = (uint8_t*) gc_alloc_untyped(&vm->gc, token.length + 1, 1);
    tmp.len = token.length;

    // Create a NUL-terminated string for the `strtod` function.
    tmp.ptr[token.length] = 0;
    s8_copy(tmp, token_s8(&parser->lexer, token));

    double number = strtod((const char*) tmp.ptr, NULL);
    return vm_alloc_number(vm, number);
}

static bool parser_parse_sexpr(
    Vm* vm,
    Parser* parser,
    ParseContext* context,
    SExpr** sexpr
);

static SExpr* parser_parse_quote(
    Vm* vm,
    Parser* parser,
    ParseContext* context
) {
    bool had_error;
    Token token = parser_next_token(vm, parser, context, &had_error);
    ASSERT(token.type == TOKEN_SINGLE_QUOTE);

    SExpr* quoted;
    bool result = parser_parse_sexpr(
        vm,
        parser,
        context,
        &quoted
    );
    if (!result) {
        parse_context_add_error(
            vm,
            context,
            MISSING_SEXPR,
            token.index,
            parser->lexer.index - token.index
        );
        quoted = vm_alloc_symbol(vm, s8("quote is missing sexpr"));
    }

    SExpr* cons_quoted = vm_alloc_cons(vm, quoted, NIL);

    VM_ROOT(vm, &cons_quoted);
    SExpr* quote_symbol = vm_alloc_symbol(vm, s8("quote"));
    VM_UNROOT(vm, &cons_quoted);

    return vm_alloc_cons(vm, quote_symbol, cons_quoted);
}

static SExpr* parser_parse_list(
    Vm* vm,
    Parser* parser,
    ParseContext* context
) {
    bool had_error;
    Token token = parser_next_token(vm, parser, context, &had_error);
    ASSERT(token.type == TOKEN_LEFT_PAREN);

    SExpr* base = NIL;
    SExpr* current = NIL;
    
    VM_ROOT(vm, &base);
    VM_ROOT(vm, &current);

    size_t start_index = token.index;
    while (
        (token = parser_peek_token(vm, parser, context, &had_error)).type
        != TOKEN_RIGHT_PAREN
    ) {
        if (token.type == TOKEN_END) {
            parse_context_add_error(
                vm,
                context,
                UNTERMINATED_LIST,
                start_index,
                parser->lexer.index - start_index
            );

            goto cleanup;
        }

        SExpr* sexpr;
        bool result = parser_parse_sexpr(
            vm,
            parser,
            context,
            &sexpr
        );
        ASSERT(result == true);

        SExpr* cons = vm_alloc_cons(vm, sexpr, NIL);
        if (base == NIL) {
            base = cons;
            current = base;
        } else {
            AS_CONS(current)->cdr = cons;
            current = AS_CONS(current)->cdr;
        }
    }

    parser_next_token(vm, parser, context, &had_error);
cleanup:
    VM_UNROOT(vm, &base);
    VM_UNROOT(vm, &current);
    return base;
}

static bool parser_parse_sexpr(
    Vm* vm,
    Parser* parser,
    ParseContext* context,
    SExpr** sexpr
) {
    bool had_error;
    Token token = parser_peek_token(vm, parser, context, &had_error);
    switch (token.type) {
        case TOKEN_SYMBOL:
            *sexpr = parser_parse_symbol(vm, parser, context);
            return true;
        case TOKEN_STRING:
            *sexpr = parser_parse_string(vm, parser, context);
            return true;
        case TOKEN_NUMBER:
            *sexpr = parser_parse_number(vm, parser, context);
            return true;
        case TOKEN_LEFT_PAREN:
            *sexpr = parser_parse_list(vm, parser, context);
            return true;
        case TOKEN_RIGHT_PAREN:
            return false;
        case TOKEN_SINGLE_QUOTE:
            *sexpr = parser_parse_quote(vm, parser, context);
            return true;
        case TOKEN_END:
            return false;
    }
}

bool parser_next_sexpr(Vm* vm, Parser* parser, ParseResult* result) {
    ParseContext context = NULL;
    VM_ROOT(vm, &context);

    bool had_error;
    while (
        parser_peek_token(
            vm,
            parser,
            &context,
            &had_error
        ).type == TOKEN_RIGHT_PAREN
    ) {
        parser_next_token(vm, parser, &context, &had_error);
    }

    SExpr* sexpr;
    if (!parser_parse_sexpr(vm, parser, &context, &sexpr)) {
        VM_UNROOT(vm, &context);
        return false;
    }

    if (parse_context_error_count(context) == 0) {
        result->ok = true;
        result->as.ok = sexpr;
    } else {
        result->ok = false;
        result->as.err = context;
    }

    VM_UNROOT(vm, &context);
    return true;
}

#ifdef ENABLE_TESTS

#include "test.h"
#include "util.h"

bool parser_parse_nil() {
    Vm vm;
    Parser parser;

    if (!vm_init(&vm)) return false;
    parser_init_s8(&parser, s8("()"));

    bool test_result = false;

    ParseResult result;
    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_NIL(result.as.ok)) goto cleanup;

    if (parser_next_sexpr(&vm, &parser, &result)) goto cleanup;

    test_result = true;
cleanup:
    parser_free(&parser);
    vm_free(&vm);
    return test_result;
}

bool parser_parse_double_nil() {
    Vm vm;
    Parser parser;

    if (!vm_init(&vm)) return false;
    parser_init_s8(&parser, s8("() ()"));

    bool test_result = false;

    ParseResult result;
    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_NIL(result.as.ok)) goto cleanup;

    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_NIL(result.as.ok)) goto cleanup;

    if (parser_next_sexpr(&vm, &parser, &result)) goto cleanup;

    test_result = true;
cleanup:
    parser_free(&parser);
    vm_free(&vm);
    return test_result;
}

bool parser_parse_basic_symbols() {
    Vm vm;
    Parser parser;

    if (!vm_init(&vm)) return false;
    parser_init_s8(&parser, s8("news nil? string?"));

    bool test_result = false;

    ParseResult result;
    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_SYMBOL(result.as.ok)) goto cleanup;

    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_SYMBOL(result.as.ok)) goto cleanup;

    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_SYMBOL(result.as.ok)) goto cleanup;

    if (parser_next_sexpr(&vm, &parser, &result)) goto cleanup;

    test_result = true;
cleanup:
    parser_free(&parser);
    vm_free(&vm);
    return test_result;
}

bool parser_parse_simple_list() {
    Vm vm;
    Parser parser;

    if (!vm_init(&vm)) return false;
    parser_init_s8(&parser, s8("(news)"));

    bool test_result = false;

    ParseResult result;
    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_CONS(result.as.ok)) goto cleanup;

    if (parser_next_sexpr(&vm, &parser, &result)) goto cleanup;

    test_result = true;
cleanup:
    parser_free(&parser);
    vm_free(&vm);
    return test_result;
}

bool parser_skip_right_parens() {
    Vm vm;
    Parser parser;

    if (!vm_init(&vm)) return false;
    parser_init_s8(&parser, s8(")))))) (news)"));

    bool test_result = false;

    ParseResult result;
    if (!parser_next_sexpr(&vm, &parser, &result)) goto cleanup;
    if (!result.ok) goto cleanup;
    if (!IS_CONS(result.as.ok)) goto cleanup;

    if (parser_next_sexpr(&vm, &parser, &result)) goto cleanup;

    test_result = true;
cleanup:
    parser_free(&parser);
    vm_free(&vm);
    return test_result;
}

TestDefinition parser_tests[] = {
    DEFINE_UNIT_TEST(parser_parse_nil, 1),
    DEFINE_UNIT_TEST(parser_parse_double_nil, 1),
    DEFINE_UNIT_TEST(parser_parse_basic_symbols, 1),
    DEFINE_UNIT_TEST(parser_parse_simple_list, 1),
    DEFINE_UNIT_TEST(parser_skip_right_parens, 1),
};

TestList parser_test_list = (TestList) {
    parser_tests,
    countof(parser_tests),
};

#endif
