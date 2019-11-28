LOCAL_PATH := $(call my-dir)

SNAP_C_FLAGS := -Wno-error

include $(CLEAR_VARS)
LOCAL_MODULE := heapsnap
LOCAL_SRC_FILES := inject.c process_util.c ptrace_util.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_VERSION=$(PLATFORM_VERSION)
include $(BUILD_EXECUTABLE)

# for 32bit
include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE := heapsnap.32
LOCAL_SRC_FILES := inject.c process_util.c ptrace_util.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_VERSION=$(PLATFORM_VERSION)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := leak_test
LOCAL_SRC_FILES := leak_test.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

# for 32bit
include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE := leak_test.32
LOCAL_SRC_FILES := leak_test.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := libheapsnap/heapsnap.cpp
#LOCAL_SRC_FILES += \
	libheapsnap/android10/MemoryLeakTrackUtil.cpp \
	libheapsnap/android10/backtrace.cpp \
	libheapsnap/android10/MapData.cpp
LOCAL_C_INCLUDES :=
LOCAL_MODULE:= libheapsnap
#LOCAL_SHARED_LIBRARIES := libc_malloc_debug
LOCAL_STATIC_LIBRARIES := libc_malloc_debug_backtrace
# libdemangle
LOCAL_LDLIBS := -ldl -llog
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_VERSION=$(PLATFORM_VERSION)
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 6)))
LOCAL_LDLIBS += -lgccdemangle
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 9)))
LOCAL_C_INCLUDES += bionic/libc
LOCAL_C_INCLUDES += bionic/libc/private
LOCAL_C_INCLUDES += system/core/demangle/include
#LOCAL_LDLIBS += -lc_malloc_debug
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
