/*
 * lpsh - Expansion
 *
 * Handles: $VAR, $?, $$, $!, ~, ~user, glob(3), $(cmd), `cmd`
 */

#include "expand.h"
#include "shell.h"

#include <ctype.h>
#include <glob.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* variable expansion */

static char *expand_variables(const char *word)
{
    char buf[MAX_LINE * 2];
    int  bi = 0;
    const char *p = word;

    while (*p && bi < (int)sizeof(buf) - 64) {
        if (*p == '\\' && p[1]) {
            buf[bi++] = p[1];
            p += 2;
            continue;
        }
        if (*p == '\'') {
            /* single-quoted: literal, skip variable expansion */
            p++;
            while (*p && *p != '\'') {
                if (bi < (int)sizeof(buf) - 1) buf[bi++] = *p;
                p++;
            }
            if (*p == '\'') p++;
            continue;
        }
        if (*p != '$') {
            buf[bi++] = *p++;
            continue;
        }

        /* $ expansion */
        p++; /* skip $ */
        if (*p == '?') {
            bi += snprintf(buf + bi, sizeof(buf) - bi, "%d", shell.last_status);
            p++;
        } else if (*p == '$') {
            bi += snprintf(buf + bi, sizeof(buf) - bi, "%d", shell.shell_pid);
            p++;
        } else if (*p == '!') {
            bi += snprintf(buf + bi, sizeof(buf) - bi, "%d", shell.last_bg_pid);
            p++;
        } else if (*p == '(') {
            /* command substitution $(cmd) */
            p++; /* skip ( */
            char cmd[MAX_LINE];
            int ci = 0;
            int depth = 1;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                else if (*p == ')') { depth--; if (depth == 0) break; }
                if (ci < (int)sizeof(cmd) - 1) cmd[ci++] = *p;
                p++;
            }
            cmd[ci] = '\0';
            if (*p == ')') p++;

            FILE *fp = popen(cmd, "r");
            if (fp) {
                char out[MAX_LINE];
                while (fgets(out, sizeof(out), fp)) {
                    int len = strlen(out);
                    /* strip trailing newline */
                    while (len > 0 && out[len - 1] == '\n') len--;
                    for (int i = 0; i < len && bi < (int)sizeof(buf) - 1; i++)
                        buf[bi++] = out[i];
                }
                pclose(fp);
            }
        } else if (*p == '{') {
            /* ${VAR} */
            p++;
            char varname[256];
            int vi = 0;
            while (*p && *p != '}' && vi < (int)sizeof(varname) - 1)
                varname[vi++] = *p++;
            varname[vi] = '\0';
            if (*p == '}') p++;
            const char *val = getenv(varname);
            if (val) {
                int len = strlen(val);
                for (int i = 0; i < len && bi < (int)sizeof(buf) - 1; i++)
                    buf[bi++] = val[i];
            }
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            char varname[256];
            int vi = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_') &&
                   vi < (int)sizeof(varname) - 1)
                varname[vi++] = *p++;
            varname[vi] = '\0';
            const char *val = getenv(varname);
            if (val) {
                int len = strlen(val);
                for (int i = 0; i < len && bi < (int)sizeof(buf) - 1; i++)
                    buf[bi++] = val[i];
            }
        } else {
            buf[bi++] = '$';
        }
    }

    buf[bi] = '\0';
    return lpsh_strdup(buf);
}

/* tilde expansion */

static char *expand_tilde(const char *word)
{
    if (word[0] != '~')
        return lpsh_strdup(word);

    if (word[1] == '\0' || word[1] == '/') {
        /* ~ or ~/... */
        const char *home = getenv("HOME");
        if (!home) return lpsh_strdup(word);
        char buf[MAX_LINE];
        snprintf(buf, sizeof(buf), "%s%s", home, word + 1);
        return lpsh_strdup(buf);
    }

    /* ~user */
    char username[256];
    int i = 0;
    const char *p = word + 1;
    while (*p && *p != '/' && i < (int)sizeof(username) - 1)
        username[i++] = *p++;
    username[i] = '\0';

    struct passwd *pw = getpwnam(username);
    if (!pw) return lpsh_strdup(word);

    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s%s", pw->pw_dir, p);
    return lpsh_strdup(buf);
}

/* backtick command substitution */

static char *expand_backticks(const char *word)
{
    char buf[MAX_LINE * 2];
    int bi = 0;
    const char *p = word;

    while (*p && bi < (int)sizeof(buf) - 64) {
        if (*p == '`') {
            p++;
            char cmd[MAX_LINE];
            int ci = 0;
            while (*p && *p != '`' && ci < (int)sizeof(cmd) - 1)
                cmd[ci++] = *p++;
            cmd[ci] = '\0';
            if (*p == '`') p++;

            FILE *fp = popen(cmd, "r");
            if (fp) {
                char out[MAX_LINE];
                while (fgets(out, sizeof(out), fp)) {
                    int len = strlen(out);
                    while (len > 0 && out[len - 1] == '\n') len--;
                    for (int i = 0; i < len && bi < (int)sizeof(buf) - 1; i++)
                        buf[bi++] = out[i];
                }
                pclose(fp);
            }
        } else {
            buf[bi++] = *p++;
        }
    }

    buf[bi] = '\0';
    return lpsh_strdup(buf);
}

/* single word expansion (no glob) */

char *expand_word(const char *word)
{
    if (!word || !*word)
        return lpsh_strdup("");

    /* tilde first */
    char *s1 = expand_tilde(word);

    /* then variables */
    char *s2 = expand_variables(s1);
    free(s1);

    /* then backticks */
    char *s3 = expand_backticks(s2);
    free(s2);

    return s3;
}

/* has glob chars? */

static int has_glob(const char *s)
{
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == '[')
            return 1;
    }
    return 0;
}

/* full argv expansion */

char **expand_argv(char **argv, int argc, int *out_argc)
{
    char **result = malloc(sizeof(char *) * (argc * 16 + 1));
    int ri = 0;

    for (int i = 0; i < argc; i++) {
        char *expanded = expand_word(argv[i]);

        if (has_glob(expanded)) {
            glob_t g;
            int ret = glob(expanded, GLOB_NOCHECK | GLOB_TILDE, NULL, &g);
            if (ret == 0) {
                for (size_t j = 0; j < g.gl_pathc; j++)
                    result[ri++] = lpsh_strdup(g.gl_pathv[j]);
                globfree(&g);
            } else {
                result[ri++] = expanded;
                expanded = NULL; /* ownership transferred */
            }
        } else {
            result[ri++] = expanded;
            expanded = NULL;
        }

        free(expanded);
    }

    result[ri] = NULL;
    *out_argc = ri;
    return result;
}
