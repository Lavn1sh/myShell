/*
 * lpsh - Expansion
 * Variable, tilde, glob, and command substitution.
 */

#ifndef EXPAND_H
#define EXPAND_H

/*
 * Expand a single word in-place (returns a new malloc'd string or NULL).
 * Handles: $VAR, $?, $$, $!, ~, ~user, globs, $(cmd), `cmd`
 *
 * For glob expansion, may return multiple words.
 * expand_argv() handles the full argv case.
 */
char *expand_word(const char *word);

/*
 * Expand all words in an argv array.
 * Glob expansion may increase argc.
 * Returns a new malloc'd argv (caller frees) or NULL on error.
 * *out_argc is set to the new count.
 */
char **expand_argv(char **argv, int argc, int *out_argc);

#endif /* EXPAND_H */
