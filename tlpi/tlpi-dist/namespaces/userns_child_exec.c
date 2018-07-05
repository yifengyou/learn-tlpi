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

/* userns_child_exec.c

   Create a child process that executes a shell command in new
   namespace(s); allow UID and GID mappings to be specified when
   creating a user namespace.

   See https://lwn.net/Articles/532593/
*/
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <limits.h>
#include <linux/securebits.h>
#include <sys/capability.h>
#include "cap_functions.h"
#include "userns_functions.h"

#ifndef CLONE_NEWCGROUP         /* Added in Linux 4.6 */
#define CLONE_NEWCGROUP         0x02000000
#endif

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

struct child_args {
    char **argv;        /* Command to be executed by child, with args */
    int    pipe_fd[2];  /* Pipe used to synchronize parent and child */
};

static int verbose;
static int secbits;             /* If nonzero, then set process securebits
                                   flags to this value before exec() */
static int newuid = -1;         /* If set to value other than -1, then set
                                   UIDs in child process before exec() */
static int useAmbient = 0;      /* If set, then push all possible capabilities
                                   into ambient set before exec() */

static void
usage(char *pname)
{
    fprintf(stderr, "Usage: %s [options] cmd [arg...]\n\n", pname);
    fprintf(stderr, "Create a child process that executes a shell "
            "command in a new user namespace,\n"
            "and possibly also other new namespace(s).\n\n");
    fprintf(stderr, "Options can be:\n\n");
#define fpe(str) fprintf(stderr, "    %s", str);
    fpe("-C          New cgroup namespace\n");
    fpe("-i          New IPC namespace\n");
    fpe("-m          New mount namespace\n");
    fpe("-n          New network namespace\n");
    fpe("-p          New PID namespace\n");
    fpe("-u          New UTS namespace\n");
    fpe("-U          New user namespace\n");
    fpe("-M uid_map  Specify UID map for user namespace\n");
    fpe("-G gid_map  Specify GID map for user namespace\n");
    fpe("-D          Do not write \"deny\" to /proc/PID/setgroups before\n");
    fpe("            updating GID map\n");
    fpe("-r          Create 'root' mappings: map user's UID and GID to "
        "0 in user\n");
    fpe("            namespace (equivalent to: -M '0 <uid> 1' "
        "-G '0 <gid> 1')\n");
    fpe("-z          Synonym for '-r'\n");
    fpe("-a          Use ambient capability set to preserve all possible\n");
    fpe("            capabilities across execve()\n");
    fpe("-s <uid>    Set process UIDs to <uid> before exec()\n");
    fpe("            (useful for some experiments with user namespaces)\n");
    fpe("-b <bits>   Set securebits flags before modifying process IDs:\n");
    fpe("                'r' - SECBIT_NOROOT\n");
    fpe("                's' - SECBIT_NO_SETUID_FIXUP\n");
    fpe("-v          Display verbose messages\n");
    fpe("\n");
    fpe("If -r, -M, or -G is specified, -U is required.\n");
    fpe("It is not permitted to specify both -r and either -M or -G.\n");
    fpe("\n");
    fpe("Map strings for -M and -G consist of records of the form:\n");
    fpe("\n");
    fpe("    ID-inside-ns   ID-outside-ns   len\n");
    fpe("\n");
    fpe("A map string can contain multiple records, separated"
        " by commas;\n");
    fpe("the commas are replaced by newlines before writing"
        " to map files.\n");

    exit(EXIT_FAILURE);
}

/* Raise as many ambient capabilities as possible.

   Return value: the number of ambient capabilities that are raised. */

static int
raiseAmbientCaps(void)
{
    int cap;
    int cnt;

    cnt = 0;

    /* Walk through the range of possible capabilities trying to add each
       capability to the ambient set. In order to raise an ambient capability,
       it must first be raised in the process inheritable set. */

    for (cap = 0; cap <= CAP_LAST_CAP; cap++) {
        modifyCapSetting(CAP_INHERITABLE, cap, CAP_SET);
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) == 0)
            cnt++;
    }

    return cnt;
}

static int              /* Start function for cloned child */
childFunc(void *arg)
{
    struct child_args *args = (struct child_args *) arg;
    char ch;

    /* Wait until the parent has updated the UID and GID mappings.
       See the comment in main(). We wait for end of file on a
       pipe that will be closed by the parent process once it has
       updated the mappings. */

    close(args->pipe_fd[1]);    /* Close our descriptor for the write
                                   end of the pipe so that we see EOF
                                   when parent closes its descriptor */
    if (read(args->pipe_fd[0], &ch, 1) != 0) {
        fprintf(stderr,
                "Failure in child: read from pipe returned != 0\n");
        exit(EXIT_FAILURE);
    }

    close(args->pipe_fd[0]);    /* We no longer need the pipe */

    /* If the securebits command-line option was specified, then set
       the specified securebits. */

    if (secbits != 0) {
        if (prctl(PR_SET_SECUREBITS, secbits) == -1)
            errExit("prctl");
    }

    /* If the "set UID" command-line option was specified, then change
       all UID credentials to the specified UID */

    if (newuid > 0) {
        if (setresuid(newuid, newuid, newuid) == -1)
            errExit("setresuid");
        if (verbose)
            display_creds_and_caps("\t[child after setresuid()] ");
    }

    /* If the "raise ambient capabilities" command-line option was specified,
       then raise as many ambient capabilities as possible */

    if (useAmbient)
        raiseAmbientCaps();

    /* Execute a shell command */

    if (verbose)
        printf("About to exec: %s\n", args->argv[0]);

    execvp(args->argv[0], args->argv);
    errExit("execvp");
}

#define STACK_SIZE (1024 * 1024)

int
main(int argc, char *argv[])
{
    int flags, opt, create_root_mappings, deny_setgroups;
    pid_t child_pid;
    struct child_args args;
    char *uid_map, *gid_map;
    const int MAP_BUF_SIZE = 100;
    char map_buf[MAP_BUF_SIZE];
    char map_path[PATH_MAX];
    char *child_stack;
    char *p;

    /* Parse command-line options. The initial '+' character in
       the final getopt(3) argument prevents GNU-style permutation
       of command-line options. Preventing that is useful, since
       sometimes the 'command' to be executed by this program itself
       has command-line options. We don't want getopt() to treat
       those as options to this program. */

    flags = 0;
    verbose = 0;
    gid_map = NULL;
    uid_map = NULL;
    create_root_mappings = 0;
    deny_setgroups = 1;
    while ((opt = getopt(argc, argv, "+CimnuUM:G:rzDvpas:b:")) != -1) {
        switch (opt) {
        case 'C': flags |= CLONE_NEWCGROUP;     break;
        case 'i': flags |= CLONE_NEWIPC;        break;
        case 'm': flags |= CLONE_NEWNS;         break;
        case 'n': flags |= CLONE_NEWNET;        break;
        case 'p': flags |= CLONE_NEWPID;        break;
        case 'u': flags |= CLONE_NEWUTS;        break;
        case 'U': flags |= CLONE_NEWUSER;       break;
        case 'M': uid_map = optarg;             break;
        case 'G': gid_map = optarg;             break;
        case 'r':
        case 'z': create_root_mappings = 1;     break;
        case 'D': deny_setgroups = 0;           break;
        case 'v': verbose = 1;                  break;
        case 'a': useAmbient = 1;               break;
        case 's': newuid = atoi(optarg);        break;
        case 'b':
            p = optarg;
            secbits = 0;
            for (p = optarg; *p != '\0'; p++) {
                switch (*p) {
                case 'r': secbits |= SECBIT_NOROOT;             break;
                case 's': secbits |= SECBIT_NO_SETUID_FIXUP;    break;
                default: usage(argv[0]);
                }
            }
            break;
        default:  usage(argv[0]);
        }
    }

    /* -M, -G, or -r without -U is nonsensical */

    if (((uid_map != NULL || gid_map != NULL || create_root_mappings) &&
                !(flags & CLONE_NEWUSER)) ||
            (create_root_mappings && (uid_map != NULL || gid_map != NULL)))
        usage(argv[0]);

    if (optind >= argc)
        usage(argv[0]);

    args.argv = &argv[optind];

    /* We use a pipe to synchronize the parent and child, in order to
       ensure that the parent sets the UID and GID maps before the child
       calls execve(). This ensures that the child maintains its
       capabilities during the execve() in the common case where we
       want to map the child's effective user ID to 0 in the new user
       namespace. Without this synchronization, the child would lose
       its capabilities if it performed an execve() with nonzero
       user IDs (see the capabilities(7) man page for details of the
       transformation of a process's capabilities during execve()). */

    if (pipe(args.pipe_fd) == -1)
        errExit("pipe");

    /* Create the child in new namespace(s) */

    child_stack = malloc(STACK_SIZE);
    if (child_stack == NULL)
        errExit("malloc");

    child_pid = clone(childFunc, child_stack + STACK_SIZE,
                      flags | SIGCHLD, &args);
    if (child_pid == -1)
        errExit("clone");

    /* Parent falls through to here */

    if (verbose)
        printf("%s: PID of child created by clone() is %ld\n",
                argv[0], (long) child_pid);

    /* Update the UID and GID maps in the child */

    if (uid_map != NULL || create_root_mappings) {
        snprintf(map_path, PATH_MAX, "/proc/%ld/uid_map",
                (long) child_pid);
        if (create_root_mappings) {
            snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long) getuid());
            uid_map = map_buf;
        }
        if (update_map(uid_map, map_path) == -1)
            errExit("update_map: uid_map");
    }

    if (gid_map != NULL || create_root_mappings) {
        if (deny_setgroups) {
            if (proc_setgroups_write(child_pid, "deny") == -1)
                errExit("proc_setgroups_write");
        }

        snprintf(map_path, PATH_MAX, "/proc/%ld/gid_map",
                (long) child_pid);
        if (create_root_mappings) {
            snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long) getgid());
            gid_map = map_buf;
        }
        if (update_map(gid_map, map_path) == -1)
            errExit("update_map: gid_map");
    }

    /* Close the write end of the pipe, to signal to the child that we
       have updated the UID and GID maps */

    close(args.pipe_fd[1]);

    if (waitpid(child_pid, NULL, 0) == -1)      /* Wait for child */
        errExit("waitpid");

    if (verbose)
        printf("%s: terminating\n", argv[0]);

    exit(EXIT_SUCCESS);
}
