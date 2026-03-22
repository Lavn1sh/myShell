/*
 * lpsh - Prompt
 */

#ifndef PROMPT_H
#define PROMPT_H

/*
 * Build and return the prompt string.
 * Format: user@host:~/dir (branch) ✓/✗ $
 * The returned string is in a static buffer.
 */
const char *prompt_build(void);

#endif /* PROMPT_H */
