#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define HIST_SIZE 100

typedef struct {
    char *commands[HIST_SIZE];
    int start;
    int end;
    int count;
} History;

void init_history(History *history) {
    history->start = 0;
    history->end = 0;
    history->count = 0;
}

void add_to_history(History *history, const char *cmd) {
    if (history->count == HIST_SIZE) {
        free(history->commands[history->start]);
        history->start = (history->start + 1) % HIST_SIZE;
    }
    history->commands[history->end] = strdup(cmd);
    history->end = (history->end + 1) % HIST_SIZE;
    history->count++;
}

void print_history(History *history) {
    int i;
    for (i = 0; i < history->count; i++) {
        printf("%d: %s\n", i + 1, history->commands[(history->start + i) % HIST_SIZE]);
    }
}

// Function to parse the input into arguments
void parse_command(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \n");  // Split the input by space and newline
    while (args[i] != NULL) {
        i++;
        args[i] = strtok(NULL, " \n");
    }
}

int main() {
    char input[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGS];
    pid_t pid;
    int status;
    History history = { .start = 0, .end = 0, .count = 0 };

    while (1) {
        // Print the prompt
        printf("$ ");
        // Read the command from standard input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        // Remove the newline character from the input
        input[strcspn(input, "\n")] = 0;

        // Add the command to history
        add_to_history(&history, input);

        // Parse the command into arguments
        parse_command(input, args);

        // Check if the command is 'history'
        if (strcmp(args[0], "history") == 0) {
            print_history(&history);
            continue;
        }

        // If the input is empty, continue
        if (args[0] == NULL) {
            continue;
        }

        // Fork a child process
        pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Child process: Execute the command
            if (execvp(args[0], args) == -1) {
                perror("Execution failed");
            }
            exit(1);
        } else {
            // Parent process: Wait for the child process to complete
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}
