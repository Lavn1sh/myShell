/*
 * lpsh - Alias System
 */

#ifndef ALIAS_H
#define ALIAS_H

/* Set an alias. Returns 0 on success. */
int alias_set(const char *name, const char *value);

/* Remove an alias. Returns 0 on success, -1 if not found. */
int alias_unset(const char *name);

/* Look up an alias. Returns value or NULL. */
const char *alias_get(const char *name);

/* Print all aliases. */
void alias_list(void);

/* Expand aliases in a raw input line. Returns new malloc'd string. */
char *alias_expand(const char *input);

#endif /* ALIAS_H */
