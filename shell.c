#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define MAX_HISTORY 10

typedef struct {
    char commands[MAX_HISTORY][MAX_COMMAND_LENGTH];
    int start;
    int end;
    int count;
} History;

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

void parse_command(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL) {
        i++;
        args[i] = strtok(NULL, " ");
    }
}

void execute_command(char *command) {
    char *args[MAX_ARGS];
    parse_command(command, args);
    if (execvp(args[0], args) == -1) {
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
}

void execute_pipeline(char *commands[], int num_commands) {
    int pipefds[2 * (num_commands - 1)];
    pid_t pid;
    int status;

    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_commands; i++) {
        pid = fork();
        if (pid == 0) {
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], 0) == -1) {
                    perror("dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }
            if (i < num_commands - 1) {
                if (dup2(pipefds[i * 2 + 1], 1) == -1) {
                    perror("dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipefds[j]);
            }
            execute_command(commands[i]);
        } else if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefds[i]);
    }

    for (int i = 0; i < num_commands; i++) {
        wait(&status);
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

        // Check if the command is 'history'
        if (strcmp(input, "history") == 0) {
            print_history(&history);
            continue;
        }

        // Check for background process indicator '&'
        int background = 0;
        if (input[strlen(input) - 1] == '&') {
            background = 1;
            input[strlen(input) - 1] = '\0'; // Remove '&' from command
        }

        // Split the command by pipes
        char *commands[MAX_ARGS];
        int num_commands = 0;
        commands[num_commands] = strtok(input, "|");
        while (commands[num_commands] != NULL) {
            num_commands++;
            commands[num_commands] = strtok(NULL, "|");
        }

        if (num_commands > 1) {
            execute_pipeline(commands, num_commands);
        } else {
            // Parse the command into arguments
            parse_command(input, args);

            // If the input is empty, continue
            if (args[0] == NULL) {
                continue;
            }

            // Fork and execute the command
            pid = fork();
            if (pid == 0) {
                // Child process
                if (execvp(args[0], args) == -1) {
                    perror("execvp failed");
                }
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                // Forking error
                perror("fork failed");
            } else {
                // Parent process
                if (!background) {
                    waitpid(pid, &status, 0);
                } else {
                    printf("Process running in background with PID %d\n", pid);
                }
            }
        }
    }

    return 0;
}