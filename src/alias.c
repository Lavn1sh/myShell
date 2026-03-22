/*
 * lpsh - Alias System
 *
 * Simple alias table with recursive expansion (with loop detection).
 */

#include "alias.h"
#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* set */

int alias_set(const char *name, const char *value)
{
    /* update existing */
    for (int i = 0; i < shell.num_aliases; i++) {
        if (strcmp(shell.aliases[i].name, name) == 0) {
            strncpy(shell.aliases[i].value, value, MAX_LINE - 1);
            shell.aliases[i].value[MAX_LINE - 1] = '\0';
            return 0;
        }
    }
    /* add new */
    if (shell.num_aliases >= MAX_ALIASES) {
        fprintf(stderr, "lpsh: alias table full\n");
        return -1;
    }
    strncpy(shell.aliases[shell.num_aliases].name, name, MAX_ALIAS_LEN - 1);
    shell.aliases[shell.num_aliases].name[MAX_ALIAS_LEN - 1] = '\0';
    strncpy(shell.aliases[shell.num_aliases].value, value, MAX_LINE - 1);
    shell.aliases[shell.num_aliases].value[MAX_LINE - 1] = '\0';
    shell.num_aliases++;
    return 0;
}

/* unset */

int alias_unset(const char *name)
{
    for (int i = 0; i < shell.num_aliases; i++) {
        if (strcmp(shell.aliases[i].name, name) == 0) {
            /* shift down */
            for (int j = i; j < shell.num_aliases - 1; j++)
                shell.aliases[j] = shell.aliases[j + 1];
            shell.num_aliases--;
            return 0;
        }
    }
    return -1;
}

/* get */

const char *alias_get(const char *name)
{
    for (int i = 0; i < shell.num_aliases; i++) {
        if (strcmp(shell.aliases[i].name, name) == 0)
            return shell.aliases[i].value;
    }
    return NULL;
}

/* list */

void alias_list(void)
{
    for (int i = 0; i < shell.num_aliases; i++)
        printf("alias %s='%s'\n", shell.aliases[i].name, shell.aliases[i].value);
}

/* expand aliases in first word */

char *alias_expand(const char *input)
{
    /* Extract the first word */
    const char *p = input;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return lpsh_strdup(input);

    char first[MAX_ALIAS_LEN];
    int fi = 0;
    const char *word_start = p;
    while (*p && !isspace((unsigned char)*p) && fi < MAX_ALIAS_LEN - 1) {
        /* stop at operators too */
        if (*p == '|' || *p == '&' || *p == ';' || *p == '>' || *p == '<')
            break;
        first[fi++] = *p++;
    }
    first[fi] = '\0';

    const char *val = alias_get(first);
    if (!val)
        return lpsh_strdup(input);

    /* Build expanded string: leading whitespace + alias expansion + rest */
    char buf[MAX_LINE * 2];
    int leading = (int)(word_start - input);
    snprintf(buf, sizeof(buf), "%.*s%s%s", leading, input, val, p);
    return lpsh_strdup(buf);
}
