/*
 * Core types and shared state
 */

#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <termios.h>

/* Limits */
#define MAX_LINE        4096
#define MAX_ARGS        256
#define MAX_TOKENS      512
#define MAX_PIPES       64
#define MAX_JOBS        64
#define MAX_ALIASES     256
#define MAX_ALIAS_LEN   256
#define HISTORY_FILE    ".lpsh_history"
#define RC_FILE         ".lpshrc"

/* Token types */
typedef enum {
    TOK_WORD,           /* plain word / argument              */
    TOK_PIPE,           /* |                                  */
    TOK_AND,            /* &&                                 */
    TOK_OR,             /* ||                                 */
    TOK_SEMI,           /* ;                                  */
    TOK_BG,             /* &                                  */
    TOK_REDIR_IN,       /* <                                  */
    TOK_REDIR_OUT,      /* >                                  */
    TOK_REDIR_APPEND,   /* >>                                 */
    TOK_REDIR_ERR,      /* 2>                                 */
    TOK_REDIR_ERR_APPEND, /* 2>>                              */
    TOK_HEREDOC,        /* <<                                 */
    TOK_NEWLINE,        /* end of input                       */
} TokenType;

typedef struct {
    TokenType type;
    char     *value;    /* heap-allocated string for TOK_WORD */
} Token;

typedef struct {
    Token  *tokens;
    int     count;
    int     capacity;
} TokenList;

/* Redirect descriptor */
typedef struct {
    char *in_file;           /* < file            */
    char *out_file;          /* > or >> file       */
    int   out_append;        /* true if >>         */
    char *err_file;          /* 2> or 2>> file     */
    int   err_append;        /* true if 2>>        */
    char *heredoc_delim;     /* << DELIM           */
    char *heredoc_body;      /* collected heredoc text */
} Redirect;

/* Single command (one stage of a pipeline) */
typedef struct {
    char    **argv;          /* null-terminated argument vector */
    int       argc;
    Redirect  redir;
} Command;

/* Pipeline: cmd1 | cmd2 | cmd3 */
typedef enum { CONN_NONE, CONN_AND, CONN_OR, CONN_SEMI } Connector;

typedef struct {
    Command *commands;       /* array of pipeline stages    */
    int      num_commands;
    int      background;     /* trailing &                  */
} Pipeline;

/* Command list: pipeline1 && pipeline2 || pipeline3 */
typedef struct {
    Pipeline  *pipelines;
    Connector *connectors;   /* connectors[i] joins pipeline[i] to [i+1] */
    int        count;
} CommandList;

/* Job control */
typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } JobState;

typedef struct {
    int       id;                /* job number [1], [2], ... */
    pid_t     pgid;              /* process group id         */
    pid_t    *pids;              /* array of pids            */
    int       num_pids;
    JobState  state;
    char      cmdline[MAX_LINE]; /* display string           */
} Job;

/* Alias */
typedef struct {
    char name[MAX_ALIAS_LEN];
    char value[MAX_LINE];
} Alias;

/* Global shell state */
typedef struct {
    int       last_status;       /* $?                       */
    pid_t     shell_pid;         /* $$                       */
    pid_t     last_bg_pid;       /* $!                       */
    int       interactive;       /* is stdin a tty?          */
    int       running;           /* main loop flag           */
    struct termios orig_termios; /* saved terminal state     */

    /* Job table */
    Job       jobs[MAX_JOBS];
    int       num_jobs;
    int       next_job_id;

    /* Alias table */
    Alias     aliases[MAX_ALIASES];
    int       num_aliases;

    /* Previous directory for cd - */
    char      prev_dir[MAX_LINE];
} ShellState;

/* The single global instance */
extern ShellState shell;

/* Utility */
char *lpsh_strdup(const char *s);

#endif /* SHELL_H */
