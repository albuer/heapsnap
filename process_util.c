#include "process_util.h"


/**
 * 根据进程名称查找进程ID
 * */
int find_pid_of(const char *process_name) {
	int id;
	pid_t pid = -1;
	DIR* dir;
	FILE *fp;
	char filename[32];
	char cmdline[256];

	struct dirent * entry;

	if (process_name == NULL)
		return -1;

	dir = opendir("/proc");
	if (dir == NULL)
		return -1;

	while ((entry = readdir(dir)) != NULL) {
		id = atoi(entry->d_name);
		if (id != 0) {
			sprintf(filename, "/proc/%d/cmdline", id);
			fp = fopen(filename, "r");
			if (fp) {
				fgets(cmdline, sizeof(cmdline), fp);
				fclose(fp);

				if (strcmp(process_name, cmdline) == 0) {
					/* process found */
					pid = id;
					break;
				}
			}
		}
	}

	closedir(dir);
	return pid;
}

/**
 * 获取动态库装载地址
 * */
void* get_lib_adress(pid_t pid, const char* module_name) {
	FILE *fp;
	long addr = 0;
	char *pch;
	char filename[32];
	char line[1024];

	if (pid < 0) {
		/* self process */
		snprintf(filename, sizeof(filename), "/proc/self/maps");
	} else {
		snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
	}

	fp = fopen(filename, "r");

	if (fp != NULL) {
		while (fgets(line, sizeof(line), fp)) {
			//在所有的映射行中寻找目标动态库所在的行
			if (strstr(line, module_name)) {
				pch = strtok(line, "-");
				addr = strtoull(pch, NULL, 16);

				if (addr == 0x8000)
					addr = 0;

				break;
			}
		}

		fclose(fp);
	}

	return (void *) addr;
}

/**
 * 获取目标进程中函数地址
 * */
void* get_remote_func_address(pid_t target_pid, const char* module_name,void* local_addr) {
	void* local_handle, *remote_handle;
	local_handle = get_lib_adress(-1, module_name);
	if (local_handle==NULL)
		return NULL;
	remote_handle = get_lib_adress(target_pid, module_name);
	if (remote_handle==NULL)
		return NULL;

	  /*目标进程函数地址= 目标进程lib库地址 + （本进程函数地址 -本进程lib库地址）*/
	void * ret_addr = (void *) ((uintptr_t) remote_handle + (uintptr_t) local_addr - (uintptr_t) local_handle);
#if defined(__i386__)
	if (!strcmp(module_name, libc_path)) {
		ret_addr += 2;
	}
#endif
	return ret_addr;
}
