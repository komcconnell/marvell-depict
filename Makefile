TOP		:= `pwd`
WORK_DIR	:= /work/marvell/workspace.5.1
BUILD_DIR	:= $(WORK_DIR)/vendor/marvell/build

all: build
	@echo "Done!"

build:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y
	@cd $(TOP)

patch:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s patch
	@cd $(TOP)
	make dpatch

linux:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s linux
	@cd $(TOP)

amp_core:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s amp_core
	@cd $(TOP)

mv88de3100_sdk:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s mv88de3100_sdk
	@cd $(TOP)

image:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s image
	@cd $(TOP)

copy_result:
	cd $(WORK_DIR) && ./vendor/marvell/build/build_androidtv -e y -s copy_result
	@cd $(TOP)

#########
dpatch_logo:
	# This is a hack!  The Marvell 'patch' process seems to overwrite android-logo-mask.png
	# so this must be done after their patch!
	cp frameworks/base/core/res/assets/images/depict-logo-mask.png \
	   $(WORK_DIR)/frameworks/base/core/res/assets/images/android-logo-mask.png

dpatch: dpatch_logo
	cp frameworks/base/policy/src/com/android/internal/policy/impl/keyguard/KeyguardServiceDelegate.java \
	   $(WORK_DIR)/frameworks/base/policy/src/com/android/internal/policy/impl/keyguard/KeyguardServiceDelegate.java
	cp external/sepolicy/app.te \
	   $(WORK_DIR)/external/sepolicy/app.te
	cp vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/common/pinmux_setting/bg2dtv_poplar/pinmux_setting.h \
	   $(WORK_DIR)/vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/common/pinmux_setting/bg2dtv_poplar/pinmux_setting.h
	cp vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/bootloader/customization/bg2dtva0/bg2dtv_poplar/platform_customization.c \
	   $(WORK_DIR)/vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/bootloader/customization/bg2dtva0/bg2dtv_poplar/platform_customization.c
	cp vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/system_manager/core/source/sm_power.c \
	   $(WORK_DIR)/vendor/marvell-sdk/MV88DE3100_SDK/MV88DE3100_Tools/bsp/system_manager/core/source/sm_power.c
	mkdir -p $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_adc
	mkdir -p $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_gpio
	cp vendor/marvell/bg2q4k_tpv2k15/AndroidBoard.mk \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/AndroidBoard.mk
	cp vendor/marvell/bg2q4k_tpv2k15/bg2q4k_tpv2k15.mk \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/bg2q4k_tpv2k15.mk
	cp vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_adc/Android.mk \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_adc/Android.mk
	cp vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_adc/depict_adc.c \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_adc/depict_adc.c
	cp vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_gpio/Android.mk \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_gpio/Android.mk
	cp vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_gpio/depict_gpio.c \
	   $(WORK_DIR)/vendor/marvell/bg2q4k_tpv2k15/test_sample/depict_gpio/depict_gpio.c
	mkdir -p $(WORK_DIR)/vendor/marvell/generic/frameworks/hdmi_depict
	cp vendor/marvell/generic/frameworks/hdmi_depict/Android.mk \
	   $(WORK_DIR)/vendor/marvell/generic/frameworks/hdmi_depict/Android.mk
	cp vendor/marvell/generic/frameworks/hdmi_depict/hdmi_depict.c \
	   $(WORK_DIR)/vendor/marvell/generic/frameworks/hdmi_depict/hdmi_depict.c
	cp vendor/marvell/generic/overlays/patches.config \
	   $(WORK_DIR)/vendor/marvell/generic/overlays/patches.config
	cp vendor/marvell/generic/products/berlin_device.mk \
	   $(WORK_DIR)/vendor/marvell/generic/products/berlin_device.mk
	cp vendor/marvell/generic/products/BoardConfigCommon.mk \
	   $(WORK_DIR)/vendor/marvell/generic/products/BoardConfigCommon.mk
	cp vendor/marvell/generic/sepolicy/file_contexts \
	   $(WORK_DIR)/vendor/marvell/generic/sepolicy/file_contexts
	cp vendor/marvell/generic/sepolicy/hdmi_depict.te \
	   $(WORK_DIR)/vendor/marvell/generic/sepolicy/hdmi_depict.te
	cp vendor/marvell/generic/sepolicy/system_app.te \
	   $(WORK_DIR)/vendor/marvell/generic/sepolicy/system_app.te
	cp vendor/marvell/generic/system/core/rootdir/init.berlin.rc \
	   $(WORK_DIR)/vendor/marvell/generic/system/core/rootdir/init.berlin.rc
	cp vendor/marvell/prebuilts/bg2q4k_tpv2k15/amp/berlin_config_sw.xml \
	   $(WORK_DIR)/vendor/marvell/prebuilts/bg2q4k_tpv2k15/amp/berlin_config_sw.xml
	cp vendor/marvell/generic/development/tools/reinstall/reinstall.sh \
	   $(WORK_DIR)/vendor/marvell/generic/development/tools/reinstall/reinstall.sh
