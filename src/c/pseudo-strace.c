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
#include "pseudo-strace.h"

#include "trace.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
/******************************************************************************
*                                   MACROS                                    *
******************************************************************************/
#define SYCALL_ARG(type, n, regs) (type)syscall_arg(n, regs)
#define SYCALL_RETVAL(type, regs) (type)syscall_retval(regs)

#define SYSCALL_BUF(str, strlen, n, x, regs) \
	sprint_buffer( \
		SYCALL_ARG(char*, n, regs), \
		str, \
		SYCALL_ARG(ssize_t, x, regs), \
		strlen \
	)

#define CHAR_ARR_STRLEN(s) (sizeof(s) - 1)
/******************************************************************************
*                                  CONSTANTS                                  *
******************************************************************************/
static const ssize_t PRINT_BUFFER_SIZE = 256;
/******************************************************************************
*                            FUNCTION DECLARATIONS                            *
******************************************************************************/
static void* init(void *arg);
static void* handle(void *argg, const struct tracee_state *state);
static void print_syscall(
	FILE *fp, pid_t pid, const struct user_regs_struct *regs
);
static uint64_t syscall_retval(const struct user_regs_struct *regs);
static uint64_t syscall_arg(int n, const struct user_regs_struct *regs);
static int repr_byte(char *str, char byte, ssize_t *space_size);
static char octal_char(int val, int n);
static char *sprint_buffer(
	const char *buffer,
	char *space,
	ssize_t buffer_size,
	ssize_t space_size
);
/******************************************************************************
*                              STATIC FUNCTIONS                               *
******************************************************************************/
static char octal_char(int val, int n)
{
	return ((val >> (3 * n)) & 0x7) + '0';
}
/*****************************************************************************/
static int repr_byte(char *str, char byte, ssize_t *space_size)
{
	if((byte == '"') || (byte == '\\')) {
		if(*space_size < 2) {
			return 0;
		} else {
			str[0] = '\\';
			str[1] = byte;
			*space_size -= 2;
			return 2;
		}
	} else if(byte == '\n') {
		if(*space_size < 2) {
			return 0;
		} else {
			str[0] = '\\';
			str[1] = 'n';
			*space_size -= 2;
			return 2;
		}
	} else if(isprint(byte) || (byte == '\t')) {
		if(*space_size == 0) {
			return 0;
		} else {
			str[0] = byte;
			*space_size -= 1;
			return 1;
		}
	} else {
		if(*space_size < 4) {
			return 0;
		} else {
			str[0] = '\\';
			str[1] = octal_char(byte, 2);
			str[2] = octal_char(byte, 1);
			str[3] = octal_char(byte, 0);
			*space_size -= 4;
			return 4;
		}
	}
}
/*****************************************************************************/
static char *sprint_buffer(
	const char *buffer,
	char *str,
	ssize_t buffer_size,
	ssize_t space_size
) {
	int len = 0;
	char border = '"';
	const char continuation[] = "\"...";

	space_size -= sizeof(border) + CHAR_ARR_STRLEN(continuation) + 1;

	if(space_size < 0) {
		return NULL;
	}

	str[len] = border;
	len += 1;

	for(size_t i = 0; i < buffer_size; i++) {
		int s = 0;
		char c = buffer[i];

		if((s = repr_byte(str + len, c, &space_size)) == 0) {
			memcpy(str + len, continuation, sizeof(continuation));
			return str;
		}
		len += s;
	}

	str[len] = border;
	str[len + 1] = '\0';

	return str;
}
/*****************************************************************************/
static uint64_t syscall_retval(const struct user_regs_struct *regs)
{
	return regs->rax;
}
/*****************************************************************************/
static uint64_t syscall_arg(int n, const struct user_regs_struct *regs)
{
	assert(n < 6);

	switch(n) {
	case 0:
		return regs->rdi;
	case 1:
		return regs->rsi;
	case 2:
		return regs->rdx;
	case 3:
		return regs->r10;
	case 4:
		return regs->r8;
	case 5:
		return regs->r9;
	default:
		return 0;
	}
}
/*****************************************************************************/
static void print_syscall(
	FILE *fp, pid_t pid, const struct user_regs_struct *regs
) {
	char p_buffer[PRINT_BUFFER_SIZE];
	int syscall_no = regs->orig_rax;

	switch(syscall_no) {
	case SYS_write:
		fprintf(
			fp, "[ID %d]: write(%d, %s, %ld) = %d\n",
			pid,
			SYCALL_ARG(int,     0, regs),
			SYSCALL_BUF(p_buffer, PRINT_BUFFER_SIZE, 1, 2, regs),
			SYCALL_ARG(int64_t, 2, regs),
			SYCALL_RETVAL(int, regs)
		);
		break;
	case SYS_close:
		fprintf(
			fp, "[ID %d]: close(%d) = %d\n",
			pid,
			SYCALL_ARG(int,     0, regs),
			SYCALL_RETVAL(int, regs)
		);
		break;
	case SYS_fstat:
		fprintf(
			fp, "[ID %d]: fstat(%d, %p) = %d\n",
			pid,
			SYCALL_ARG(int,     0, regs),
			SYCALL_ARG(void*,   1, regs),
			SYCALL_RETVAL(int, regs)
		);
		break;
	case SYS_mmap:
		fprintf(
			fp, "[ID %d]: mmap(%p, %ld, %d, %d, %d, %lu) = %p\n",
			pid,
			SYCALL_ARG(void*,    0, regs),
			SYCALL_ARG(int64_t,  1, regs),
			SYCALL_ARG(int,      2, regs),
			SYCALL_ARG(int,      3, regs),
			SYCALL_ARG(int,      4, regs),
			SYCALL_ARG(uint64_t, 5, regs),
			SYCALL_RETVAL(void*,    regs)
		);
		break;
	case SYS_ioctl:
		fprintf(
			fp, "[ID %d]: ioctl(%d, %lu, %p) = %d\n",
			pid,
			SYCALL_ARG(int,       0, regs),
			SYCALL_ARG(uint64_t,  1, regs),
			SYCALL_ARG(void*,     2, regs),
			SYCALL_RETVAL(int,    regs)
		);
		break;
	case SYS_getpid:
		fprintf(
			fp, "[ID %d]: getpid() = %d\n",
			pid,
			SYCALL_RETVAL(int,    regs)
		);
		break;
	case SYS_getdents:
		fprintf(
			fp, "[ID %d]: getdents(%d, %p, %d) = %d\n",
			pid,
			SYCALL_ARG(int,       0, regs),
			SYCALL_ARG(void*,     1, regs),
			SYCALL_ARG(int,       2, regs),
			SYCALL_RETVAL(int,    regs)
		);
		break;
	case SYS_openat:
		fprintf(
			fp, "[ID %d]: openat(%d, %p, %d, %d) = %d\n",
			pid,
			SYCALL_ARG(int,       0, regs),
			SYCALL_ARG(void*,     1, regs),
			SYCALL_ARG(int,       2, regs),
			SYCALL_ARG(int,       3, regs),
			SYCALL_RETVAL(int,    regs)
		);
		break;
	default:
		fprintf(
			fp, "[ID %d]: syscall(%d, ...) = %lu\n",
			pid, syscall_no, SYCALL_RETVAL(uint64_t, regs)
		);
	}
}
/*****************************************************************************/
static void* init(void *arg)
{
	return fopen("/dev/stderr", "w");
}
/*****************************************************************************/
static void* handle(void *arg, const struct tracee_state *state)
{
	if(state->status == SYSCALL_ENTER_STOP) {

	} else if(state->status == SYSCALL_EXIT_STOP) {
		print_syscall(arg, state->pid, &state->data.regs);
	} else if(state->status == EXITED_NORMAL) {
		fprintf(
			arg,
			"[ID %d]: Exited: %d\n",
			state->pid,
			state->data.exit_status
		);
	}

	return arg;
}
/******************************************************************************
*                            FUNCTION DEFINITIONS                             *
******************************************************************************/
struct trace_descriptor pseudo_strace_descriptor(void)
{
	struct trace_descriptor descr;

	descr.handle = handle;
	descr.init = init;
	descr.arg = NULL;

	return descr;
}
/*****************************************************************************/