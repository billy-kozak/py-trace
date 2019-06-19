/******************************************************************************
* Copyright (C) 2019  Billy Kozak                                             *
*                                                                             *
* This file is part of the py-trace program                                   *
*                                                                             *
* This program is free software: you can redistribute it and/or modify        *
* it under the terms of the GNU Lesser General Public License as published by *
* the Free Software Foundation, either version 3 of the License, or           *
* (at your option) any later version.                                         *
*                                                                             *
* This program is distributed in the hope that it will be useful,             *
* but WITHOUT ANY WARRANTY; without even the implied warranty of              *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
* GNU Lesser General Public License for more details.                         *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.       *
******************************************************************************/
#define _GNU_SOURCE
/******************************************************************************
*                                  INCLUDES                                   *
******************************************************************************/
#include "shared.h"

#include "trace.h"
#include "syscall-utl.h"

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>
#include <sys/types.h>
/******************************************************************************
*                                    DATA                                     *
******************************************************************************/
static pid_t old_main_pid;
static pid_t new_main_pid;
/******************************************************************************
*                              STATIC FUNCTIONS                               *
******************************************************************************/
static bool am_py_trace(const char *progname);
static sigjmp_buf jump_buffer;
/*****************************************************************************/
static void do_special_setup(void)
{
	if(start_trace()) {
		perror("Unable to start trace");
	}
}
/*****************************************************************************/
static bool am_py_trace(const char *progname)
{
	return strcmp(basename(progname), "py-trace") == 0;
}
/*****************************************************************************/
static int fake_main(int argc, char **argv, char **envp)
{
	if(!am_py_trace(argv[0])) {
		old_main_pid = syscall_getpid();

		do_special_setup();

		new_main_pid = syscall_getpid();
	}

	siglongjmp(jump_buffer, 1);
	return 0;
}
/******************************************************************************
*                            FUNCTION DECLARATIONS                            *
******************************************************************************/
EXPORT int __libc_start_main(
	int (*main)(int, char **, char **),
	int argc,
	char **ubp_av,
	void (*init)(void),
	void (*fini)(void),
	void (*rtld_fini) (void),
	void (* stack_end)
) {
	int (*real_libc_start_main)
		(
			int (*main)(int, char **, char **),
			int argc,
			char **ubp_av, void (*init)(void),
			void (*fini)(void),
			void (*rtld_fini) (void),
			void (* stack_end)
		);

	if(sigsetjmp(jump_buffer, 0) == 0) {
		real_libc_start_main = dlsym(RTLD_NEXT, "__libc_start_main");
		return real_libc_start_main(
			fake_main,
			argc,
			ubp_av,
			init,
			fini,
			rtld_fini,
			stack_end
		);
	} else {
		real_libc_start_main = dlsym(RTLD_NEXT, "__libc_start_main");
		return real_libc_start_main(
			main,
			argc,
			ubp_av,
			init,
			fini,
			rtld_fini,
			stack_end
		);
	}


}
/*****************************************************************************/
EXPORT pid_t getpid(void)
{
	pid_t real_pid = syscall_getpid();

	if(real_pid == new_main_pid) {
		return old_main_pid;
	} else {
		return real_pid;
	}
}
/*****************************************************************************/