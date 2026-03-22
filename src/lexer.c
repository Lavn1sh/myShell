/*
 * lpsh - Lexer / Tokenizer
 *
 * Handles:
 *   - Single-quoted strings (literal, no escape processing)
 *   - Double-quoted strings (preserves spaces, allows \" and \\ escapes)
 *   - Backslash escapes outside quotes
 *   - Operators: | && || ; & > >> < 2> 2>> <<
 */

#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helpers */

static void token_list_init(TokenList *tl)
{
    tl->capacity = 32;
    tl->count    = 0;
    tl->tokens   = malloc(sizeof(Token) * tl->capacity);
}

static void token_list_push(TokenList *tl, TokenType type, const char *value)
{
    if (tl->count >= tl->capacity) {
        tl->capacity *= 2;
        tl->tokens = realloc(tl->tokens, sizeof(Token) * tl->capacity);
    }
    tl->tokens[tl->count].type  = type;
    tl->tokens[tl->count].value = value ? lpsh_strdup(value) : NULL;
    tl->count++;
}

void token_list_free(TokenList *tl)
{
    for (int i = 0; i < tl->count; i++)
        free(tl->tokens[i].value);
    free(tl->tokens);
    tl->tokens = NULL;
    tl->count  = 0;
}

/* main tokenizer */

TokenList lexer_tokenize(const char *input)
{
    TokenList tl;
    token_list_init(&tl);

    const char *p = input;
    char buf[MAX_LINE];
    int  bi;

    while (*p) {
        /* skip whitespace */
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;

        /* comments */
        if (*p == '#') {
            break; /* rest of line is a comment */
        }

        /* operators (multi-char first) */
        if (p[0] == '>' && p[1] == '>') {
            token_list_push(&tl, TOK_REDIR_APPEND, ">>");
            p += 2;
            continue;
        }
        if (p[0] == '2' && p[1] == '>' && p[2] == '>') {
            token_list_push(&tl, TOK_REDIR_ERR_APPEND, "2>>");
            p += 3;
            continue;
        }
        if (p[0] == '2' && p[1] == '>') {
            token_list_push(&tl, TOK_REDIR_ERR, "2>");
            p += 2;
            continue;
        }
        if (p[0] == '<' && p[1] == '<') {
            token_list_push(&tl, TOK_HEREDOC, "<<");
            p += 2;
            continue;
        }
        if (p[0] == '&' && p[1] == '&') {
            token_list_push(&tl, TOK_AND, "&&");
            p += 2;
            continue;
        }
        if (p[0] == '|' && p[1] == '|') {
            token_list_push(&tl, TOK_OR, "||");
            p += 2;
            continue;
        }

        /* single-char operators */
        if (*p == '|') {
            token_list_push(&tl, TOK_PIPE, "|");
            p++;
            continue;
        }
        if (*p == ';') {
            token_list_push(&tl, TOK_SEMI, ";");
            p++;
            continue;
        }
        if (*p == '&') {
            token_list_push(&tl, TOK_BG, "&");
            p++;
            continue;
        }
        if (*p == '>') {
            token_list_push(&tl, TOK_REDIR_OUT, ">");
            p++;
            continue;
        }
        if (*p == '<') {
            token_list_push(&tl, TOK_REDIR_IN, "<");
            p++;
            continue;
        }

        /* word (possibly quoted) */
        bi = 0;
        while (*p && !isspace((unsigned char)*p)) {
            /* single quote: everything literal until closing ' */
            if (*p == '\'') {
                p++;
                while (*p && *p != '\'') {
                    if (bi < MAX_LINE - 1) buf[bi++] = *p;
                    p++;
                }
                if (*p == '\'') p++;
                continue;
            }

            /* double quote: allows escapes, keeps spaces */
            if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (p[1] == '"' || p[1] == '\\' ||
                                       p[1] == '$' || p[1] == '`')) {
                        p++;
                        if (bi < MAX_LINE - 1) buf[bi++] = *p;
                        p++;
                    } else {
                        if (bi < MAX_LINE - 1) buf[bi++] = *p;
                        p++;
                    }
                }
                if (*p == '"') p++;
                continue;
            }

            /* backslash escape outside quotes */
            if (*p == '\\' && p[1]) {
                p++;
                if (bi < MAX_LINE - 1) buf[bi++] = *p;
                p++;
                continue;
            }

            /* break on operator characters (they're not part of a word) */
            if (*p == '|' || *p == '&' || *p == ';' ||
                *p == '>' || *p == '<' || *p == '#') {
                /* special: 2> and 2>> at word boundary */
                if (*p == '>' || (*p == '2' && (p[1] == '>')))
                    break;
                break;
            }

            if (bi < MAX_LINE - 1) buf[bi++] = *p;
            p++;
        }

        if (bi > 0) {
            buf[bi] = '\0';
            token_list_push(&tl, TOK_WORD, buf);
        }
    }

    token_list_push(&tl, TOK_NEWLINE, NULL);
    return tl;
}
