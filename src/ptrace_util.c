#include "ptrace_util.h"


#define CPSR_T_MASK     ( 1u << 5 )

extern const char *libc_path;

long ptrace_retval(struct pt_regs * regs)
{
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_r0;
#elif defined(__i386__)
    return regs->eax;
#else
#error "Not supported"
#endif
}

long ptrace_ip(struct pt_regs * regs)
{
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_pc;
#elif defined(__i386__)
    return regs->eip;
#else
#error "Not supported"
#endif
}

int ptrace_readdata(pid_t pid, const uint8_t *src, const uint8_t *buf, size_t size)
{
    long i, j, remain;
    uint8_t *laddr;
    const size_t bytes_width = sizeof(long);

    union u {
        long val;
        char chars[bytes_width];
    } d;

    j = size / bytes_width;
    remain = size % bytes_width;
    laddr = (uint8_t*)buf;
	
    for (i = 0; i < j; i ++) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, bytes_width);
        src += bytes_width;
        laddr += bytes_width;
    }

    if (remain > 0) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, remain);
    }

    return 0;
}

int ptrace_writedata(pid_t pid, const uint8_t *dest, const uint8_t *data, size_t size)
{
    long i, j, remain;
    const uint8_t *laddr;
    const size_t bytes_width = sizeof(long);

    union u {
        long val;
        char chars[bytes_width];
    } d;

    j = size / bytes_width;
    remain = size % bytes_width;

    laddr = data;

    for (i = 0; i < j; i ++) {
        memcpy(d.chars, laddr, bytes_width);
        ptrace(PTRACE_POKETEXT, pid, dest, d.val);
        dest  += bytes_width;
        laddr += bytes_width;
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

#if defined(__arm__) || defined(__aarch64__)
int ptrace_call(pid_t pid, uintptr_t addr, long *params, int num_params, struct pt_regs* regs)
{
    int i;
#if defined(__arm__)
    int num_param_registers = 4;
#elif defined(__aarch64__)
    int num_param_registers = 8;
#endif

    //前面四个参数用寄存器传递
    for (i = 0; i < num_params && i < num_param_registers; i ++) {
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
#elif defined(__i386__)
long ptrace_call(pid_t pid, uintptr_t addr, long *params, int num_params, struct user_regs_struct * regs)
{
    regs->esp -= (num_params) * sizeof(long);
    ptrace_writedata(pid, (void *)regs->esp, (uint8_t *)params, (num_params) * sizeof(long));

    long tmp_addr = 0x00;
    regs->esp -= sizeof(long);
    ptrace_writedata(pid, regs->esp, (char *)&tmp_addr, sizeof(tmp_addr));

    regs->eip = addr;

    if (ptrace_setregs(pid, regs) == -1
            || ptrace_continue( pid) == -1) {
        printf("error\n");
        return -1;
    }

    int stat = 0;
    waitpid(pid, &stat, WUNTRACED);
    while (stat != 0xb7f) {
        if (ptrace_continue(pid) == -1) {
            printf("error\n");
            return -1;
        }
        waitpid(pid, &stat, WUNTRACED);
    }

    return 0;
}
#else
#error "Not supported"
#endif

int ptrace_getregs(pid_t pid, const struct pt_regs * regs) {
#if defined (__aarch64__)
    long regset = NT_PRSTATUS;
    struct iovec ioVec;

    ioVec.iov_base = (void*)regs;
    ioVec.iov_len = sizeof(*regs);
    if (ptrace(PTRACE_GETREGSET, pid, (void*)regset, &ioVec) < 0) {
        perror("ptrace_getregs: Can not get register values");
        printf(" io %p, %lu", ioVec.iov_base, ioVec.iov_len);
        return -1;
    }
#else
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0) {
    	perror("ptrace_getregs");
        return -1;
    }
#endif

    return 0;
}

int ptrace_setregs(pid_t pid, const struct pt_regs * regs) {
#if defined (__aarch64__)
    long regset = NT_PRSTATUS;
    struct iovec ioVec;

    ioVec.iov_base = (void*)regs;
    ioVec.iov_len = sizeof(*regs);
    if (ptrace(PTRACE_SETREGSET, pid, (void*)regset, &ioVec) < 0) {
        perror("ptrace_setregs: Can not get register values");
        return -1;
    }
#else
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0) {
        perror("ptrace_setregs");
        return -1;
    }
#endif

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
    if (ptrace_call(target_pid, (uintptr_t)func_addr, parameters, param_num, regs) < 0) {
        return -1;
	}
    if (ptrace_getregs(target_pid, regs) < 0) {
        return -1;
	}
    return 0;
}
