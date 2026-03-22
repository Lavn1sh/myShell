/*
 * lpsh - RC File
 *
 * Reads and executes ~/.lpshrc on startup.
 * Each line is processed through the normal shell pipeline.
 */

#include "rc.h"
#include "alias.h"
#include "executor.h"
#include "expand.h"
#include "lexer.h"
#include "parser.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* execute a single line through the shell pipeline */

static int rc_exec_line(const char *line)
{
    /* skip empty lines and comments */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#' || *p == '\n')
        return 0;

    /* make a mutable copy and strip newline */
    char buf[MAX_LINE];
    strncpy(buf, line, MAX_LINE - 1);
    buf[MAX_LINE - 1] = '\0';
    int len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';

    /* alias expansion */
    char *expanded = alias_expand(buf);

    /* lex */
    TokenList tl = lexer_tokenize(expanded);
    free(expanded);

    /* parse */
    CommandList cl;
    if (parser_parse(&tl, &cl) < 0) {
        token_list_free(&tl);
        return -1;
    }

    /* execute */
    int status = executor_run(&cl);

    command_list_free(&cl);
    token_list_free(&tl);

    return status;
}

/* load a file */

int rc_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        rc_exec_line(line);
    }

    fclose(fp);
    return 0;
}

/* load default rc */

int rc_load_default(void)
{
    const char *home = getenv("HOME");
    if (!home) return -1;

    char path[MAX_LINE];
    snprintf(path, sizeof(path), "%s/%s", home, RC_FILE);
    return rc_load(path);
}
