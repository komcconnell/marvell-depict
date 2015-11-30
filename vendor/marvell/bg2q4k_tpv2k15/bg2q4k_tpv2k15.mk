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

VENDOR_TOP := vendor/marvell
TARGET_DEVICE_DIRECTORY := bg2q4k_tpv2k15
TARGET_PREBUILT_TOP := vendor/marvell/prebuilts/$(TARGET_DEVICE_DIRECTORY)
TARGET_BOARD_PLATFORM := marvellberlin
PRODUCT_SYSTEM_VERITY_PARTITION := 179:8

PRODUCT_COPY_FILES += \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/frameworks/wifi/p2p_supplicant.conf:system/etc/wifi/p2p_supplicant.conf

$(call inherit-product, vendor/marvell/generic/products/berlin_device.mk)
$(call inherit-product, vendor/marvell/generic/products/berlin_tv.mk)
$(call inherit-product, vendor/marvell/generic/products/berlin_tvinput.mk)
#$(call inherit-product, vendor/marvell/generic/products/berlin_gms.mk)
$(call inherit-product, vendor/marvell/generic/products/berlin_a3ce.mk)
$(call inherit-product, vendor/marvell/generic/products/berlin_wfd.mk)
$(call inherit-product, vendor/marvell/generic/products/berlin_iptv.mk)

PRODUCT_PACKAGES += \
    tpv2k15_audio_samples

PRODUCT_PACKAGES += \
    depict_gpio \
    depict_adc

PRODUCT_PACKAGES += \
    libaudiohw
 
PRODUCT_PACKAGE_OVERLAYS := \
    vendor/marvell/$(TARGET_DEVICE_DIRECTORY)/overlay \
    $(PRODUCT_PACKAGE_OVERLAYS)

PRODUCT_PROPERTY_OVERRIDES += \
    ro.hdmi.vendor_id=0x00903E

PRODUCT_COPY_FILES += \
    $(VENDOR_TOP)/generic/system/core/rootdir/ueventd.berlin.rc:root/ueventd.$(TARGET_BOARD_PLATFORM).rc \
    $(VENDOR_TOP)/generic/system/core/rootdir/init.berlin.tv.rc:root/init.berlin.tv.rc \
    $(VENDOR_TOP)/generic/system/core/rootdir/init.berlin.wfd.rc:root/init.berlin.wfd.rc \
    $(VENDOR_TOP)/generic/system/core/rootdir/init.berlin.tvinput.rc:root/init.berlin.tvinput.rc \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/init.$(TARGET_BOARD_PLATFORM).rc:root/init.$(TARGET_BOARD_PLATFORM).rc \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/init.recovery.$(TARGET_BOARD_PLATFORM).rc:root/init.recovery.$(TARGET_BOARD_PLATFORM).rc \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/fstab.$(TARGET_BOARD_PLATFORM):root/fstab.$(TARGET_BOARD_PLATFORM) \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/Infra-Red.kl:/system/usr/keylayout/Infra-Red.kl \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/avsettings.xml:system/etc/avsettings.xml \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/audio_policy.conf:system/etc/audio_policy.conf \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/bin/testAmplifier:system/vendor/bin/testAmplifier \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/bin/12Nov_spibasictest_latest:system/vendor/bin/12Nov_spibasictest_latest \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/bin/2164Test:system/vendor/bin/2164Test \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/bin/cimax_spidvb_211.bin:system/bin/cimax_spidvb_211.bin \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/lib/libcustomi2c.so:system/vendor/lib/libcustomi2c.so \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/lib/libTPVision.so:system/vendor/lib/libTPVision.so \
    $(VENDOR_TOP)/$(TARGET_DEVICE_DIRECTORY)/tools/bin/i2ctestapp:system/vendor/bin/i2ctestapp

PRODUCT_BRAND := marvell
PRODUCT_MODEL := bg2q4k_tpv2k15
PRODUCT_DEVICE := bg2q4k_tpv2k15
PRODUCT_NAME := bg2q4k_tpv2k15
PRODUCT_MANUFACTURER := marvell
