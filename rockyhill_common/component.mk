# Component makefile for rockyhill_common

INC_DIRS += $(rockyhill_common_ROOT)

rockyhill_common_INC_DIR = $(rockyhill_common_ROOT) $(rockyhill_common_ROOT)/../components/esp-8266/led_status $(rockyhill_common_ROOT)/../components/common/homekit $(rockyhill_common_ROOT)/../components/esp-8266/adv_button
rockyhill_common_SRC_DIR = $(rockyhill_common_ROOT)

$(eval $(call component_compile_rules,rockyhill_common))
