/*
 * lpsh - A Unix Shell
 *
 * Entry point: REPL loop, signal setup, initialization.
 */

#include "alias.h"
#include "executor.h"
#include "expand.h"
#include "history.h"
#include "jobs.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "rc.h"
#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* global shell state */

ShellState shell;

/* lpsh_strdup (safe strdup) */

char *lpsh_strdup(const char *s)
{
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) {
        perror("lpsh: strdup");
        exit(1);
    }
    return dup;
}

/* signal handlers */

static void sigint_handler(int sig)
{
    (void)sig;
    /* Just print a new prompt on Ctrl-C */
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

static void sigtstp_handler(int sig)
{
    (void)sig;
    /* Ignore SIGTSTP in the shell itself - only forward to children */
}

static void sigchld_handler(int sig)
{
    (void)sig;
    /* Reap background jobs */
    jobs_check_status();
}

static void setup_signals(void)
{
    struct sigaction sa;

    /* SIGINT */
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    /* SIGTSTP */
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    /* SIGCHLD */
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore SIGTTOU so tcsetpgrp doesn't stop us */
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

/* tab completion generator */

static char *command_generator(const char *text, int state)
{
    /* Use default readline filename completion */
    return rl_filename_completion_function(text, state);
}

static char **command_completion(const char *text, int start, int end)
{
    (void)end;
    (void)start;
    rl_attempted_completion_over = 0;
    return rl_completion_matches(text, command_generator);
}

/* initialization */

static void shell_init(void)
{
    memset(&shell, 0, sizeof(shell));
    shell.shell_pid   = getpid();
    shell.interactive = isatty(STDIN_FILENO);
    shell.running     = 1;
    shell.next_job_id = 1;

    if (shell.interactive) {
        /* Put ourselves in our own process group */
        setpgid(0, 0);
        tcsetpgrp(STDIN_FILENO, getpgrp());
        tcgetattr(STDIN_FILENO, &shell.orig_termios);
    }

    setup_signals();

    /* readline setup */
    rl_attempted_completion_function = command_completion;

    /* load history */
    history_load();

    /* load rc file */
    rc_load_default();
}

static void shell_cleanup(void)
{
    history_save();
    if (shell.interactive)
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell.orig_termios);
}

/* main REPL */

int main(void)
{
    shell_init();

    while (shell.running) {
        /* check for finished background jobs */
        jobs_check_status();

        /* get prompt and read input */
        const char *ps1 = shell.interactive ? prompt_build() : "";
        char *line = readline(ps1);

        if (!line) {
            /* EOF (Ctrl-D) */
            if (shell.interactive) printf("\n");
            break;
        }

        /* skip empty lines */
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        /* history expansion (!! !n !string) */
        char *expanded_line = history_expand_line(line);
        free(line);
        if (!expanded_line)
            continue;

        /* add to history */
        add_history(expanded_line);

        /* alias expansion */
        char *aliased = alias_expand(expanded_line);
        free(expanded_line);

        /* lex */
        TokenList tl = lexer_tokenize(aliased);
        free(aliased);

        /* parse */
        CommandList cl;
        if (parser_parse(&tl, &cl) < 0) {
            token_list_free(&tl);
            continue;
        }

        /* execute */
        shell.last_status = executor_run(&cl);

        /* cleanup */
        command_list_free(&cl);
        token_list_free(&tl);
    }

    shell_cleanup();
    return shell.last_status;
}
