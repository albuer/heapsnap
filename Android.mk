LOCAL_PATH := $(call my-dir)

SNAP_C_FLAGS := -Wno-error

include $(CLEAR_VARS)
LOCAL_MODULE := heapsnap
LOCAL_SRC_FILES := inject.c process_util.c ptrace_util.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := leak_test
LOCAL_SRC_FILES := leak_test.c
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -ldl
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := libheapsnap/heapsnap.cpp
LOCAL_C_INCLUDES :=
LOCAL_MODULE:= libheapsnap
LOCAL_SHARED_LIBRARIES :=
LOCAL_LDLIBS := -ldl -llog
LOCAL_CFLAGS := $(SNAP_C_FLAGS)
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \< 6.0)))
LOCAL_LDLIBS += -lgccdemangle
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
