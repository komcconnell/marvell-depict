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

include vendor/marvell/generic/build/definitions.mk

$(call inherit-product, device/google/atv/products/atv_base.mk)

# support /system partition's dm verity
$(call inherit-product, $(SRC_TARGET_DIR)/product/verity.mk)

PRODUCT_PACKAGE_OVERLAYS := \
    vendor/marvell/generic/overlay \
    $(PRODUCT_PACKAGE_OVERLAYS)

# Set limits on heap size.
include vendor/marvell/generic/products/tv-dalvik-heap.mk

# Get a list of languages.
$(call inherit-product, $(SRC_TARGET_DIR)/product/locales_full.mk)

PRODUCT_CHARACTERISTICS := tv,nosdcard
PRODUCT_AAPT_CONFIG += normal mdpi hdpi xhdpi xxhdpi large

# Core Application Intents per CDD
PRODUCT_PACKAGES += \
    Calendar \
    Contacts \
    DeskClock  \
    DocumentsUI \
    DownloadProviderUi \
    Gallery2 \
    Music  \
    MusicFX \
    QuickSearchBox \
    Settings

PRODUCT_PACKAGES += \
    libcrashcounter_jni \
    CrashCounter

PRODUCT_PACKAGES += \
    librmclient \
    librmservice \
    resourcemanager

PRODUCT_PACKAGES += \
    hdmi_cec.$(TARGET_BOARD_PLATFORM) \
    test_cechal

PRODUCT_PACKAGES += \
    e2fsck \
    e2fsck.conf

PRODUCT_PACKAGES += \
    amp_service \
    amp_service_recovery \
    tareg \
    gpio_ctl

PRODUCT_PACKAGES += \
    hdmi_depict \
    backlight_depict

PRODUCT_PACKAGES += \
    install_recovery \
    install-vendor-recovery

PRODUCT_PACKAGES += \
    ethconfig \
    fixdate \
    fts \
    iwpriv \
    iwlist \
    iwconfig \
    iperf \
    rfkill \
    MarvellWirelessDaemon \
    libMarvellWireless \
    wpa_supplicant \
    hostapd \
    kmsglogd

PRODUCT_PACKAGES += \
    keystore.$(TARGET_BOARD_PLATFORM) \
    libext2_blkid \
    libext2_uuid \
    ntfs-3g \
    libblkutil

PRODUCT_PACKAGES += \
   lib_driver_cmd_mrvl \
   MRPlayer \
   CMDExecuter \
   UnitTestTool \
   AudioRouter \
   libmarvell_extractor_jni

PRODUCT_PACKAGES += \
   MultiVideoView

PRODUCT_PACKAGES += \
    libhdmicert \
    HDMI_Cert \

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard $(TARGET_MARVELL_AMP_TOP)/*.xml)),\
        $(TARGET_MARVELL_AMP_TOP)/$(file):system/etc/$(file))

PRODUCT_PACKAGES += \
    $(VENDOR_SDK_BINS) \
    $(VENDOR_SDK_BINS-JIMTCL) \
    $(VENDOR_SDK_LIBS)

PRODUCT_PACKAGES += \
    libgmock \
    libmvutils

PRODUCT_PACKAGES += \
    libtinyalsa \
    tinycap \
    tinymix \
    tinyplay

PRODUCT_PACKAGES += \
    libstagefrighthw \
    libextractorplugin \
    libstagefright_hdcp

PRODUCT_PACKAGES += \
    libBerlinCore \
    libBerlinAudio \
    libBerlinClock \
    libBerlinVideo

PRODUCT_PACKAGES += \
    libopenkode \
    libffmpeg_avutil \
    libffmpeg_avcodec  \
    libffmpeg_avformat \
    libffmpeg_swresample \
    libffmpeg_swscale

PRODUCT_PACKAGES += \
    av_settings \
    lib_avsettings \
    lib_avsettings_client \
    lib_avsettings_impl \
    libavsettings_jni \
    libobserver_jni \
    liboverscan_jni \
    liboverscan_impl \
    libsourcecontrol_jni

PRODUCT_PACKAGES += \
    audio.primary.$(TARGET_BOARD_PLATFORM) \
    audio.usb.default

PRODUCT_PACKAGES += \
    libdirectfb_linux_input \
    libidirectfbimageprovider_jpeg  \
    libidirectfbimageprovider_png   \
    libidirectfbfont_ft2  \
    libidirectfbimageprovider_dfiff  \
    libidirectfbimageprovider_gif \
    libidirectfbfont_dgiff  \
    libidirectfbvideoprovider_gif  \
    libidirectfbvideoprovider_v4l  \
    libdirectfbwm_default  \
    libdirectfb_mvgfx

PRODUCT_PACKAGES += \
    bluetooth.$(TARGET_BOARD_PLATFORM)

PRODUCT_PACKAGES += \
    set_plane \
    jpeg_vout_test \
    jpeg_performance \
    3d_jpeg_sample \
    jpeg_test

PRODUCT_PACKAGES += \
    libsideband \
    test_sideband

PRODUCT_PACKAGES += \
    camera.$(TARGET_BOARD_PLATFORM)

PRODUCT_PACKAGES += \
    audio.a2dp.default \
    audio.a2dp.$(TARGET_BOARD_PLATFORM) \
    audio.r_submix.default

PRODUCT_PACKAGES += \
    libGAL \
    libGLSLC \
    libVSC \
    libEGL_VIVANTE \
    libGLESv1_CM_VIVANTE \
    libGLESv2_VIVANTE \
    gralloc.$(TARGET_BOARD_PLATFORM) \
    hwcomposer.$(TARGET_BOARD_PLATFORM) \
    libdirectfb_gal \
    libVivanteOpenCL \
    libOpenCL \
    libCLC

PRODUCT_PACKAGES += \
    df_andi \
    df_bltload \
    df_cpuload \
    df_databuffer \
    df_dioload \
    df_dok \
    df_drivertest \
    df_fire \
    df_flip \
    df_font \
    df_input \
    df_joystick \
    df_layer \
    df_matrix \
    df_netload \
    df_palette \
    df_particle \
    df_porter \
    df_pss \
    df_stress \
    df_texture \
    df_ve_test \
    df_video \
    df_video_particle \
    df_window \
    df_knuckles \
    df_neo \
    df_spacedream

PRODUCT_PACKAGES += \
    mediaserver_alt \
    player_mmp \
    libmrvl_media_player \
    libmmu \
    libmedia_online_debug \
    MediaDebugClient \
    libstagefrighthw

# Add for liboemcrypto
PRODUCT_PACKAGES += \
    liboemcrypto

# Add into berlin_device.mk libplayreadydrmplugin as TPVisionrequest
PRODUCT_PACKAGES += \
    libplayreadydrmplugin

# Browser for testing
PRODUCT_PACKAGES += \
    Browser \

# Camera apk for testing
PRODUCT_PACKAGES += \
    LegacyCamera

# Provides modules for MRVL "Power Saving Mode"
MV_POWER_USE_MRVL_SUSPEND := TRUE
PRODUCT_PACKAGES += \
    suspend2.$(TARGET_BOARD_PLATFORM) \
    libsoftsuspend2 \
    libpowermessage \
    libsuspend_alt \
    libpowersetting \
    pm_tool \
    PowerSetting

# Provides secure adb key
PRODUCT_PACKAGES += \
    adb_keys

# mount checker apk for testing
PRODUCT_PACKAGES += \
    mount_checker

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/fstab.berlin.s*)),\
        $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/$(file):root/$(file))

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard vendor/marvell/generic/frameworks/base/data/keyboards/*.kl)),\
        vendor/marvell/generic/frameworks/base/data/keyboards/$(file):system/usr/keylayout/$(file))

PRODUCT_COPY_FILES += \
    $(VENDOR_TOP)/generic/system/core/rootdir/init.recovery.berlin.rc:root/init.recovery.berlin.rc \
    vendor/marvell/generic/system/core/rootdir/init.berlin.rc:root/init.berlin.rc

PRODUCT_COPY_FILES += \
    $(TARGET_VIVANTE_LIB_TOP)/directfbrc:system/etc/directfb/directfbrc

PRODUCT_COPY_FILES += \
    $(foreach file, $(filter-out Makefile%, $(notdir $(wildcard vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/*))),\
        vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/$(file):system/usr/directfb/$(file))

PRODUCT_COPY_FILES += \
    $(foreach file, $(filter-out Makefile%, $(notdir $(wildcard vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/df_neo/*))),\
        vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/df_neo/$(file):system/usr/directfb/df_neo/$(file))

PRODUCT_COPY_FILES += \
    $(foreach file, $(filter-out Makefile%, $(notdir $(wildcard vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/spacedream/*))),\
        vendor/marvell/external/directfb/DirectFB-examples-1.6.0/data/spacedream/$(file):system/usr/directfb/spacedream/$(file))

PRODUCT_COPY_FILES += \
    $(TARGET_MARVELL_LINUX_TOP)/kernel:kernel

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard $(TARGET_MARVELL_LINUX_TOP)/modules/*.ko)),\
        $(TARGET_MARVELL_LINUX_TOP)/modules/$(file):system/vendor/lib/modules/$(file))

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard $(TARGET_MARVELL_LINUX_TOP)/modules/*.bin)),\
        $(TARGET_MARVELL_LINUX_TOP)/modules/$(file):system/etc/firmware/mrvl/$(file))

PRODUCT_COPY_FILES += \
    $(foreach file,$(notdir $(wildcard $(TARGET_MARVELL_AMP_TOP)/lib/modules/*.ko)),\
        $(TARGET_MARVELL_AMP_TOP)/lib/modules/$(file):system/vendor/lib/modules/$(file))

PRODUCT_COPY_FILES += \
    vendor/marvell/generic/frameworks/wifi/dhcpcd.conf:system/etc/wifi/dhcpcd.conf \
    vendor/marvell/generic/frameworks/wifi/mef.conf:system/etc/wifi/mef.conf \
    vendor/marvell/generic/frameworks/wifi/p2p_supplicant.conf:system/etc/wifi/p2p_supplicant.conf \
    vendor/marvell/generic/frameworks/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    vendor/marvell/generic/frameworks/wifi/wireless_device_detect.sh:system/bin/wireless_device_detect.sh \
    vendor/marvell/generic/frameworks/wifi/wifi_wakeup_config.sh:system/bin/wifi_wakeup_config.sh \
    vendor/marvell/generic/frameworks/wifi/mlanutl:system/bin/mlanutl \
    vendor/marvell/generic/frameworks/media/media_codecs.xml:system/etc/media_codecs.xml \
    vendor/marvell/generic/frameworks/media/media_codecs_marvell.xml:system/etc/media_codecs_marvell.xml \
    vendor/marvell/generic/frameworks/media/media_profiles.xml:system/etc/media_profiles.xml \
    vendor/marvell/generic/frameworks/multimedia/media_online_debug/DebugableModules.xml:system/etc/DebugableModules.xml \
    frameworks/av/media/libeffects/data/audio_effects.conf:system/etc/audio_effects.conf \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:system/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml

# TODO: Check whehther Google adds them to device/google/atv/tv_core_hardware.xml
# Install common permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:system/etc/permissions/android.hardware.audio.low_latency.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.camera.external.xml:system/etc/permissions/android.hardware.camera.external.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.hardware.hdmi.cec.xml:system/etc/permissions/android.hardware.hdmi.cec.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml

# TODO: Check whehther Google adds them to device/google/atv/tv_core_hardware.xml
# Support microphone live_tv input_methods
PRODUCT_COPY_FILES += \
    vendor/marvell/generic/frameworks/base/data/etc/android.hardware.microphone.xml:system/etc/permissions/android.hardware.microphone.xml \
    vendor/marvell/generic/frameworks/base/data/etc/android.software.input_methods.xml:system/etc/permissions/android.software.input_methods.xml

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=adb

ifeq ($(TARGET_BUILD_VARIANT),user)
ADDITIONAL_DEFAULT_PROPERTIES += ro.adb.secure=1
endif
