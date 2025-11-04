#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lexer.h"
#include "s8.h"
#include "util.h"
#include "vm.h"

void lexer_init(Lexer* lexer, FILE* file, FILE* prompt) {
    lexer->file = file;

    lexer->input = NULL;
    lexer->input_len = 0;
    lexer->input_capacity = 0;

    lexer->prompt = prompt;
    lexer->index = 0;
}

void lexer_init_s8(Lexer* lexer, s8 input) {
    lexer_init(lexer, NULL, NULL);

    lexer->input = input.ptr;
    lexer->input_len = input.len;
}

void lexer_free(Lexer* lexer) {
    lexer->file = NULL;

    if (lexer->input_capacity != 0) {
        free(lexer->input);
    }

    lexer->input = NULL;
    lexer->input_len = 0;
    lexer->input_capacity = 0;

    lexer->prompt = NULL;
    lexer->index = 0;
}

s8 token_s8(Lexer* lexer, Token token) {
    s8 s;

    s.ptr = &lexer->input[token.index];
    s.len = token.length;

    return s;
}

static bool lexer_get_additional_input(Lexer* lexer) {
    // Input file isn't available, so we can't read additional input.
    if (lexer->file == NULL) return false;

    // The atypical sizing check is because `fgets()` requires more than 1
    // byte in the buffer, so we need to expand the buffer earlier.
    if (lexer->input_len + 1 >= lexer->input_capacity) {
        if (!GROW(&lexer->input, &lexer->input_capacity, 1, 128)) {
            fprintf(stderr, "failed to expand lexer buffer\n");
            exit(EXIT_FAILURE);
        }
    }

    if (lexer->prompt) {
        fprintf(lexer->prompt, "$> ");
        fflush(lexer->prompt);
    }

    uint8_t* additional_input_ptr = lexer->input + lexer->input_len;
    char* result = fgets(
        (char*) additional_input_ptr,
        lexer->input_capacity - lexer->input_len,
        lexer->file
    );

    if (result == NULL) {
        // No additional input returned, so we disable additional reads.
        lexer->file = NULL;
        return false;
    }

    size_t additional_len = strlen(result);
    if (additional_len == 0) {
        // No additional input returned, so we disable additional reads.
        lexer->file = NULL;
        return false;
    }

    lexer->input_len += additional_len;
    return true;
}

static bool lexer_peek_nth_byte(Lexer* lexer, size_t nth, uint8_t* byte) {
    size_t read_index = lexer->index + nth;
    ASSERT(read_index >= lexer->index, "lexer peek index overflowed");

    while (read_index >= lexer->input_len) {
        // More input required.
        if (!lexer_get_additional_input(lexer)) {
            return false;
        }
    }

    *byte = lexer->input[read_index];
    return true;
}

static bool lexer_codepoint_at_offset(
    Lexer* lexer,
    bool* utf8_error,
    size_t* offset,
    uint32_t* codepoint
) {
    uint8_t byte;
    if (!lexer_peek_nth_byte(lexer, *offset, &byte)) goto handle_end;

    uint8_t codepoint_width;
    uint32_t val;
    if (byte >> 7 == 0) {
        codepoint_width = 1;
        val = byte & ~(1 << 7);
    } else if (byte >> 5 == 0x6) {
        codepoint_width = 2;
        val = byte & ~(0x7 << 7);
    } else if (byte >> 4 == 0x16) {
        codepoint_width = 3;
        val = byte & ~(0xF << 7);
    } else if (byte >> 3 == 0x36) {
        codepoint_width = 4;
        val = byte & ~(0x1F << 7);
    } else {
        goto handle_utf8_error;
    }

    uint8_t byte_index = 1;
    while (byte_index < codepoint_width) {
        if (!lexer_peek_nth_byte(lexer, *offset + byte_index, &byte)) {
            goto handle_utf8_error;
        }

        if (byte >> 6 != 0x2) {
            goto handle_utf8_error;
        }

        val = (val << 6) | (byte & ((1 << 6) - 1));
        byte_index += 1;
    }

    if (!(val <= 0x10FFFF) || (0xD800 <= val && val <= 0xDFFF)) {
        // `val` is either not in the unicode codepoint range or is a surrogate
        // code point, both of which invalid `val` for the purposes for this
        // lexer.
        goto handle_utf8_error;
    }

    *codepoint = val;
    *offset += codepoint_width;
    return true;

handle_utf8_error:
    // Bytes located at index do not form a valid UTF-8 codepoint.
    *utf8_error = true;
    return false;

handle_end:
    // `index` is not a valid index into the input.
    *utf8_error = false;
    return false;
}

static bool lexer_peek_codepoint(
    Lexer* lexer,
    bool* utf8_error,
    uint32_t* codepoint
) {
    size_t offset = 0;
    return lexer_codepoint_at_offset(
        lexer,
        utf8_error,
        &offset,
        codepoint
    );
}

static bool lexer_next_codepoint(
    Lexer* lexer,
    bool* utf8_error,
    uint32_t* codepoint
) {
    size_t offset = 0;
    bool result = lexer_codepoint_at_offset(
        lexer,
        utf8_error,
        &offset,
        codepoint
    );

    lexer->index += offset;
    return result;
}

static void lexer_skip_invalid_utf8(Lexer* lexer) {
    bool is_utf8_error;
    uint32_t codepoint;

    // Validate that the next bytes do not form a valid UTF-8 codepoint.
    ASSERT(
        !lexer_peek_codepoint(lexer, &is_utf8_error, &codepoint)
        && is_utf8_error
    );

    do {
        size_t index = 0;
        bool result = lexer_codepoint_at_offset(
            lexer,
            &is_utf8_error,
            &index,
            &codepoint
        );
        if (result == true) return; // All non-UTF-8 bytes have been skipped.

        if (!is_utf8_error) return; // All non-UTF-8 bytes have been skipped.
        lexer->index += 1;
    } while(1);
}

static bool codepoint_is_whitespace(uint32_t c) {
    return c == 0x09  // Horizontal tab
        || c == 0x0A  // Line feed / new line
        || c == 0x0D  // Carriage return
        || c == 0x20; // Space
}

static bool codepoint_is_digit(uint32_t c) {
    return 0x30 <= c && c <= 0x39; // Digits 0-9
}

static bool codepoint_is_numeric_continue(uint32_t c) {
    return codepoint_is_digit(c) || c == 0x2E; // Period
}

static bool codepoint_is_numeric_start(uint32_t c) {
    return codepoint_is_numeric_continue(c)
        || c == 0x2B  // Plus sign
        || c == 0x2D; // Minus sign
}

static bool codepoint_is_hex_digit(uint32_t c) {
    return codepoint_is_digit(c)
        || (0x41 <= c && c <= 0x46)
        || (0x61 <= c && c <= 0x66);
}

void lexer_handle_utf8_error(Vm* vm, Lexer* lexer, ParseContext* context) {
    // Store the start of the invalid byte sequence.
    size_t utf8_error_start = lexer->index;
    // Skip the invalid byte sequence.
    lexer_skip_invalid_utf8(lexer);
    // Add the error to the parse context.
    parse_context_add_error(
        vm,
        context,
        INVALID_UTF8,
        utf8_error_start,
        lexer->index - utf8_error_start
    );
}

static void lexer_handle_escape(
    Vm* vm,
    Lexer* lexer,
    ParseContext* context
) {
    uint32_t c;
    bool utf8_error;

    size_t escape_start_index = lexer->index - 1;
    if (!lexer_next_codepoint(lexer, &utf8_error, &c)) goto peek_error;

    switch (c) {
        case 0x22: // Double quotation mark
        case 0x27: // Single quotation mark
        case 0x30: // Zero (escape for a NUL character)
        case 0x5C: // Backslash
        case 0x6E: // n (escape for a newline)
        case 0x72: // r (escape for a carriage return)
        case 0x74: // t (escape for a tab)
            // Valid single byte escapes
            return;
        case 0x75: // u (escape for an arbitrary unicode codepoint)
            if (!lexer_peek_codepoint(lexer, &utf8_error, &c)) goto peek_error;

            // Left curly bracket
            if (c != 0x7B) goto wrong_char_error;
            lexer_next_codepoint(lexer, &utf8_error, &c);

            uint32_t val = 0;
            uint8_t digits_read = 0;
            do {
                if (!lexer_peek_codepoint(lexer, &utf8_error, &c))
                    goto peek_error;

                if (c == 0x7D) { // Right curly bracket
                    lexer_next_codepoint(lexer, &utf8_error, &c);
                    break;
                }

                if (codepoint_is_hex_digit(c)) {
                    digits_read += 1;
                    val = val << 4;
                    if (codepoint_is_digit(c)) {
                        val |= c - 0x30;
                    } else if (0x41 <= c && c <= 0x46) {
                        val |= c - 0x41 + 10;
                    } else {
                        val |= c - 0x61 + 10;
                    }

                    lexer_next_codepoint(lexer, &utf8_error, &c);
                } else {
                    goto wrong_char_error;
                }
            } while (1);

            if (digits_read > 6
                || !(val <= 0x10FFF)
                || (0xD800 <= val && val <= 0xDFFF)
            ) {
                parse_context_add_error(
                    vm,
                    context,
                    INVALID_ESCAPE,
                    escape_start_index,
                    lexer->index - escape_start_index
                );

                return;
            }
            return;
        case 0x78: // x (escape for an 8-bit unicode codepoint)
            if (!lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                goto peek_error;
            }

            if (!codepoint_is_hex_digit(c)) {
                goto wrong_char_error;
            }

            lexer_next_codepoint(lexer, &utf8_error, &c);
            if (!lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                goto peek_error;
            }

            if (!codepoint_is_hex_digit(c)) {
                goto wrong_char_error;
            }

            lexer_next_codepoint(lexer, &utf8_error, &c);
            return;
        default:
            parse_context_add_error(
                vm,
                context,
                INVALID_ESCAPE,
                escape_start_index,
                lexer->index - escape_start_index
            );

            return;
    }

    return;
peek_error:
    if (utf8_error) lexer_handle_utf8_error(vm, lexer, context);

    parse_context_add_error(
        vm,
        context,
        INVALID_ESCAPE,
        escape_start_index,
        lexer->index - escape_start_index
    );

    return;
wrong_char_error:
    parse_context_add_error(
        vm,
        context,
        INVALID_ESCAPE,
        escape_start_index,
        (lexer->index + 1) - escape_start_index
    );

    return;
}

Token lexer_next_token(
    Vm* vm,
    Lexer* lexer,
    ParseContext* context,
    bool* had_error
) {
    Token token;

    uint32_t c;
    bool utf8_error = false;

    *had_error = false;
restart:
    token.index = lexer->index;
    size_t error_count = parse_context_error_count(*context);
    if (!lexer_next_codepoint(lexer, &utf8_error, &c)) {
        if (!utf8_error) {
            // End of character stream.
            //
            // Return TOKEN_END to signify that lexing has finished.
            token.type = TOKEN_END;
            goto exit;
        }

restart_sym:
        lexer_handle_utf8_error(vm, lexer, context);

        utf8_error = false;
        // Continue lexing the symbol.
        while (lexer_peek_codepoint(lexer, &utf8_error, &c)) {
            if (codepoint_is_whitespace(c) || c == 0x28 || c == 0x29) break;

            lexer_next_codepoint(lexer, &utf8_error, &c);
        }

        if (utf8_error) goto restart_sym;

        token.type = TOKEN_SYMBOL;
        goto exit;
    }

    if (codepoint_is_whitespace(c)) {
        while (lexer_peek_codepoint(lexer, &utf8_error, &c)) {
            if (!codepoint_is_whitespace(c)) break;
            lexer_next_codepoint(lexer, &utf8_error, &c);
        }

        // We don't need to handle end of character stream or invalid UTF-8
        // bytes here, since we essentially restart the function.
        goto restart;
    } else if (c == 0x3B) {
        // Semicolon
        while (1) {
            while (lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                if (c == 0x0A) break;
                lexer_next_codepoint(lexer, &utf8_error, &c);
            }

            if (utf8_error) {
                lexer_handle_utf8_error(vm, lexer, context);
                continue;
            }

            break;
        }

        // Continue parsing (we just skipped the comment).
        goto restart;
    }

    size_t encountered_dots;
    bool valid_number;
    bool contains_digit;
    switch (c) {
        case 0x22: // Double quotation mark
            do {
                if (!lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                    if (utf8_error) {
                        lexer_handle_utf8_error(vm, lexer, context);
                        continue;
                    }

                    parse_context_add_error(
                        vm,
                        context,
                        UNTERMINATED_STRING,
                        token.index,
                        lexer->index - token.index
                    );

                    token.type = TOKEN_STRING;
                    goto exit;
                }

                lexer_next_codepoint(lexer, &utf8_error, &c);
                if (c == 0x5C) {
                    lexer_handle_escape(vm, lexer, context);
                }
            } while (c != 0x22);

            // Now we need to check that the string either has whitespace or a
            // parenthesis after it or that character stream ends after it.

            token.type = TOKEN_STRING;
            if (lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                if (!codepoint_is_whitespace(c) && c != 0x28 && c != 0x29) {
                    parse_context_add_error(
                        vm,
                        context,
                        INVALID_SUFFIX,
                        token.index,
                        lexer->index - token.index
                    );
                }
            } else if (utf8_error) {
                parse_context_add_error(
                    vm,
                    context,
                    INVALID_SUFFIX,
                    token.index,
                    lexer->index - token.index
                );
            }

            goto exit;
        case 0x27: // Single quotation mark
            token.type = TOKEN_SINGLE_QUOTE;
            goto exit;
        case 0x28: // Left parenthesis
            token.type = TOKEN_LEFT_PAREN;
            goto exit;
        case 0x29: // Right parenthesis
            token.type = TOKEN_RIGHT_PAREN;
            goto exit;
        default: // All other codepoints
            encountered_dots = (c == 0x2E);
            valid_number = codepoint_is_numeric_start(c);
            contains_digit = codepoint_is_digit(c);

restart_sym_num:
            while (lexer_peek_codepoint(lexer, &utf8_error, &c)) {
                if (codepoint_is_whitespace(c) || c == 0x28 || c == 0x29) {
                    break;
                }

                encountered_dots += (c == 0x2E);
                valid_number =
                    valid_number && codepoint_is_numeric_continue(c);
                contains_digit = contains_digit || codepoint_is_digit(c);

                lexer_next_codepoint(lexer, &utf8_error, &c);
            }

            if (utf8_error) {
                utf8_error = false;

                lexer_handle_utf8_error(vm, lexer, context);
                valid_number = false;
                goto restart_sym_num;
            }

            if (valid_number && encountered_dots <= 1 && contains_digit) {
                token.type = TOKEN_NUMBER;
            } else {
                token.type = TOKEN_SYMBOL;
            }
            goto exit;
    }

exit:
    if (parse_context_error_count(*context) != error_count) *had_error = true;
    token.length = lexer->index - token.index;
    return token;
}

#ifdef ENABLE_TESTS

#include "test.h"

static bool lexer_lex_nothing() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8(""));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END)
        goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_nil() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("()"));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_LEFT_PAREN) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_RIGHT_PAREN) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_numbers() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("1.0 .1 1. 1.2 +1.2 -1.2"));

    bool result = false;

    bool had_error = false;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_NUMBER) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_strange_symbols() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("1.0. a\" a'1. .1.2 1+1.2 q-1.2"));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_complex_escapes() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("\"\\u\" \"\\u{\" \"\\u{AV\" \"\\xA\""));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    if (parse_context_error_count(parse_context) != 4) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_string_suffix() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("\"\"a"));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    if (parse_context_error_count(parse_context) != 1) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_unterminated_string() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    lexer_init_s8(&lexer, s8("\"Hello World!\" (\"Game on!"));

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_LEFT_PAREN) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_STRING) goto cleanup;

    tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_END) goto cleanup;

    if (parse_context_error_count(parse_context) != 1) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

static bool lexer_lex_utf8_error() {
    Vm vm;
    Lexer lexer;

    if (!vm_init(&vm)) return false;
    uint8_t bytes[] = { 0x88, 0x29, 0x0a };

    s8 input;
    input.ptr = bytes;
    input.len = countof(bytes);
    lexer_init_s8(&lexer, input);

    bool result = false;

    bool had_error;
    ParseContext parse_context = NULL;
    Token tok = lexer_next_token(&vm, &lexer, &parse_context, &had_error);
    if (tok.type != TOKEN_SYMBOL) goto cleanup;

    if (parse_context_error_count(parse_context) != 1) goto cleanup;

    result = true;
cleanup:
    lexer_free(&lexer);
    vm_free(&vm);
    return result;
}

TestDefinition lexer_tests[] = {
    DEFINE_UNIT_TEST(lexer_lex_nothing, 1),
    DEFINE_UNIT_TEST(lexer_lex_nil, 1),
    DEFINE_UNIT_TEST(lexer_lex_numbers, 1),
    DEFINE_UNIT_TEST(lexer_lex_strange_symbols, 1),
    DEFINE_UNIT_TEST(lexer_lex_complex_escapes, 1),
    DEFINE_UNIT_TEST(lexer_lex_string_suffix, 1),
    DEFINE_UNIT_TEST(lexer_lex_unterminated_string, 1),
    DEFINE_UNIT_TEST(lexer_lex_utf8_error, 1),
};

TestList lexer_test_list = (TestList) {
    lexer_tests,
    countof(lexer_tests)
};

#endif
