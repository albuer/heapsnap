#ifndef __PTRACE_UTIL_H__
#define __PTRACE_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
//#include <asm/user.h>
#include <asm/ptrace.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <elf.h>
#include <sys/uio.h>
#include "log_util.h"

#if defined(__i386__)
    #define pt_regs         user_regs_struct
#elif defined(__aarch64__)
    #define pt_regs         user_pt_regs
    #define uregs   regs
    #define ARM_pc  pc
    #define ARM_sp  sp
    #define ARM_cpsr    pstate
    #define ARM_lr      regs[30]
    #define ARM_r0      regs[0]
    #define PTRACE_GETREGS PTRACE_GETREGSET
    #define PTRACE_SETREGS PTRACE_SETREGSET
#endif

int ptrace_readdata(pid_t pid, const uint8_t *src, const uint8_t *buf, size_t size);
int ptrace_writedata(pid_t pid, const uint8_t *dest, const uint8_t *data, size_t size);
int ptrace_call(pid_t pid, uintptr_t addr, long *params, int num_params, struct pt_regs* regs);
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
