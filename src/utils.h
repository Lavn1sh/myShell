#ifndef UTILS_H
#define UTILS_H

#define MAX_ARGS 64

void parse_command(char *input, char **args);
void execute_command(char *command);

#endif