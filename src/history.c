/*
 * lpsh - History
 *
 * Uses GNU Readline's history API for persistent command history
 * stored in ~/.lpsh_history. Supports history expansion (!! !n !string).
 */

#include "history.h"
#include "shell.h"

#include <stdio.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* get history file path */

static const char *history_path(void)
{
    static char path[MAX_LINE];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILE);
    return path;
}

/* load */

void history_load(void)
{
    using_history();
    stifle_history(1000);
    read_history(history_path());
}

/* save */

void history_save(void)
{
    write_history(history_path());
}

/* expand !! !n !string */

char *history_expand_line(const char *line)
{
    /* Quick check: does line contain ! at all? */
    if (!strchr(line, '!'))
        return lpsh_strdup(line);

    char *expansion = NULL;
    int result = history_expand((char *)line, &expansion);

    if (result == -1) {
        /* error */
        fprintf(stderr, "lpsh: %s\n", expansion);
        free(expansion);
        return NULL;
    }
    if (result == 1) {
        /* expansion happened, print it */
        printf("%s\n", expansion);
    }
    /* result == 0: no expansion, or result == 2: display only */

    return expansion;
}