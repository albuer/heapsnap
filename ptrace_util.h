#ifndef __PTRACE_UTIL_H__
#define __PTRACE_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
//#include <asm/user.h>
#include <asm/ptrace.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "log_util.h"

int ptrace_readdata(pid_t pid, const uint8_t *src, const uint8_t *buf, size_t size);
int ptrace_writedata(pid_t pid, const uint8_t *dest, const uint8_t *data, size_t size);
int ptrace_call(pid_t pid, uint32_t addr, const long *params, uint32_t num_params, struct pt_regs* regs);
int ptrace_getregs(pid_t pid, const struct pt_regs * regs);
int ptrace_setregs(pid_t pid, const struct pt_regs * regs);
int ptrace_continue(pid_t pid);

int ptrace_attach(pid_t pid);

int ptrace_detach(pid_t pid);

/**
 * 取执行返回值
 * */
long ptrace_retval(struct pt_regs * regs);

/**
 * 取PC寄存器值
 * */
long ptrace_ip(struct pt_regs * regs);

int ptrace_call_wrapper(pid_t target_pid, void * func_addr, long * parameters, int param_num, struct pt_regs * regs);

#endif
