
LOCAL_PATH:= $(call my-dir)

#
# amp_samples
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-c-files-under, ./)

PRIVATE_AMP_TOP : vendor/marvell/prebuilts/$(TARGET_PRODUCT)/amp
LOCAL_C_INCLUDES := \
    $(PRIVATE_AMP_TOP)/include \
    $(VENDOR_SDK_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
    libsm
        


LOCAL_CFLAGS := -D__BIONIC__ -DANDROID -D__LINUX__
LOCAL_MODULE := depict_adc
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)

include $(BUILD_EXECUTABLE)
