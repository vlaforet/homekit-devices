PROGRAM = clock

HK_PASSWORD=647-15-639
SETUPID=8E6A

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	extras/i2s_dma extras/ws2812_i2s \
	extras/sntp \
	$(abspath ../components/esp-8266/wifi_config) \
	$(abspath ../components/common/wolfssl) \
	$(abspath ../components/common/cJSON) \
	$(abspath ../components/common/homekit) \
	$(abspath ../components/esp-8266/esp-button) \
	$(abspath ../components/esp-8266/led-status)
LIBS+=m

EXTRA_CFLAGS += -DHK_PASSWORD=$(HK_PASSWORD) -DSETUPID=$(SETUPID)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
