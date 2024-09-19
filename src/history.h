#ifndef HISTORY_H
#define HISTORY_H

#include <time.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_HISTORY 100

typedef struct {
    char command[MAX_COMMAND_LENGTH];
    pid_t pid;
    time_t start_time;
    double duration;
} CommandDetails;

typedef struct {
    CommandDetails commands[MAX_HISTORY];
    int start;
    int end;
    int count;
} History;

void add_to_history(History *history, const char *command, pid_t pid, time_t start_time, double duration);
void print_history(const History *history);
void display_exec_details(const History *history);

#endif