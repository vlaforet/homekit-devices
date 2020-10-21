PROGRAM = temp_sensor

HK_PASSWORD=178-85-989
SETUPID=4E6B

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	extras/dht \
	$(abspath ../components/esp-8266/wifi_config) \
	$(abspath ../components/common/wolfssl) \
	$(abspath ../components/common/cJSON) \
	$(abspath ../components/common/homekit) \
	$(abspath ../components/esp-8266/led-status)

EXTRA_CFLAGS += -DHK_PASSWORD=$(HK_PASSWORD) -DSETUPID=$(SETUPID)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
