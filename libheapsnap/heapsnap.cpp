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

#define TAG                         "heapsnap"
#define VERION                      "v0.2"
#define DEFAULT_HEAPSNAP_SIG        SIGTTIN
#define HEPA_SNAP_PATH              "/data/local/tmp/heap_snap"

#undef HEAPSNAP_DEBUG

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

#if defined(__LP64__)
#define PAD_PTR "016" PRIxPTR
#else
#define PAD_PTR "08" PRIxPTR
#endif
#define BACKTRACE_SIZE      32
#define SIZE_FLAG_ZYGOTE_CHILD  (1<<31)


extern "C" void get_malloc_leak_info(uint8_t** info, size_t* overallSize,
        size_t* infoSize, size_t* totalMemory, size_t* backtraceSize);
extern "C" void free_malloc_leak_info(uint8_t* info);
extern "C" char* __cxa_demangle(const char* mangled, char* buf, size_t* len,
                                int* status);

static pid_t myPid = 0;
static int save_f = 1;

/* mapinfo */

struct mapinfo_t {
  struct mapinfo_t* next;
  uintptr_t start;
  uintptr_t end;
  char name[];
};

static mapinfo_t* s_map_info = NULL;

// Format of /proc/<PID>/maps:
//   6f000000-6f01e000 rwxp 00000000 00:0c 16389419   /system/lib/libcomposer.so
static mapinfo_t* parse_maps_line(char* line) {
  uintptr_t start;
  uintptr_t end;
  int name_pos;
  if (sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %*4s %*x %*x:%*x %*d%n", &start,
             &end, &name_pos) < 2) {
    return NULL;
  }

  while (isspace(line[name_pos])) {
    name_pos += 1;
  }
  const char* name = line + name_pos;
  size_t name_len = strlen(name);
  if (name_len && name[name_len - 1] == '\n') {
    name_len -= 1;
  }

  mapinfo_t* mi = reinterpret_cast<mapinfo_t*>(calloc(1, sizeof(mapinfo_t) + name_len + 1));
  if (mi) {
    mi->start = start;
    mi->end = end;
    memcpy(mi->name, name, name_len);
    mi->name[name_len] = '\0';
  }
  return mi;
}

static mapinfo_t* mapinfo_create(pid_t pid) {
  struct mapinfo_t* milist = NULL;
  char data[1024]; // Used to read lines as well as to construct the filename.
  snprintf(data, sizeof(data), "/proc/%d/maps", pid);
  FILE* fp = fopen(data, "r");
  if (fp != NULL) {
    while (fgets(data, sizeof(data), fp) != NULL) {
      mapinfo_t* mi = parse_maps_line(data);
      if (mi) {
        mi->next = milist;
        milist = mi;
      }
    }
    fclose(fp);
  }
  return milist;
}

static void mapinfo_destroy(mapinfo_t* mi) {
  while (mi != NULL) {
    mapinfo_t* del = mi;
    mi = mi->next;
    free(del);
  }
}

// Find the containing map info for the PC.
static const mapinfo_t* mapinfo_find(mapinfo_t* mi, uintptr_t pc, uintptr_t* rel_pc) {
  for (; mi != NULL; mi = mi->next) {
    if ((pc >= mi->start) && (pc < mi->end)) {
      *rel_pc = pc - mi->start;
      return mi;
    }
  }
  *rel_pc = pc;
  return NULL;
}

static void dump_backtrace_symbols(FILE *fp, uintptr_t* frames, size_t frame_count) {
  for (size_t i = 0 ; i < frame_count; ++i) {
    uintptr_t offset = 0;
    const char* symbol = NULL;

    Dl_info info;
    if (dladdr((void*) frames[i], &info) != 0) {
      offset = reinterpret_cast<uintptr_t>(info.dli_saddr);
      symbol = info.dli_sname;
    }

    uintptr_t rel_pc = offset;
    const mapinfo_t* mi = (s_map_info != NULL) ? mapinfo_find(s_map_info, frames[i], &rel_pc) : NULL;
    const char* soname = (mi != NULL) ? mi->name : info.dli_fname;
    if (soname == NULL) {
      soname = "<unknown>";
    }
    if (symbol != NULL) {
      // TODO: we might need a flag to say whether it's safe to allocate (demangling allocates).
      char* demangled_symbol = __cxa_demangle(symbol, NULL, NULL, NULL);
      const char* best_name = (demangled_symbol != NULL) ? demangled_symbol : symbol;

      fprintf(fp, "          #%02zd  pc %" PAD_PTR "  %s (%s+%" PRIuPTR ")\n",
                        i, rel_pc, soname, best_name, frames[i] - offset);

      free(demangled_symbol);
    } else {
      fprintf(fp, "          #%02zd  pc %" PAD_PTR "  %s\n",
                        i, rel_pc, soname);
    }
  }
}

static int compareHeapRecords(const void* vrec1, const void* vrec2)
{
    const size_t* rec1 = (const size_t*) vrec1;
    const size_t* rec2 = (const size_t*) vrec2;
    size_t size1 = *rec1;
    size_t size2 = *rec2;

    if (size1 < size2) {
        return 1;
    } else if (size1 > size2) {
        return -1;
    }

    intptr_t* bt1 = (intptr_t*)(rec1 + 2);
    intptr_t* bt2 = (intptr_t*)(rec2 + 2);
    for (size_t idx = 0; idx < BACKTRACE_SIZE; idx++) {
        intptr_t addr1 = bt1[idx];
        intptr_t addr2 = bt2[idx];
        if (addr1 == addr2) {
            if (addr1 == 0)
                break;
            continue;
        }
        if (addr1 < addr2) {
            return -1;
        } else if (addr1 > addr2) {
            return 1;
        }
    }

    return 0;
}

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

static FILE* heapsnap_getfile()
{
    size_t FILENAME_SIZE = 1024;
    char fileName[FILENAME_SIZE];
    char filename_heap[FILENAME_SIZE];
    char filename_maps[FILENAME_SIZE];
    FILE* fp_heap = stdout;

    if (save_f) {
    	int fno = 0;
	    if (access(HEPA_SNAP_PATH, 0) == -1) {
	        info_log("create directory: %s\n", HEPA_SNAP_PATH);
	        if (mkdir(HEPA_SNAP_PATH, 0775) < 0) {
	            err_log("create directory failed: %s!\n", HEPA_SNAP_PATH);
	            return NULL;
	        }
	    }
        while(1) {
    	    snprintf(fileName, FILENAME_SIZE-1, "%s/proc_%d_%04d", HEPA_SNAP_PATH, myPid, fno);
    	    snprintf(filename_heap, FILENAME_SIZE-1, "%s.heap", fileName);
            if (access(filename_heap, 0) < 0)
                break;
            ++fno;
        }
        fp_heap = fopen(filename_heap, "w+");
        if(fp_heap == NULL)
        {
            err_log("Open file failed: %s!\n", filename_heap);
            return NULL;
        }
        info_log("Save process heap to file: %s\n", filename_heap);
    }
    return fp_heap;
}

extern "C" void heapsnap_save(void)
{
    size_t FILENAME_SIZE = 1024;
    uint8_t* info = NULL;
    size_t overallSize, infoSize, totalMemory, backtraceSize;
    char buf[1024] = "";

    get_malloc_leak_info(&info, &overallSize, &infoSize, &totalMemory,
        &backtraceSize);
    if (info == NULL || infoSize==0 || (overallSize % infoSize)) {
        err_log("PID(%d): heap is empty\n", myPid);
        return;
    }

    FILE *fp = heapsnap_getfile();
    if (fp == NULL) {
        err_log("PID(%d): open failed: %d!\n", myPid, errno);
        return;
    }

    fprintf(fp, "Heap Snapshot %s\n\n", VERION);

    size_t recordCount = overallSize / infoSize;
    fprintf(fp, "Total memory: %zu\n", totalMemory);
    fprintf(fp, "Allocation records: %zd\n", recordCount);
    if (backtraceSize != BACKTRACE_SIZE) {
        err_log("PID(%d): mismatched backtrace sizes (%d vs. %d)\n",
            myPid, backtraceSize, BACKTRACE_SIZE);
    }
    fprintf(fp, "\n");

    /* re-sort the entries */
    qsort(info, recordCount, infoSize, compareHeapRecords);

    s_map_info = mapinfo_create(myPid);

    /* dump the entries to the file */
    const uint8_t* ptr = info;
    for (size_t idx = 0; idx < recordCount; idx++) {
        size_t size = *(size_t*) ptr;
        size_t allocations = *(size_t*) (ptr + sizeof(size_t));
        intptr_t* backtrace = (intptr_t*) (ptr + sizeof(size_t) * 2);
        size_t bt = 0;

        fprintf(fp, "size %8i, dup %4i", size & ~SIZE_FLAG_ZYGOTE_CHILD, allocations);
        for (bt = 0; bt < backtraceSize; bt++) {
            if (backtrace[bt] == 0) {
                break;
            } else {
                fprintf(fp, ", 0x%08x", backtrace[bt]);
            }
        }
        fprintf(fp, "\n");

        dump_backtrace_symbols(fp, (uintptr_t*)backtrace, bt);

        ptr += infoSize;
    }

    mapinfo_destroy(s_map_info);
    free_malloc_leak_info(info);
    char target_maps[128];
    snprintf(target_maps, 127, "/proc/%d/maps", myPid);
    info_log("Save maps(%s)\n", target_maps);
    write_maps(target_maps, fp);
    fclose(fp);

    info_log("Done.\n");

    return;
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

