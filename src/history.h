#ifndef HISTORY_H
#define HISTORY_H

#define MAX_COMMAND_LENGTH 1024
#define MAX_HISTORY 100

typedef struct {
    char commands[MAX_HISTORY][MAX_COMMAND_LENGTH];
    int start;
    int end;
    int count;
} History;

void add_to_history(History *history, const char *command);
void print_history(const History *history);

#endif