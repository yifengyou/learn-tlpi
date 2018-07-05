/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2018.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter Z */

/* multi_pidns.c

   Create a series of child processes in nested PID namespaces.

   See https://lwn.net/Articles/531419/
*/
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define STACK_SIZE (1024 * 1024)

/* Recursively create a series of child process in nested PID namespaces.
   'arg' is an integer that counts down to 0 during the recursion.
   When the counter reaches 0, recursion stops and the tail child
   executes the sleep(1) program. */

static int
childFunc(void *arg)
{
    static int first_call = 1;
    long level = (long) arg;
    char *child_stack;

    if (!first_call) {

        /* Unless this is the first recursive call to childFunc()
           (i.e., we were invoked from main()), mount a procfs
           for the current PID namespace */

        char mount_point[PATH_MAX];

        snprintf(mount_point, PATH_MAX, "/proc%c", (char) ('0' + level));

        mkdir(mount_point, 0555);       /* Create directory for mount point */
        if (mount("proc", mount_point, "proc", 0, NULL) == -1)
            errExit("mount");
        printf("Mounting procfs at %s\n", mount_point);
    }

    first_call = 0;

    if (level > 0) {

        /* Recursively invoke childFunc() to create another child in a
           nested PID namespace */

        level--;
        pid_t child_pid;

        child_stack = malloc(STACK_SIZE);
        if (child_stack == NULL)
            errExit("malloc");

        child_pid = clone(childFunc,
                    child_stack + STACK_SIZE,   /* Points to start of
                                                   downwardly growing stack */
                    CLONE_NEWPID | SIGCHLD, (void *) level);

        if (child_pid == -1)
            errExit("clone");

        if (waitpid(child_pid, NULL, 0) == -1)  /* Wait for child */
            errExit("waitpid");

        free(child_stack);
    } else {

        /* Tail end of recursion: execute sleep(1) */

        printf("Final child sleeping\n");
        execlp("sleep", "sleep", "1000", (char *) NULL);
        errExit("execlp");
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    long levels;

    levels = (argc > 1) ? atoi(argv[1]) : 5;
    childFunc((void *) levels);

    exit(EXIT_SUCCESS);
}
