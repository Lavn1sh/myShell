/*
 * lpsh - Job Control
 *
 * Manages background/stopped jobs, SIGCHLD handling,
 * and fg/bg resumption with proper process groups.
 */

#include "jobs.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* add a job */

int job_add(pid_t pgid, pid_t *pids, int num_pids, const char *cmdline, int bg)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id == 0) {
            shell.jobs[i].id       = shell.next_job_id++;
            shell.jobs[i].pgid     = pgid;
            shell.jobs[i].pids     = malloc(sizeof(pid_t) * num_pids);
            memcpy(shell.jobs[i].pids, pids, sizeof(pid_t) * num_pids);
            shell.jobs[i].num_pids = num_pids;
            shell.jobs[i].state    = bg ? JOB_RUNNING : JOB_RUNNING;
            strncpy(shell.jobs[i].cmdline, cmdline, MAX_LINE - 1);
            shell.jobs[i].cmdline[MAX_LINE - 1] = '\0';

            if (shell.jobs[i].id >= shell.next_job_id)
                shell.next_job_id = shell.jobs[i].id + 1;

            if (bg)
                printf("[%d] %d\n", shell.jobs[i].id, pgid);

            return shell.jobs[i].id;
        }
    }
    fprintf(stderr, "lpsh: maximum number of jobs reached\n");
    return -1;
}

/* remove a job */

void job_remove(int id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id == id) {
            free(shell.jobs[i].pids);
            memset(&shell.jobs[i], 0, sizeof(Job));
            return;
        }
    }
}

/* find */

Job *job_find(int id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id == id)
            return &shell.jobs[i];
    }
    return NULL;
}

Job *job_find_by_pgid(pid_t pgid)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id != 0 && shell.jobs[i].pgid == pgid)
            return &shell.jobs[i];
    }
    return NULL;
}

/* list all jobs */

void jobs_list(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id != 0) {
            const char *state_str;
            switch (shell.jobs[i].state) {
                case JOB_RUNNING: state_str = "Running";  break;
                case JOB_STOPPED: state_str = "Stopped";  break;
                case JOB_DONE:    state_str = "Done";     break;
                default:          state_str = "Unknown";  break;
            }
            printf("[%d] %s\t\t%s\n", shell.jobs[i].id, state_str,
                   shell.jobs[i].cmdline);
        }
    }
}

/* check for finished/stopped jobs (non-blocking) */

void jobs_check_status(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (shell.jobs[i].id == 0 || shell.jobs[i].state == JOB_DONE)
            continue;

        int status;
        pid_t result = waitpid(-shell.jobs[i].pgid, &status, WNOHANG | WUNTRACED);

        if (result > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf("[%d] Done\t\t%s\n", shell.jobs[i].id,
                       shell.jobs[i].cmdline);
                shell.jobs[i].state = JOB_DONE;
                job_remove(shell.jobs[i].id);
            } else if (WIFSTOPPED(status)) {
                shell.jobs[i].state = JOB_STOPPED;
            }
        }
    }
}

/* wait for foreground job */

int job_wait(Job *job)
{
    int status = 0;
    int last_status = 0;

    for (int i = 0; i < job->num_pids; i++) {
        pid_t result;
        do {
            result = waitpid(job->pids[i], &status, WUNTRACED);
        } while (result == -1 && errno == EINTR);

        if (result > 0) {
            if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
                printf("\n[%d] Stopped\t\t%s\n", job->id, job->cmdline);
                return 128 + WSTOPSIG(status);
            }
            if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_status = 128 + WTERMSIG(status);
        }
    }

    job_remove(job->id);
    return last_status;
}

/* bring job to foreground */

int job_foreground(Job *job)
{
    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, job->pgid);

    if (job->state == JOB_STOPPED)
        kill(-job->pgid, SIGCONT);

    job->state = JOB_RUNNING;
    int status = job_wait(job);

    if (shell.interactive)
        tcsetpgrp(STDIN_FILENO, getpgrp());

    return status;
}

/* resume job in background */

int job_background(Job *job)
{
    if (job->state == JOB_STOPPED) {
        kill(-job->pgid, SIGCONT);
        job->state = JOB_RUNNING;
        printf("[%d] %s &\n", job->id, job->cmdline);
    }
    return 0;
}
