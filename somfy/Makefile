PROGRAM = somfy

HK_PASSWORD=111-11-111
SETUPID=4E6A

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	$(abspath ../components/esp-8266/wifi_config) \
	$(abspath ../components/common/wolfssl) \
	$(abspath ../components/common/cJSON) \
	$(abspath ../components/common/homekit) \
	$(abspath ../components/esp-8266/esp-button) \
	$(abspath ../components/esp-8266/led-status)

EXTRA_CFLAGS += -DHK_PASSWORD=$(HK_PASSWORD) -DSETUPID=$(SETUPID)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
