/*
 * lpsh - Job Control
 */

#ifndef JOBS_H
#define JOBS_H

#include "shell.h"

/* Add a new job. Returns the job number. */
int   job_add(pid_t pgid, pid_t *pids, int num_pids, const char *cmdline, int bg);

/* Remove a job by id. */
void  job_remove(int id);

/* Find a job by id. Returns NULL if not found. */
Job  *job_find(int id);

/* Find a job by pgid. */
Job  *job_find_by_pgid(pid_t pgid);

/* Print all jobs. */
void  jobs_list(void);

/* Update job state (called from SIGCHLD handler). */
void  jobs_check_status(void);

/* Wait for a foreground job. */
int   job_wait(Job *job);

/* Continue a stopped job in foreground. */
int   job_foreground(Job *job);

/* Continue a stopped job in background. */
int   job_background(Job *job);

#endif /* JOBS_H */
