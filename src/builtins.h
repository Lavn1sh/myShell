/*
 * lpsh - Built-in Commands
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

/*
 * Check if a command is a builtin and execute it.
 * Returns: 1 if it was a builtin (executed), 0 if not a builtin.
 * Sets *status to the exit status of the builtin.
 */
int builtin_exec(Command *cmd, int *status);

#endif /* BUILTINS_H */
