# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
ifeq ($(TARGET_BOARD_PLATFORM), s5p6442)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog 
LOCAL_SRC_FILES := hdmi_api.c

LOCAL_CFLAGS  += \
        -DSCREEN_WIDTH=$(SCREEN_WIDTH) \
        -DSCREEN_HEIGHT=$(SCREEN_HEIGHT) \
		-DDEFAULT_FB_NUM=$(DEFAULT_FB_NUM)

ifeq ($(BOARD_HDMI_STD),STD_480P)
LOCAL_CFLAGS  += -DSTD_480P
endif

ifeq ($(BOARD_HDMI_STD),STD_720P)
LOCAL_CFLAGS  += -DSTD_720P
endif

ifeq ($(BOARD_HDMI_STD),STD_1080P)
LOCAL_CFLAGS  += -DSTD_1080P
endif

LOCAL_C_INCLUDES := \
 	$(LOCAL_PATH)/../include	

#Tori add TAGS
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libhdmi
include $(BUILD_SHARED_LIBRARY)

endif
