#include "ptrace_util.h"


#define CPSR_T_MASK     ( 1u << 5 )

long ptrace_retval(struct pt_regs * regs) {
	return regs->ARM_r0;
}

long ptrace_ip(struct pt_regs * regs) {
	return regs->ARM_pc;
}

int ptrace_readdata(pid_t pid, const uint8_t *src, const uint8_t *buf, size_t size)
{
    uint32_t i, j, remain;
    uint8_t *laddr;

    union u {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / 4;
    remain = size % 4;
    laddr = (uint8_t*)buf;
	
    for (i = 0; i < j; i ++) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, 4);
        src += 4;
        laddr += 4;
    }

    if (remain > 0) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, remain);
    }

    return 0;
}

int ptrace_writedata(pid_t pid, const uint8_t *dest, const uint8_t *data, size_t size)
{
    uint32_t i, j, remain;
    const uint8_t *laddr;

    union u {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / 4;
    remain = size % 4;

    laddr = data;

    for (i = 0; i < j; i ++) {
        memcpy(d.chars, laddr, 4);
        ptrace(PTRACE_POKETEXT, pid, dest, d.val);
        dest  += 4;
        laddr += 4;
    }

    if (remain > 0) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, dest, 0);
        for (i = 0; i < remain; i ++) {
            d.chars[i] = *laddr ++;
        }

        ptrace(PTRACE_POKETEXT, pid, dest, d.val);
    }

    return 0;
}

int ptrace_call(pid_t pid, uint32_t addr, const long *params, uint32_t num_params, struct pt_regs* regs)
{
    uint32_t i;
    //前面四个参数用寄存器传递
    for (i = 0; i < num_params && i < 4; i ++) {
        regs->uregs[i] = params[i];
    }

    //后面参数放到栈里
    if (i < num_params) {
        regs->ARM_sp -= (num_params - i) * sizeof(long) ;
        ptrace_writedata(pid, (void *)regs->ARM_sp, (uint8_t *)&params[i], (num_params - i) * sizeof(long));
    }

    //PC指向要执行的函数地址
    regs->ARM_pc = addr;

    if (regs->ARM_pc & 1) {
        /* thumb */
        regs->ARM_pc &= (~1u);
        regs->ARM_cpsr |= CPSR_T_MASK;
    } else {
        /* arm */
        regs->ARM_cpsr &= ~CPSR_T_MASK;
    }

    //把返回地址设为0，这样目标进程执行完返回时会出现地址错误，这样目标进程将被挂起，控制权会回到调试进程手中
    regs->ARM_lr = 0;

    //设置目标进程的寄存器,让目标进程继续运行
    if (ptrace_setregs(pid, regs) == -1 || ptrace_continue(pid) == -1) {
        return -1;
    }
    //等待目标进程结束
    int stat = 0;
    waitpid(pid, &stat, WUNTRACED);
    while (stat != 0xb7f) {
        if (ptrace_continue(pid) == -1) {
            return -1;
        }
        waitpid(pid, &stat, WUNTRACED);
    }

    return 0;
}

int ptrace_getregs(pid_t pid, const struct pt_regs * regs) {
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0) {
    	perror("ptrace_getregs");
        return -1;
    }

    return 0;
}

int ptrace_setregs(pid_t pid, const struct pt_regs * regs) {
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0) {
        perror("ptrace_setregs");
        return -1;
    }

    return 0;
}

int ptrace_continue(pid_t pid) {
    if (ptrace(PTRACE_CONT, pid, NULL, 0) < 0) {
        perror("ptrace_cont");
        return -1;
    }

    return 0;
}

int ptrace_attach(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
    	perror("ptrace_attach");
        return -1;
    }

    int status = 0;
    waitpid(pid, &status , WUNTRACED);

    return 0;
}

int ptrace_detach(pid_t pid) {
    if (ptrace(PTRACE_DETACH, pid, NULL, 0) < 0) {
        perror("ptrace_detach");
        return -1;
    }

    return 0;
}

int ptrace_call_wrapper(pid_t target_pid, void * func_addr, long * parameters, int param_num, struct pt_regs * regs) {
    if (ptrace_call(target_pid, (uint32_t)func_addr, parameters, param_num, regs) < 0) {
        return -1;
	}
    if (ptrace_getregs(target_pid, regs) < 0) {
        return -1;
	}
    return 0;
}
