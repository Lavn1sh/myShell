/*
 * lpsh - Lexer / Tokenizer
 * Converts raw input string into a stream of tokens.
 */

#ifndef LEXER_H
#define LEXER_H

#include "shell.h"

/* Tokenize input line. Caller must free with token_list_free(). */
TokenList lexer_tokenize(const char *input);

/* Free all memory in a TokenList. */
void token_list_free(TokenList *tl);

#endif /* LEXER_H */
