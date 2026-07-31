
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_OWNER := qti
LOCAL_C_INCLUDES += $(TOP)/$(QC_PROP_ROOT)/mm-hab/uhab

LOCAL_SRC_FILES := \
    hsi2s_interface.c \
    shm_habmm.c \

LOCAL_MODULE := libhsi2sinterface

LOCAL_SHARED_LIBRARIES := liblog libbase 
LOCAL_SHARED_LIBRARIES += libuhab

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter -Wno-unused-variable
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
