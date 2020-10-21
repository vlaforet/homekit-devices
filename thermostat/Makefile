PROGRAM = thermostat

HK_PASSWORD=211-11-112
SETUPID=3E6F

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	extras/dht \
	$(abspath ../components/esp-8266/wifi_config) \
	$(abspath ../components/common/wolfssl) \
	$(abspath ../components/common/cJSON) \
	$(abspath ../components/common/homekit) \
	$(abspath ../components/esp-8266/adv_button) \
	$(abspath ../components/esp-8266/led-status) \
	$(abspath ../rockyhill_common)

EXTRA_CFLAGS += -I../.. -DHK_PASSWORD=$(HK_PASSWORD) -DSETUPID=$(SETUPID)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
