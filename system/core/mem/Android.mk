LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= md.c
LOCAL_MODULE := md
LOCAL_MODULE_TAGS := debug eng tests
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= mw.c
LOCAL_MODULE := mw
LOCAL_MODULE_TAGS := debug eng tests
include $(BUILD_EXECUTABLE)
