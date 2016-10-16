# Copyright (C) 2007 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    mrvl_recovery.cpp \
    mrvl_bootloader.cpp \
    mrvl_install.cpp \
    mrvl_roots.cpp \
    mrvl_ui.cpp \
    mrvl_screen_ui.cpp \
    mrvl_asn1_decoder.cpp \
    mrvl_verifier.cpp \
    mrvl_adb_install.cpp \
    mrvl_fuse_sdcard_provider.c

LOCAL_MODULE := recovery_mrvl

LOCAL_C_INCLUDES += \
    bootable/recovery \
    vendor/marvell/generic/frameworks/base/native/include

ifeq ($(HOST_OS),linux)
LOCAL_REQUIRED_MODULES := mkfs.f2fs
endif

# Depict custom setting
MV_MISC_RECOVERY_DISP_TYPE := TV

RECOVERY_API_VERSION := 3
RECOVERY_FSTAB_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)
LOCAL_CFLAGS += -Wno-unused-parameter

LOCAL_STATIC_LIBRARIES := \
    libext4_utils_static \
    libsparse_static \
    libminzip \
    libz \
    libmtdutils \
    libmincrypt \
    libminadbd \
    libfusesideload \
    libminui_mrvl \
    libpng \
    libfs_mgr \
    libcutils \
    liblog \
    libselinux \
    libstdc++ \
    libm
ifeq ($(TARGET_USERIMAGES_USE_EXT4), true)
    LOCAL_CFLAGS += -DUSE_EXT4
    LOCAL_C_INCLUDES += system/extras/ext4_utils system/vold
    LOCAL_STATIC_LIBRARIES += libext4_utils_static libz
endif

ifeq ($(MV_MISC_RECOVERY_DISP_TYPE),TV)
    LOCAL_CFLAGS += -DRECOVERY_TV_DISP_OUTPUT
endif

LOCAL_SHARED_LIBRARIES := libampclient libgraphics libOSAL
LOCAL_SHARED_LIBRARIES += libc

# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.
LOCAL_MODULE_TAGS := eng

ifeq ($(TARGET_RECOVERY_UI_LIB),)
  LOCAL_SRC_FILES += mrvl_device.cpp
else
  LOCAL_STATIC_LIBRARIES += $(TARGET_RECOVERY_UI_LIB)
endif

LOCAL_C_INCLUDES += system/extras/ext4_utils
LOCAL_C_INCLUDES += external/openssl/include

include $(BUILD_EXECUTABLE)

include $(LOCAL_PATH)/minui/Android.mk
