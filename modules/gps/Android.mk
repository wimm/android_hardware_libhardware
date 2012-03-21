# HAL module implemenation, not prelinked and stored in
# hw/<GPS_HARDWARE_MODULE_ID>.<ro.hardware>.so

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# LOCAL_MODULE := gps.wimm330
LOCAL_MODULE := gps.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional 

LOCAL_SRC_FILES := \
    buffer.c \
    crc.c \
    interface.c \
    power.c \
    storage.c \
    thread.c \
    gps_comm.c \
    gps_datetime.c \
    gps_location.c \
    gps_state.c \
    message.c \
    message_queue.c \
    nmea_msg.c \
    nmea_parser.c \
    nmea_tokenizer.c \
    osp_msg.c \
    osp_parser.c \
    read_thread.c \
    write_thread.c \
    work_thread.c \
    work_thread_eclm.c \
    work_thread_geo.c \
    work_thread_sirf.c \
    patch_file.c \
    patch_manager.c \
    sgee_thread.c \

LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -Werror
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware

include $(BUILD_SHARED_LIBRARY)
