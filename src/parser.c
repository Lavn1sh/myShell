/*
 * lpsh - Parser
 *
 * Grammar (simplified):
 *   command_list : pipeline ((&&, ||, ;) pipeline)*
 *   pipeline     : command (| command)* [&]
 *   command      : WORD+ [redirections]
 */

#include "parser.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helpers */

static Command command_new(void)
{
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.argv = calloc(MAX_ARGS, sizeof(char *));
    cmd.argc = 0;
    return cmd;
}

static void command_free(Command *cmd)
{
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    free(cmd->redir.in_file);
    free(cmd->redir.out_file);
    free(cmd->redir.err_file);
    free(cmd->redir.heredoc_delim);
    free(cmd->redir.heredoc_body);
}

static Pipeline pipeline_new(void)
{
    Pipeline pl;
    memset(&pl, 0, sizeof(pl));
    pl.commands = malloc(sizeof(Command) * MAX_PIPES);
    pl.num_commands = 0;
    pl.background = 0;
    return pl;
}

static void pipeline_free(Pipeline *pl)
{
    for (int i = 0; i < pl->num_commands; i++)
        command_free(&pl->commands[i]);
    free(pl->commands);
}

/* parser state */

typedef struct {
    const TokenList *tl;
    int pos;
} ParserState;

static Token *peek(ParserState *ps)
{
    if (ps->pos < ps->tl->count)
        return &ps->tl->tokens[ps->pos];
    return NULL;
}

static Token *advance(ParserState *ps)
{
    if (ps->pos < ps->tl->count)
        return &ps->tl->tokens[ps->pos++];
    return NULL;
}

static int at_end(ParserState *ps)
{
    Token *t = peek(ps);
    return !t || t->type == TOK_NEWLINE;
}

/* parse a single command (words + redirections) */

static int parse_command(ParserState *ps, Command *cmd)
{
    *cmd = command_new();

    while (!at_end(ps)) {
        Token *t = peek(ps);
        if (!t) break;

        /* stop at pipeline / list separators */
        if (t->type == TOK_PIPE || t->type == TOK_AND ||
            t->type == TOK_OR  || t->type == TOK_SEMI ||
            t->type == TOK_BG)
            break;

        /* redirections */
        if (t->type == TOK_REDIR_IN) {
            advance(ps);
            Token *file = advance(ps);
            if (!file || file->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '<'\n");
                return -1;
            }
            free(cmd->redir.in_file);
            cmd->redir.in_file = lpsh_strdup(file->value);
            continue;
        }
        if (t->type == TOK_REDIR_OUT) {
            advance(ps);
            Token *file = advance(ps);
            if (!file || file->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '>'\n");
                return -1;
            }
            free(cmd->redir.out_file);
            cmd->redir.out_file = lpsh_strdup(file->value);
            cmd->redir.out_append = 0;
            continue;
        }
        if (t->type == TOK_REDIR_APPEND) {
            advance(ps);
            Token *file = advance(ps);
            if (!file || file->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '>>'\n");
                return -1;
            }
            free(cmd->redir.out_file);
            cmd->redir.out_file = lpsh_strdup(file->value);
            cmd->redir.out_append = 1;
            continue;
        }
        if (t->type == TOK_REDIR_ERR) {
            advance(ps);
            Token *file = advance(ps);
            if (!file || file->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '2>'\n");
                return -1;
            }
            free(cmd->redir.err_file);
            cmd->redir.err_file = lpsh_strdup(file->value);
            cmd->redir.err_append = 0;
            continue;
        }
        if (t->type == TOK_REDIR_ERR_APPEND) {
            advance(ps);
            Token *file = advance(ps);
            if (!file || file->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '2>>'\n");
                return -1;
            }
            free(cmd->redir.err_file);
            cmd->redir.err_file = lpsh_strdup(file->value);
            cmd->redir.err_append = 1;
            continue;
        }
        if (t->type == TOK_HEREDOC) {
            advance(ps);
            Token *delim = advance(ps);
            if (!delim || delim->type != TOK_WORD) {
                fprintf(stderr, "lpsh: syntax error near '<<'\n");
                return -1;
            }
            free(cmd->redir.heredoc_delim);
            cmd->redir.heredoc_delim = lpsh_strdup(delim->value);
            /* heredoc body is collected later by the caller */
            continue;
        }

        /* regular word */
        if (t->type == TOK_WORD) {
            advance(ps);
            cmd->argv[cmd->argc++] = lpsh_strdup(t->value);
            continue;
        }

        /* unexpected token */
        break;
    }

    cmd->argv[cmd->argc] = NULL;
    return (cmd->argc > 0 || cmd->redir.in_file || cmd->redir.heredoc_delim) ? 0 : -1;
}

/* parse a pipeline */

static int parse_pipeline(ParserState *ps, Pipeline *pl)
{
    *pl = pipeline_new();

    Command cmd;
    if (parse_command(ps, &cmd) < 0) {
        pipeline_free(pl);
        return -1;
    }
    pl->commands[pl->num_commands++] = cmd;

    while (!at_end(ps)) {
        Token *t = peek(ps);
        if (!t || t->type != TOK_PIPE)
            break;
        advance(ps); /* consume | */

        if (at_end(ps)) {
            fprintf(stderr, "lpsh: syntax error near '|'\n");
            pipeline_free(pl);
            return -1;
        }

        if (parse_command(ps, &cmd) < 0) {
            pipeline_free(pl);
            return -1;
        }
        pl->commands[pl->num_commands++] = cmd;
    }

    /* check for trailing & */
    Token *t = peek(ps);
    if (t && t->type == TOK_BG) {
        pl->background = 1;
        advance(ps);
    }

    return 0;
}

/* parse command list */

int parser_parse(const TokenList *tl, CommandList *cl)
{
    memset(cl, 0, sizeof(*cl));
    cl->pipelines  = malloc(sizeof(Pipeline) * MAX_PIPES);
    cl->connectors = malloc(sizeof(Connector) * MAX_PIPES);
    cl->count      = 0;

    ParserState ps = { .tl = tl, .pos = 0 };

    /* skip leading separators */
    while (!at_end(&ps)) {
        Token *t = peek(&ps);
        if (t && t->type == TOK_SEMI)
            advance(&ps);
        else
            break;
    }

    if (at_end(&ps)) {
        return 0; /* empty input */
    }

    Pipeline pl;
    if (parse_pipeline(&ps, &pl) < 0) {
        free(cl->pipelines);
        free(cl->connectors);
        return -1;
    }
    cl->pipelines[cl->count++] = pl;

    while (!at_end(&ps)) {
        Token *t = peek(&ps);
        if (!t) break;

        Connector conn;
        if (t->type == TOK_AND) {
            conn = CONN_AND;
        } else if (t->type == TOK_OR) {
            conn = CONN_OR;
        } else if (t->type == TOK_SEMI) {
            conn = CONN_SEMI;
        } else {
            break;
        }
        advance(&ps);

        /* skip extra semicolons */
        while (!at_end(&ps) && peek(&ps)->type == TOK_SEMI)
            advance(&ps);

        if (at_end(&ps)) {
            if (conn == CONN_SEMI)
                break; /* trailing ; is okay */
            fprintf(stderr, "lpsh: syntax error near operator\n");
            command_list_free(cl);
            return -1;
        }

        if (parse_pipeline(&ps, &pl) < 0) {
            command_list_free(cl);
            return -1;
        }
        cl->connectors[cl->count - 1] = conn;
        cl->pipelines[cl->count++] = pl;
    }

    return 0;
}

/* cleanup */

void command_list_free(CommandList *cl)
{
    for (int i = 0; i < cl->count; i++)
        pipeline_free(&cl->pipelines[i]);
    free(cl->pipelines);
    free(cl->connectors);
    memset(cl, 0, sizeof(*cl));
}
