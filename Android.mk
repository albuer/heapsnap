LOCAL_PATH := $(call my-dir)

SNAP_C_FLAGS := -Wno-error

include $(CLEAR_VARS)
LOCAL_MODULE := heapsnap
LOCAL_SRC_FILES := inject.c process_util.c ptrace_util.c
LOCAL_SHARED_LIBRARIES := liblog libdl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := leak_test
LOCAL_SRC_FILES := leak_test.c
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := libheapsnap/heapsnap.cpp
LOCAL_C_INCLUDES :=
LOCAL_MODULE:= libheapsnap
LOCAL_SHARED_LIBRARIES := liblog libdl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 23)))
LOCAL_SHARED_LIBRARIES += libgccdemangle
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 24)))
LOCAL_C_INCLUDES += bionic/libc
LOCAL_C_INCLUDES += bionic/libc/private
LOCAL_C_INCLUDES += system/core/demangle/include
LOCAL_STATIC_LIBRARIES := libc_malloc_debug_backtrace
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \<= 25)))
LOCAL_STATIC_LIBRARIES += libc_logging
endif
endif
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# force build 32bit binary
ifeq ($(TARGET_ARCH),arm64)

include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE := heapsnap.32
LOCAL_SRC_FILES := inject.c process_util.c ptrace_util.c
LOCAL_SHARED_LIBRARIES := liblog libdl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE := leak_test.32
LOCAL_SRC_FILES := leak_test.c
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

endif
