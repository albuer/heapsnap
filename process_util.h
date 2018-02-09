#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "log_util.h"

/**
 * 根据进程名称查找进程ID
 * */
int find_pid_of(const char *process_name) ;

/**
 * 获取动态库装载地址
 * */
void* get_lib_adress(pid_t pid, const char* module_name);

/**
 * 获取目标进程中函数地址
 * */
void* get_remote_func_address(pid_t target_pid, const char* module_name,void* local_addr);
