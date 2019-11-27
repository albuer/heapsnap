#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/cdefs.h>
#include <android/log.h>
#include <malloc.h>
#include <bionic_malloc.h>

#define TAG                         "heapsnap"
#define VERION                      "v1.0"
#define DEFAULT_HEAPSNAP_SIG        SIGTTIN
#define HEPA_SNAP_PATH              "/sdcard/heap_snap"

#define HEAPSNAP_DEBUG

#define info_log(...)                                             \
    do {                                                          \
        __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__ ); \
    } while (0)
#define err_log(...)                                              \
    do {                                                          \
        __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__ ); \
    } while (0)

#ifdef HEAPSNAP_DEBUG
#define dbg_log(...)                                              \
    do {                                                          \
        __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__ ); \
    } while (0)
#else
#define dbg_log(...)
#endif

#if (PLATFORM_VERSION<10)
extern "C" void get_malloc_leak_info(uint8_t** info, size_t* overallSize,
        size_t* infoSize, size_t* totalMemory, size_t* backtraceSize);
extern "C" void free_malloc_leak_info(uint8_t* info);
#else
typedef bool (*ANDROID_MALLOPT)(int opcode, void* arg, size_t arg_size);
#endif
extern std::string backtrace_string(const uintptr_t* frames, size_t frame_count);

typedef struct {
    // Pointer to the buffer allocated by a call
    uint8_t* buffer;
    // The size of the "info" buffer.
    size_t overall_size;
    // The size of a single entry.
    size_t info_size;
    // The sum of all allocations that have been tracked. Does not include
    // any heap overhead.
    size_t total_memory;
    // The maximum number of backtrace entries.
    size_t backtrace_size;
} hs_malloc_leak_info_t;

static pid_t myPid = 0;

static int write_maps(const char* mapFile, FILE* dest)
{
    size_t READ_BLOCK_SIZE = 4096;
    FILE* src = fopen(mapFile, "r");
    if(src == NULL)
    {
        err_log("Open source file failed: %s!", mapFile);
        return -1;
    }

    if(dest == NULL)
    {
        err_log("Invalid dest file!");
        fclose(src);
        return -1;
    }
    char buffer[READ_BLOCK_SIZE];

    fseek(dest, 0, SEEK_END);
    sprintf(buffer, "\r\n******** MAPS ********\r\n\r\n");
    fwrite(buffer, 1, strlen(buffer), dest);
    buffer[0] = 0;
    int readNum = 0;
    while(!feof(src))
    {
        readNum = fread(buffer, 1, READ_BLOCK_SIZE, src);
        if(readNum > 0)
        {
            fwrite(buffer, 1, readNum, dest);
        }
        else
        {
            err_log("Read error, readNum=%d, errno=%d", readNum, errno);
            break;
        }
    }
    fclose(src);
    return 0;
}

/*
 * fp_tra: file to save heap has been translate info
 * fp_org: file to save heap org info
 */
static bool heapsnap_getfile2(FILE** fp_tra, FILE** fp_org)
{
    size_t FILENAME_SIZE = 1024;
    char fileName[FILENAME_SIZE];
    char filename_heap[FILENAME_SIZE];
    char filename_org_heap[FILENAME_SIZE];

    int fno = 0;
    if (access(HEPA_SNAP_PATH, 0) == -1) {
        info_log("create directory: %s\n", HEPA_SNAP_PATH);
        if (mkdir(HEPA_SNAP_PATH, 0766) < 0) {
            err_log("create directory failed: %s!\n", HEPA_SNAP_PATH);
            return false;
        }
    }
    while(1) {
        snprintf(fileName, FILENAME_SIZE-1, "%s/proc_%d_%04d", HEPA_SNAP_PATH, myPid, fno);
        snprintf(filename_heap, FILENAME_SIZE-1, "%s.heap", fileName);
        if (access(filename_heap, 0) < 0)
            break;
        ++fno;
    }

    *fp_tra = fopen(filename_heap, "w");
    if (*fp_tra == NULL)
        err_log("Open file failed: %s!\n", filename_heap);
    else 
        info_log("Save process's heap to file: %s\n", filename_heap);

    snprintf(filename_org_heap, FILENAME_SIZE-1, "%s.org.heap", fileName);
    *fp_org = fopen(filename_org_heap, "w");
    if(*fp_org == NULL)
        err_log("Open file failed: %s!\n", filename_org_heap);
    else
        info_log("Save process's org heap to file: %s\n", filename_org_heap);

    return true;
}

static bool get_malloc_info(hs_malloc_leak_info_t* leak_info)
{
#if (PLATFORM_VERSION<10)
    get_malloc_leak_info(&leak_info->buffer, &leak_info->overall_size, &leak_info->info_size,
            &leak_info->total_memory, &leak_info->backtrace_size);
#else
    if (!android_mallopt(M_GET_MALLOC_LEAK_INFO, leak_info, sizeof(*inleak_infofo))) {
      return false;
    }
#endif

    if (leak_info.buffer == nullptr || leak_info.overall_size == 0 || leak_info.info_size == 0
            || (leak_info.overall_size / leak_info.info_size) == 0) {
        info_log("no malloc info, libc.debug.malloc property should be set");
        return false;
    }

    return true;
}

static void free_malloc_info(hs_malloc_leak_info_t* info)
{
#if (PLATFORM_VERSION<10)
    free_malloc_leak_info(info->buffer);
#else
    android_mallopt(M_FREE_MALLOC_LEAK_INFO, info, sizeof(*info));
#endif
}

static void save_original_info(hs_malloc_leak_info_t *leak_info, FILE* fp)
{
    // dump original heap info
    if (android_mallopt(M_WRITE_MALLOC_LEAK_INFO_TO_FILE, fp, sizeof(FILE*))) {
        info_log("Native heap dump complete.\n");
    } else {
        err_log("Failed to write native heap dump to file");
    }
    fclose(fp);
}

static void save_demangle_info(hs_malloc_leak_info_t *leak_info, FILE* fp)
{
    size_t count = leak_info->overall_size / leak_info->info_size;

    fprintf(fp, "%zu bytes in %zu allocations\n", leak_info->total_memory, count);

    // The memory is sorted based on total size which is useful for finding
    // worst memory offenders. For diffs, sometimes it is preferable to sort
    // based on the backtrace.
    for (size_t i = 0; i < count; i++) {
        struct AllocEntry {
            size_t size;  // bit 31 is set if this is zygote allocated memory
            size_t allocations;
            uintptr_t backtrace[];
        };

        const AllocEntry * const e = (AllocEntry *)(leak_info->buffer + i * leak_info->info_size);

        fprintf(fp, "%zu bytes ( %zu bytes * %zu allocations )\n", e->size * e->allocations,
                e->size, e->allocations);
        std::string str = backtrace_string(e->backtrace, leak_info->backtrace_size);
        fprintf(fp, "%s\n", str.c_str());
    }
    fprintf(fp, "\n");

    char target_maps[128];
    snprintf(target_maps, 127, "/proc/%d/maps", myPid);
    info_log("Save maps(%s)\n", target_maps);
    write_maps(target_maps, fp);

    fclose(fp);
}

extern "C" void heapsnap_save(void)
{
    hs_malloc_leak_info_t leak_info;
    FILE *fp_tra = NULL;
    FILE *fp_org = NULL;
    heapsnap_getfile2(&fp_tra, &fp_org);

    if (fp_tra==NULL && fp_org==NULL)
        return;

    if (!get_malloc_info(&leak_info))
        return;

    save_original_info(&leak_info, fp_org);
    save_demangle_info(&leak_info, fp_tra);

    info_log("Done.\n");
}

static void heapsnap_signal_handler(int sig)
{
    dbg_log("PID(%d): catch SIG: %d\n", myPid, sig);
    switch (sig) {
    case DEFAULT_HEAPSNAP_SIG: {
        heapsnap_save();
        break;
    }
    default:
        break;
    }
}

extern "C" void heapsnap_init() {
    myPid = getpid();
    dbg_log("PID(%d): register snapshot SIG: %d\n", myPid, DEFAULT_HEAPSNAP_SIG);
    signal(DEFAULT_HEAPSNAP_SIG, &heapsnap_signal_handler);
    info_log("PID(%d): Heap Snap enabled\n", myPid);
}

extern "C" void heapsnap_deinit() {
    dbg_log("PID(%d): leaving heapsnap\n", myPid);
}

extern "C" void __attribute__((constructor)) prepare()
{
    dbg_log("prepare heapsnap\n");
    heapsnap_init();
}

