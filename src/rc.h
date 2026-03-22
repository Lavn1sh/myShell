/*
 * lpsh - RC File
 */

#ifndef RC_H
#define RC_H

/* Load and execute ~/.lpshrc (or a specified file). Returns 0 on success. */
int rc_load(const char *path);

/* Load the default rc file (~/.lpshrc). */
int rc_load_default(void);

#endif /* RC_H */
