
LOCAL_PATH:= $(call my-dir)

# demo
include $(CLEAR_VARS)
LOCAL_MODULE:= hsi2s_demo
LOCAL_MODULE_OWNER := qti
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libs
LOCAL_SRC_FILES := demo.c
LOCAL_SHARED_LIBRARIES := libhsi2sinterface
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter -Wno-unused-variable
LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)


# test
include $(CLEAR_VARS)
LOCAL_MODULE:= hsi2s_test2
LOCAL_MODULE_OWNER := qti
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libs
LOCAL_SRC_FILES := driver_test.c
LOCAL_SHARED_LIBRARIES := libhsi2sinterface
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter -Wno-unused-variable
LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)

