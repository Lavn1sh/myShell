/*
 * lpsh - Executor
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "shell.h"

/*
 * Execute a complete command list (pipelines with connectors).
 * Returns the exit status.
 */
int executor_run(CommandList *cl);

#endif /* EXECUTOR_H */
