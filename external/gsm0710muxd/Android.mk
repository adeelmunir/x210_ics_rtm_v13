LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= gsm0710muxd
LOCAL_SRC_FILES:= gsm0710muxd_bp.c
LOCAL_SHARED_LIBRARIES := libcutils 
LOCAL_CFLAGS := -DMUX_ANDROID
LOCAL_MODULE_TAGS := optional eng
LOCAL_LDLIBS := -lpthread
include $(BUILD_EXECUTABLE)
