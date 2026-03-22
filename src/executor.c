/*
 * lpsh - Executor
 *
 * Handles:
 *   - Single command execution with redirections
 *   - Pipeline execution (cmd1 | cmd2 | ...)
 *   - Command list execution (&&, ||, ;)
 *   - Background job tracking
 *   - Process group management for job control
 */

#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include "jobs.h"
#include "shell.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* setup redirections in child process */

static int setup_redirections(Redirect *redir)
{
    if (redir->in_file) {
        int fd = open(redir->in_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "lpsh: %s: %s\n", redir->in_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (redir->heredoc_body) {
        int pipefd[2];
        if (pipe(pipefd) < 0) return -1;
        write(pipefd[1], redir->heredoc_body, strlen(redir->heredoc_body));
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
    }

    if (redir->out_file) {
        int flags = O_WRONLY | O_CREAT | (redir->out_append ? O_APPEND : O_TRUNC);
        int fd = open(redir->out_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "lpsh: %s: %s\n", redir->out_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (redir->err_file) {
        int flags = O_WRONLY | O_CREAT | (redir->err_append ? O_APPEND : O_TRUNC);
        int fd = open(redir->err_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "lpsh: %s: %s\n", redir->err_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    return 0;
}

/* expand a command's arguments */

static int expand_command(Command *cmd)
{
    if (cmd->argc == 0) return 0;

    int new_argc;
    char **new_argv = expand_argv(cmd->argv, cmd->argc, &new_argc);
    if (!new_argv) return -1;

    /* free old argv */
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);

    cmd->argv = new_argv;
    cmd->argc = new_argc;

    /* expand redirection filenames */
    if (cmd->redir.in_file) {
        char *exp = expand_word(cmd->redir.in_file);
        free(cmd->redir.in_file);
        cmd->redir.in_file = exp;
    }
    if (cmd->redir.out_file) {
        char *exp = expand_word(cmd->redir.out_file);
        free(cmd->redir.out_file);
        cmd->redir.out_file = exp;
    }
    if (cmd->redir.err_file) {
        char *exp = expand_word(cmd->redir.err_file);
        free(cmd->redir.err_file);
        cmd->redir.err_file = exp;
    }

    return 0;
}

/* build command line string for display */

static void pipeline_to_string(Pipeline *pl, char *buf, int buflen)
{
    int pos = 0;
    for (int i = 0; i < pl->num_commands && pos < buflen - 1; i++) {
        if (i > 0)
            pos += snprintf(buf + pos, buflen - pos, " | ");
        for (int j = 0; pl->commands[i].argv[j] && pos < buflen - 1; j++) {
            if (j > 0) buf[pos++] = ' ';
            pos += snprintf(buf + pos, buflen - pos, "%s",
                            pl->commands[i].argv[j]);
        }
    }
    if (pl->background && pos < buflen - 2) {
        buf[pos++] = ' ';
        buf[pos++] = '&';
    }
    buf[pos] = '\0';
}

/* execute a single command (no pipes) */

static int exec_single(Command *cmd, int background, const char *cmdline_str)
{
    /* expand */
    expand_command(cmd);
    if (cmd->argc == 0) return 0;

    /* check builtins first (foreground only) */
    if (!background) {
        int status;
        int is_builtin = 0;

        /* check if this is a builtin without executing */
        const char *builtin_names[] = {
            "cd", "exit", "pwd", "echo", "export", "unset", "env",
            "alias", "unalias", "history", "jobs", "fg", "bg",
            "source", ".", "type", NULL
        };
        for (int i = 0; builtin_names[i]; i++) {
            if (strcmp(cmd->argv[0], builtin_names[i]) == 0) {
                is_builtin = 1;
                break;
            }
        }

        if (is_builtin) {
            /* save original fds */
            int saved_stdin  = -1, saved_stdout = -1, saved_stderr = -1;
            int has_redir = cmd->redir.in_file || cmd->redir.out_file ||
                            cmd->redir.err_file || cmd->redir.heredoc_body;

            if (has_redir) {
                saved_stdin  = dup(STDIN_FILENO);
                saved_stdout = dup(STDOUT_FILENO);
                saved_stderr = dup(STDERR_FILENO);
                if (setup_redirections(&cmd->redir) < 0) {
                    if (saved_stdin  >= 0) close(saved_stdin);
                    if (saved_stdout >= 0) close(saved_stdout);
                    if (saved_stderr >= 0) close(saved_stderr);
                    return 1;
                }
            }

            builtin_exec(cmd, &status);

            /* restore fds */
            if (has_redir) {
                fflush(stdout);
                fflush(stderr);
                if (saved_stdin  >= 0) { dup2(saved_stdin,  STDIN_FILENO);  close(saved_stdin);  }
                if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
                if (saved_stderr >= 0) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
            }

            return status;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("lpsh: fork");
        return 1;
    }

    if (pid == 0) {
        /* child */
        /* reset signals to default */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        /* set process group */
        setpgid(0, 0);
        if (!background && shell.interactive)
            tcsetpgrp(STDIN_FILENO, getpgrp());

        if (setup_redirections(&cmd->redir) < 0)
            _exit(1);

        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "lpsh: %s: %s\n", cmd->argv[0], strerror(errno));
        _exit(127);
    }

    /* parent */
    setpgid(pid, pid);

    if (background) {
        shell.last_bg_pid = pid;
        job_add(pid, &pid, 1, cmdline_str, 1);
        return 0;
    }

    /* foreground */
    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, pid);

    int status;
    pid_t result;
    do {
        result = waitpid(pid, &status, WUNTRACED);
    } while (result == -1 && errno == EINTR);

    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFSTOPPED(status)) {
        Job *job = job_find_by_pgid(pid);
        if (!job)
            job_add(pid, &pid, 1, cmdline_str, 0);
        else
            job->state = JOB_STOPPED;
        printf("\n[%d] Stopped\t\t%s\n",
               job_find_by_pgid(pid)->id, cmdline_str);
        return 128 + WSTOPSIG(status);
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

/* execute a pipeline */

static int exec_pipeline(Pipeline *pl)
{
    char cmdstr[MAX_LINE];
    pipeline_to_string(pl, cmdstr, sizeof(cmdstr));

    /* single command: no pipes needed */
    if (pl->num_commands == 1) {
        return exec_single(&pl->commands[0], pl->background, cmdstr);
    }

    /* expand all commands */
    for (int i = 0; i < pl->num_commands; i++)
        expand_command(&pl->commands[i]);

    int n = pl->num_commands;
    int pipefds[2 * (n - 1)];
    pid_t pids[n];

    /* create pipes */
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("lpsh: pipe");
            return 1;
        }
    }

    pid_t pgid = 0;

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("lpsh: fork");
            return 1;
        }

        if (pids[i] == 0) {
            /* child */
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);

            /* wire pipes */
            if (i > 0)
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            if (i < n - 1)
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);

            /* close all pipe fds */
            for (int j = 0; j < 2 * (n - 1); j++)
                close(pipefds[j]);

            /* redirections */
            setup_redirections(&pl->commands[i].redir);

            /* exec */
            execvp(pl->commands[i].argv[0], pl->commands[i].argv);
            fprintf(stderr, "lpsh: %s: %s\n",
                    pl->commands[i].argv[0], strerror(errno));
            _exit(127);
        }

        /* parent: set process group */
        if (i == 0) pgid = pids[0];
        setpgid(pids[i], pgid);
    }

    /* close all pipe fds in parent */
    for (int j = 0; j < 2 * (n - 1); j++)
        close(pipefds[j]);

    if (pl->background) {
        shell.last_bg_pid = pgid;
        job_add(pgid, pids, n, cmdstr, 1);
        return 0;
    }

    /* foreground: give terminal to pipeline group */
    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, pgid);

    /* wait for all children */
    int last_status = 0;
    for (int i = 0; i < n; i++) {
        int status;
        pid_t result;
        do {
            result = waitpid(pids[i], &status, WUNTRACED);
        } while (result == -1 && errno == EINTR);

        if (result > 0) {
            if (WIFSTOPPED(status)) {
                job_add(pgid, pids, n, cmdstr, 0);
                Job *job = job_find_by_pgid(pgid);
                if (job) job->state = JOB_STOPPED;
                printf("\n[%d] Stopped\t\t%s\n",
                       job_find_by_pgid(pgid)->id, cmdstr);
                if (shell.interactive)
                    tcsetpgrp(STDIN_FILENO, getpgrp());
                return 128 + WSTOPSIG(status);
            }
            if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_status = 128 + WTERMSIG(status);
        }
    }

    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, getpgrp());

    return last_status;
}

/* execute a command list */

int executor_run(CommandList *cl)
{
    if (cl->count == 0) return 0;

    int status = exec_pipeline(&cl->pipelines[0]);
    shell.last_status = status;

    for (int i = 1; i < cl->count; i++) {
        Connector conn = cl->connectors[i - 1];

        if (conn == CONN_AND && status != 0)
            continue;
        if (conn == CONN_OR && status == 0)
            continue;
        /* CONN_SEMI: always execute */

        status = exec_pipeline(&cl->pipelines[i]);
        shell.last_status = status;
    }

    return status;
}
