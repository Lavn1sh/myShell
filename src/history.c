#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include "history.h"

void add_to_history(History *history, const char *command, pid_t pid, time_t start_time, double duration) {
    strncpy(history->commands[history->end].command, command, MAX_COMMAND_LENGTH - 1);
    history->commands[history->end].command[MAX_COMMAND_LENGTH - 1] = '\0'; // Ensure null-termination
    history->commands[history->end].pid = pid;
    history->commands[history->end].start_time = start_time;
    history->commands[history->end].duration = duration;
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
        printf("%d: %s\n", i + 1, history->commands[index].command);
        index = (index + 1) % MAX_HISTORY;
    }
}

void display_exec_details(const History *history) {
    printf("\nExecution Details:\n");
    int index = history->start;
    for (int i = 0; i < history->count; i++) {
        char *start_time_string = ctime(&history->commands[index].start_time);
        start_time_string[strlen(start_time_string) - 1] = '\0'; // Remove newline character
        printf("Command: %s\n", history->commands[index].command);
        printf("PID: %d\n", history->commands[index].pid);
        printf("Start Time: %s\n", start_time_string);
        printf("Duration: %.2f seconds\n\n", history->commands[index].duration);
        index = (index + 1) % MAX_HISTORY;
    }
}