#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include "process_util.h"
#include "ptrace_util.h"

#define MMAP_SIZE	0x400

#if defined(__aarch64__)
	const char *libc_path = "libc.so";
	#if (PLATFORM_VERSION<10)
	const char *linker_path = "linker64";
	#else
	const char *linker_path = "libdl.so";
	#endif
#else
	const char *libc_path = "libc.so";
	#if (PLATFORM_VERSION<10)
	const char *linker_path = "linker";
	#else
	const char *linker_path = "libdl.so";
	#endif
#endif
static int s_oneshot = 0;

static int call_func(pid_t pid, const char* lib, void* func, long* params, int param_count, struct pt_regs* regs, long* ret)
{
	void *func_addr = get_remote_func_address(pid, lib, func);

	if (func_addr==NULL) {
		return -1;
	}

	if (ptrace_call_wrapper(pid, func_addr, params, param_count, regs) < 0) {
		return -1;
	}

	*ret = ptrace_retval(regs);
	return 0;
}

int inject_remote_process(pid_t target_pid, const char *library_path,
		const char *function_name, const char *param) {
	int ret = -1;
	struct pt_regs regs, original_regs;
	long parameters[6];
	long func_ret = 0;
	uint8_t *target_mmap_base = NULL;
	void * target_so_handle = NULL;
	void * hook_func_addr = NULL;

	LOGI("libc_path:%s\nlinker_path:%s\n", libc_path, linker_path);

	LOGI("Attach process<%d>%s.\n", target_pid, s_oneshot?" at oneshot mode":"");

	if(ptrace_attach(target_pid) < 0) {
		return -1;
	}

	if (ptrace_getregs(target_pid, &original_regs) < 0) {
		goto out_detach;
	}
	memcpy(&regs, &original_regs, sizeof(original_regs));

	// mmap a memory in target process
	parameters[0] = 0;  // addr
	parameters[1] = MMAP_SIZE; // size
	parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC;  // prot
	parameters[3] = MAP_ANONYMOUS | MAP_PRIVATE; // flags
	parameters[4] = 0; //fd
	parameters[5] = 0; //offset

	if (call_func(target_pid, libc_path, (void*)mmap, parameters, 6, &regs, (long*)&target_mmap_base) != 0)
		goto out_setregs;
	LOGD("Process<%d>: mmap a buffer %d@%p\n", target_pid, MMAP_SIZE, target_mmap_base);

	ptrace_writedata(target_pid, (const uint8_t *)target_mmap_base, (const uint8_t *)library_path, strlen(library_path) + 1);
	parameters[0] = (long)target_mmap_base;
	parameters[1] = RTLD_NOW | RTLD_GLOBAL;

	if (call_func(target_pid, linker_path, (void*)dlopen, parameters, 2, &regs, (long*)&target_so_handle) != 0)
		goto out_munmap;
	LOGD("Process<%d>: dlopen return handle %p for %s\n", target_pid, target_so_handle, library_path);

	if (function_name != NULL) {
		ptrace_writedata(target_pid, (const uint8_t *)target_mmap_base, (const uint8_t *)function_name, strlen(function_name) + 1);
		parameters[0] = (long)target_so_handle;
		parameters[1] = (long)target_mmap_base;

		if (call_func(target_pid, linker_path, (void*)dlsym, parameters, 2, &regs, (long*)&hook_func_addr) != 0)
			goto out_dlclose;
		LOGD("Process<%d>: func [%s] address %p, running...\n", target_pid, function_name, hook_func_addr);

		if (param != NULL) {
			ptrace_writedata(target_pid, (const uint8_t *)target_mmap_base, (const uint8_t *)param, strlen(param) + 1);
			parameters[0] = (long)target_mmap_base;

			if (ptrace_call_wrapper(target_pid, hook_func_addr, parameters, 1, &regs) < 0) {
				LOGE("Process<%d>: call func[%s] failed!", target_pid, function_name);
				goto out_dlclose;
			}
		} else {
			if (ptrace_call_wrapper(target_pid, hook_func_addr, NULL, 0, &regs) < 0) {
				LOGE("Process<%d>: call func[%s] failed!", target_pid, function_name);
				goto out_dlclose;
			}
		}
		LOGD("Process<%d>: func [%s] done.\n", target_pid, function_name);
	}

	ret = 0;

out_dlclose:
	if (ret!=0 || s_oneshot) {
		parameters[0] = (long)target_so_handle;
		LOGD("Process<%d>: dlclose %s.\n", target_pid, library_path);
		if (call_func(target_pid, linker_path, (void*)dlclose, parameters, 2, &regs, &func_ret) != 0) {
			LOGW("Process<%d>: Warning! dlclose %s failed.", target_pid, library_path);
		}
	}
out_munmap:
	if (ret!=0 || s_oneshot) {
		parameters[0] = (long)target_mmap_base;  // addr
		parameters[1] = MMAP_SIZE; // size
		LOGD("Process<%d>: munmap %d@%p.\n", target_pid, MMAP_SIZE, target_mmap_base);
		if (call_func(target_pid, libc_path, (void*)munmap, parameters, 2, &regs, &func_ret) != 0) {
			LOGW("Process<%d>: Warning! munmap %p failed.", target_pid, target_mmap_base);
		}
	}
out_setregs:
	ptrace_setregs(target_pid, &original_regs);
	LOGD("Process<%d>: reset regs\n", target_pid);
out_detach:
	ptrace_detach(target_pid);

	LOGI("Dettach process<%d>.\n", target_pid);

	return ret;
}

static void showhelp(const char* cmd)
{
	LOGE("Usage:\n  %s -p <pid> -l <library> [-f function [-m parameter]] [-o]\n", cmd);
}

int main(int argc, char** argv)
{
	LOGE("Heap Snap Version 0.2\n");
	pid_t target_pid = (pid_t)-1;
	char* library_path = NULL;
	char* function_name = NULL;
	char* parameter = NULL;
	int ch;

	while ((ch = getopt(argc, argv, "p:l:f:m:o")) != -1) {
		switch(ch) {
		case 'p':
			target_pid = atoi(optarg);
			break;
		case 'l':
			library_path = optarg;
			break;
		case 'f':
			function_name = optarg;
			break;
		case 'm':
			parameter = optarg;
			break;
		case 'o':
			s_oneshot = 1;
			break;
		default:
			showhelp(argv[0]);
			exit(-1);
			break;
		}
	}
	if (argc <= 1 || target_pid == -1 || library_path == NULL) {
	    showhelp(argv[0]);
	    exit(-1);
	}

	inject_remote_process(target_pid, library_path, function_name, parameter);

	return 0;
}
