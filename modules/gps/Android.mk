ifeq ($(TARGET_BOARD_PLATFORM), s5p6442)

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<GPS_HARDWARE_MODULE_ID>.<ro.hardware>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware

ifeq ($(USE_ATHR_GPS_HARDWARE),true)
    LOCAL_SRC_FILES := wimm_gps_hardware.c
endif

LOCAL_SRC_FILES := wimm_gps_hardware.c
LOCAL_SRC_FILES += wimm_gps.c
LOCAL_MODULE := gps.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional 
include $(BUILD_SHARED_LIBRARY)
endif
