/*
 * lpsh - Prompt
 *
 * Generates a dynamic, colorized prompt with:
 *   - username@hostname
 *   - current directory (~ substituted for HOME)
 *   - git branch (if in a repo)
 *   - last command status indicator
 */

#include "prompt.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ANSI color codes (wrapped in \001 \002 for readline) */
#define RL_BOLD_GREEN   "\001\033[1;32m\002"
#define RL_BOLD_BLUE    "\001\033[1;34m\002"
#define RL_BOLD_CYAN    "\001\033[1;36m\002"
#define RL_BOLD_RED     "\001\033[1;31m\002"
#define RL_BOLD_YELLOW  "\001\033[1;33m\002"
#define RL_RESET        "\001\033[0m\002"

/* get git branch */

static const char *git_branch(void)
{
    static char branch[256];
    branch[0] = '\0';

    FILE *fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!fp) return NULL;

    if (fgets(branch, sizeof(branch), fp) != NULL) {
        /* strip newline */
        int len = strlen(branch);
        if (len > 0 && branch[len - 1] == '\n')
            branch[len - 1] = '\0';
    }
    int status = pclose(fp);
    if (status != 0) return NULL;

    return branch[0] ? branch : NULL;
}

/* shorten home dir to ~ */

static const char *shorten_cwd(char *cwd)
{
    static char shortened[MAX_LINE];
    const char *home = getenv("HOME");

    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(shortened, sizeof(shortened), "~%s", cwd + strlen(home));
        return shortened;
    }
    return cwd;
}

/* build prompt */

const char *prompt_build(void)
{
    static char prompt[MAX_LINE * 2];
    char cwd[MAX_LINE];
    char hostname[256];

    /* get info */
    const char *user = getenv("USER");
    if (!user) user = "user";

    if (gethostname(hostname, sizeof(hostname)) != 0)
        strncpy(hostname, "localhost", sizeof(hostname));

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strncpy(cwd, "?", sizeof(cwd));

    const char *dir = shorten_cwd(cwd);
    const char *branch = git_branch();

    /* status indicator */
    const char *status_icon = shell.last_status == 0
        ? RL_BOLD_GREEN "✓" RL_RESET
        : RL_BOLD_RED "✗" RL_RESET;

    /* build prompt */
    if (branch) {
        snprintf(prompt, sizeof(prompt),
                 "%s%s@%s%s:%s%s%s (%s%s%s) %s %s$ %s",
                 RL_BOLD_GREEN, user, hostname, RL_RESET,
                 RL_BOLD_BLUE, dir, RL_RESET,
                 RL_BOLD_CYAN, branch, RL_RESET,
                 status_icon,
                 RL_BOLD_YELLOW, RL_RESET);
    } else {
        snprintf(prompt, sizeof(prompt),
                 "%s%s@%s%s:%s%s%s %s %s$ %s",
                 RL_BOLD_GREEN, user, hostname, RL_RESET,
                 RL_BOLD_BLUE, dir, RL_RESET,
                 status_icon,
                 RL_BOLD_YELLOW, RL_RESET);
    }

    return prompt;
}
