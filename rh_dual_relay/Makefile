PROGRAM = rh_dual_relay

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	$(abspath ../components/esp-8266/wifi_config) \
	$(abspath ../components/common/wolfssl) \
	$(abspath ../components/common/cJSON) \
	$(abspath ../components/common/homekit) \
	$(abspath ../components/esp-8266/adv_button) \
	$(abspath ../components/esp-8266/led-status)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
