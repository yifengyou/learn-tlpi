/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2018.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* sv_libabc.c

*/
#include <stdio.h>

void
abc(void)
{
    void xyz(void);

    printf("abc() calling xyz()\n");
    xyz();
}

__asm__(".symver xyz,xyz@VER_1");
