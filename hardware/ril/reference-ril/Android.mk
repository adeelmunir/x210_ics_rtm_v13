LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    at_tok.c \
    atchannel.c \
    reference-ril.c \
    misc.c 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libril \
    libmedia

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE
#build shared library
LOCAL_CFLAGS += -DRIL_SHLIB

LOCAL_CFLAGS += -DPLATFORM_VERSION=$(subst .,,$(PLATFORM_VERSION))

LOCAL_C_INCLUDES := \
    hardware/ril/libril \

LOCAL_MODULE := libreference-ril
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
