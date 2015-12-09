#                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
# THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
# NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
# OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
# DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
# THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
# IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
#
# MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
# MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
# SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
# (MJKK), MARVELL ISRAEL LTD. (MSIL).

# The generic product target doesn't have any hardware-specific pieces.
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true
TARGET_PROVIDES_INIT_RC := false
TARGET_PROVIDE_GRALLOC:= true
TARGET_DISABLE_TRIPLE_BUFFERING := true
BOARD_USES_GENERIC_AUDIO := false
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_RADIO := false
BOARD_ENABLE_HELIX := false
BOARD_WLAN_DEVICE := mrvl
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_mrvl
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_mrvl
WIFI_DRIVER_MODULE_ARG := "cfg80211_wext=0xf sta_name=wlan wfd_name=p2p max_vir_bss=1 drv_mode=5"
BOARD_HOSTAPD_DRIVER := NL80211

PRODUCT_OPENGLES_VERSION := 131072
USE_OPENGL_RENDERER := true
BOARD_EGL_CFG := vendor/marvell/generic/frameworks/gfx/egl.cfg

WIFI_CHIP_TYPE := auto

ifeq ($(BOARD_KERNEL_BASE),)
BOARD_KERNEL_BASE := 0x01000000
endif

ifeq ($(BOARD_KERNEL_CMDLINE),)
BOARD_KERNEL_CMDLINE := androidboot.console=ttyS0 console=ttyS0,115200 init=/init quiet root=/dev/ram0
endif

# Include default feature control file
include vendor/marvell/generic/products/berlin.config
# Include target product feature control file
include $(VENDOR_TOP)/$(TARGET_DEVICE)/berlin.config
# Include the platform specific features
-include $(VENDOR_TOP)/$(TARGET_DEVICE)/berlin-$(PLATFORM_SDK_VERSION).config

TARGET_CRASHCOUNTER_LIB := libcrashcounter_berlin
TARGET_RECOVERY_UPDATER_LIBS := librecovery_updater_berlin

ifeq ($(TARGET_RELEASETOOLS_EXTENSIONS),)
TARGET_RELEASETOOLS_EXTENSIONS := $(VENDOR_TOP)/$(TARGET_DEVICE)/recovery/releasetools/berlin_ota
endif

VENDOR_SDK_INCLUDES := $(shell find $(TARGET_MARVELL_AMP_TOP)/include -type d)
VENDOR_SDK_CFLAGS := -D_ANDROID_ -D__LINUX__

VIVANTE_INCLUDES := $(shell find $(TARGET_VIVANTE_LIB_TOP)/include -type d)
VIVANTE_LDFLAGS := -L$(TARGET_VIVANTE_LIB_TOP) -Wl,-rpath-link=$(TARGET_VIVANTE_LIB_TOP)

ifneq ($(strip $(MRVL_MISC_FOLDER)),)
MRVL_MISC_INCLUDES := $(shell find $(MRVL_MISC_FOLDER) -type d)
else
MRVL_MISC_INCLUDES := $(shell find $(TARGET_COMMON_PREBUILT_TOP)/include -type d)
endif

TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
BOARD_DBUS_DIRS := vendor/marvell/external/dbus

# Target products should overwrite these two macros if they hope to use their own policy
BOARD_SEPOLICY_DIRS := \
    vendor/marvell/generic/sepolicy

BOARD_SEPOLICY_UNION := \
    adbd.te \
    ampservice.te \
    ampaudioloopbackservice.te \
    app_common.te \
    av_settings.te \
    bootanim.te \
    cpusvc.te \
    bluetooth.te \
    debuggerd.te \
    device.te \
    domain_common.te \
    dumpstate.te \
    ethconfig.te \
    file_contexts \
    file.te \
    fixdate.te \
    init.te \
    kmsglogd.te \
    mediaserver.te \
    netd.te \
    recovery.te \
    resourcemanager.te \
    service.te \
    service_contexts \
    sdcardd.te \
    shell.te \
    surfaceflinger.te \
    system_app.te \
    system_server.te \
    te_macros \
    voicecapture.te \
    wireless_init.te \
    wpa.te \
    zygote.te \
    hdmi_depict.te \
    backlight_depict.te


# Default hardware platform and buffer number
MV_GFX_SOC := BG2DTV
MV_GFX_BUFFER_NUM := 3
