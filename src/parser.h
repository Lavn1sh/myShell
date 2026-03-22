/*
 * lpsh - Parser
 * Converts token stream into command structures.
 */

#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

/*
 * Parse a token list into a CommandList.
 * Returns 0 on success, -1 on parse error.
 * Caller must free with command_list_free().
 */
int parser_parse(const TokenList *tl, CommandList *cl);

/* Free all memory in a CommandList. */
void command_list_free(CommandList *cl);

#endif /* PARSER_H */
