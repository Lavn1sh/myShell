#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "pipeline.h"
#include "utils.h"

void execute_pipeline(char *commands[], int num_commands, int background) {
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

    if (!background) {
        for (int i = 0; i < num_commands; i++) {
            wait(&status);
        }
    } else {
        printf("Pipeline running in background with PID %d\n", pid);
    }
}