#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "history.h"
#include "pipeline.h"
#include "utils.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

int main() {
    char input[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGS];
    pid_t pid;
    int status;
    History history = { .start = 0, .end = 0, .count = 0 };

    while (1) {
        // Print the prompt
        printf("$ ");
        fflush(stdout); // Ensure the prompt is printed immediately

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
            execute_pipeline(commands, num_commands, background);
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