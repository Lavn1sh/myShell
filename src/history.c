#include <stdio.h>
#include <string.h>
#include "history.h"

void add_to_history(History *history, const char *command) {
    strncpy(history->commands[history->end], command, MAX_COMMAND_LENGTH);
    history->end = (history->end + 1) % MAX_HISTORY;
    if (history->count < MAX_HISTORY) {
        history->count++;
    } else {
        history->start = (history->start + 1) % MAX_HISTORY;
    }
}

void print_history(const History *history) {
    int index = history->start;
    for (int i = 0; i < history->count; i++) {
        printf("%d: %s\n", i + 1, history->commands[index]);
        index = (index + 1) % MAX_HISTORY;
    }
}