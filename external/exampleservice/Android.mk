LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= exampleservice.c
LOCAL_MODULE := exampleservice
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
