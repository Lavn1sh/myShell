/*
 * lpsh - History (readline-backed)
 */

#ifndef HISTORY_H
#define HISTORY_H

/* Load history from ~/.lpsh_history */
void history_load(void);

/* Save history to ~/.lpsh_history */
void history_save(void);

/* Perform history expansion on a line. Returns new malloc'd string. */
char *history_expand_line(const char *line);

#endif /* HISTORY_H */