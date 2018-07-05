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

/* seccomp_perf.c

   How much does a simple seccomp() filter cost vis-a-vis a trivial system
   call such as getppid()?

   To test with the in-kernel JIT compiler enabled:

        $ sudo sh -c "echo 1 > /proc/sys/net/core/bpf_jit_enable"
*/
#define _GNU_SOURCE
#include <stddef.h>
#include <fcntl.h>
#include <linux/audit.h>
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int
seccomp(unsigned int operation, unsigned int flags, void *arg)
{
    return syscall(__NR_seccomp, operation, flags, arg);
    // Or: return prctl(PR_SET_SECCOMP, operation, arg);
}

static void
install_filter(void)
{
    struct sock_filter filter[] = {
        /* Load architecture */

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                (offsetof(struct seccomp_data, arch))),

        /* Kill process if the architecture is not what we expect */

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),

        /* Load system call number */

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (offsetof(struct seccomp_data, nr))),

        /* Allow system calls other than open() */

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        /* Kill process on open() */

        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL)
    };

    struct sock_fprog prog = {
        .len = (unsigned short) (sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog) == -1)
        errExit("seccomp");
}

int
main(int argc, char *argv[])
{
    int j, nloops;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num-loops> [x]\n", argv[0]);
        fprintf(stderr, "       (use 'x' to run with BPF filter applied)\n");
        exit(EXIT_FAILURE);
    }

    if (argc > 2) {
        printf("Appling BPF filter\n");

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
            errExit("prctl");

        install_filter();
    }

    nloops = atoi(argv[1]);

    for (j = 0; j < nloops; j++)
        getppid();

    exit(EXIT_SUCCESS);
}
