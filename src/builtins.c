/*
 * lpsh - Built-in Commands
 *
 * cd, exit, pwd, echo, export, unset, env, alias, unalias,
 * history, jobs, fg, bg, source, type
 */

#include "builtins.h"
#include "alias.h"
#include "jobs.h"
#include "rc.h"
#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* cd */

static int builtin_cd(Command *cmd)
{
    const char *dir = cmd->argv[1];
    char cwd[MAX_LINE];

    /* save current dir before cd */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '\0';
    }

    if (!dir) {
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "lpsh: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(dir, "-") == 0) {
        if (shell.prev_dir[0] == '\0') {
            fprintf(stderr, "lpsh: cd: OLDPWD not set\n");
            return 1;
        }
        dir = shell.prev_dir;
        printf("%s\n", dir);
    }

    if (chdir(dir) != 0) {
        fprintf(stderr, "lpsh: cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    /* update OLDPWD and prev_dir */
    strncpy(shell.prev_dir, cwd, MAX_LINE - 1);
    shell.prev_dir[MAX_LINE - 1] = '\0';

    /* update PWD */
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        setenv("PWD", cwd, 1);

    return 0;
}

/* exit */

static int builtin_exit(Command *cmd)
{
    int code = 0;
    if (cmd->argv[1])
        code = atoi(cmd->argv[1]);
    shell.running = 0;
    shell.last_status = code;
    return code;
}

/* pwd */

static int builtin_pwd(Command *cmd)
{
    (void)cmd;
    char cwd[MAX_LINE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;
    }
    perror("lpsh: pwd");
    return 1;
}

/* echo */

static int builtin_echo(Command *cmd)
{
    int newline = 1;
    int start = 1;

    if (cmd->argv[1] && strcmp(cmd->argv[1], "-n") == 0) {
        newline = 0;
        start = 2;
    }

    for (int i = start; cmd->argv[i]; i++) {
        if (i > start) printf(" ");
        printf("%s", cmd->argv[i]);
    }
    if (newline) printf("\n");

    return 0;
}

/* export */

static int builtin_export(Command *cmd)
{
    if (!cmd->argv[1]) {
        /* print all exported vars */
        extern char **environ;
        for (char **env = environ; *env; env++)
            printf("declare -x %s\n", *env);
        return 0;
    }

    for (int i = 1; cmd->argv[i]; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            *eq = '\0';
            setenv(cmd->argv[i], eq + 1, 1);
            *eq = '=';
        } else {
            /* export existing var (no-op for simplicity) */
        }
    }
    return 0;
}

/* unset */

static int builtin_unset(Command *cmd)
{
    for (int i = 1; cmd->argv[i]; i++)
        unsetenv(cmd->argv[i]);
    return 0;
}

/* env */

static int builtin_env(Command *cmd)
{
    (void)cmd;
    extern char **environ;
    for (char **env = environ; *env; env++)
        printf("%s\n", *env);
    return 0;
}

/* alias */

static int builtin_alias(Command *cmd)
{
    if (!cmd->argv[1]) {
        alias_list();
        return 0;
    }
    for (int i = 1; cmd->argv[i]; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            *eq = '\0';
            alias_set(cmd->argv[i], eq + 1);
            *eq = '=';
        } else {
            const char *val = alias_get(cmd->argv[i]);
            if (val)
                printf("alias %s='%s'\n", cmd->argv[i], val);
            else
                fprintf(stderr, "lpsh: alias: %s: not found\n", cmd->argv[i]);
        }
    }
    return 0;
}

/* unalias */

static int builtin_unalias(Command *cmd)
{
    if (!cmd->argv[1]) {
        fprintf(stderr, "lpsh: unalias: usage: unalias name\n");
        return 1;
    }
    for (int i = 1; cmd->argv[i]; i++) {
        if (alias_unset(cmd->argv[i]) < 0)
            fprintf(stderr, "lpsh: unalias: %s: not found\n", cmd->argv[i]);
    }
    return 0;
}

/* history */

static int builtin_history(Command *cmd)
{
    (void)cmd;
    HIST_ENTRY **the_list = history_list();
    if (!the_list) return 0;

    for (int i = 0; the_list[i]; i++)
        printf("%5d  %s\n", i + history_base, the_list[i]->line);

    return 0;
}

/* jobs */

static int builtin_jobs(Command *cmd)
{
    (void)cmd;
    jobs_list();
    return 0;
}

/* fg */

static int builtin_fg(Command *cmd)
{
    int id;
    if (cmd->argv[1]) {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%') arg++;
        id = atoi(arg);
    } else {
        /* find most recent job */
        id = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (shell.jobs[i].id > id)
                id = shell.jobs[i].id;
        }
    }

    Job *job = job_find(id);
    if (!job) {
        fprintf(stderr, "lpsh: fg: %%%d: no such job\n", id);
        return 1;
    }

    printf("%s\n", job->cmdline);
    return job_foreground(job);
}

/* bg */

static int builtin_bg(Command *cmd)
{
    int id;
    if (cmd->argv[1]) {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%') arg++;
        id = atoi(arg);
    } else {
        /* find most recent stopped job */
        id = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (shell.jobs[i].id > id && shell.jobs[i].state == JOB_STOPPED)
                id = shell.jobs[i].id;
        }
    }

    Job *job = job_find(id);
    if (!job) {
        fprintf(stderr, "lpsh: bg: %%%d: no such job\n", id);
        return 1;
    }

    return job_background(job);
}

/* source */

static int builtin_source(Command *cmd)
{
    if (!cmd->argv[1]) {
        fprintf(stderr, "lpsh: source: filename argument required\n");
        return 1;
    }
    return rc_load(cmd->argv[1]);
}

/* type */

static int builtin_type(Command *cmd)
{
    if (!cmd->argv[1]) {
        fprintf(stderr, "lpsh: type: argument required\n");
        return 1;
    }

    for (int i = 1; cmd->argv[i]; i++) {
        const char *name = cmd->argv[i];

        /* check builtins */
        const char *builtins[] = {
            "cd", "exit", "pwd", "echo", "export", "unset", "env",
            "alias", "unalias", "history", "jobs", "fg", "bg",
            "source", "type", NULL
        };
        int is_builtin = 0;
        for (int j = 0; builtins[j]; j++) {
            if (strcmp(name, builtins[j]) == 0) {
                printf("%s is a shell builtin\n", name);
                is_builtin = 1;
                break;
            }
        }
        if (is_builtin) continue;

        /* check aliases */
        const char *alias_val = alias_get(name);
        if (alias_val) {
            printf("%s is aliased to '%s'\n", name, alias_val);
            continue;
        }

        /* check PATH */
        char *path_env = getenv("PATH");
        if (path_env) {
            char path_copy[MAX_LINE];
            strncpy(path_copy, path_env, MAX_LINE - 1);
            path_copy[MAX_LINE - 1] = '\0';

            char *dir = strtok(path_copy, ":");
            int found = 0;
            while (dir) {
                char full[MAX_LINE];
                snprintf(full, sizeof(full), "%s/%s", dir, name);
                if (access(full, X_OK) == 0) {
                    printf("%s is %s\n", name, full);
                    found = 1;
                    break;
                }
                dir = strtok(NULL, ":");
            }
            if (found) continue;
        }

        fprintf(stderr, "lpsh: type: %s: not found\n", name);
    }
    return 0;
}

/* dispatch */

typedef struct {
    const char *name;
    int (*func)(Command *cmd);
} BuiltinEntry;

static BuiltinEntry builtin_table[] = {
    { "cd",       builtin_cd       },
    { "exit",     builtin_exit     },
    { "pwd",      builtin_pwd      },
    { "echo",     builtin_echo     },
    { "export",   builtin_export   },
    { "unset",    builtin_unset    },
    { "env",      builtin_env      },
    { "alias",    builtin_alias    },
    { "unalias",  builtin_unalias  },
    { "history",  builtin_history  },
    { "jobs",     builtin_jobs     },
    { "fg",       builtin_fg       },
    { "bg",       builtin_bg       },
    { "source",   builtin_source   },
    { ".",        builtin_source   },
    { "type",     builtin_type     },
    { NULL, NULL }
};

int builtin_exec(Command *cmd, int *status)
{
    if (!cmd->argv[0])
        return 0;

    for (int i = 0; builtin_table[i].name; i++) {
        if (strcmp(cmd->argv[0], builtin_table[i].name) == 0) {
            *status = builtin_table[i].func(cmd);
            return 1;
        }
    }
    return 0;
}
