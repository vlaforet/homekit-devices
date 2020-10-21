PROGRAM = display

EXTRA_COMPONENTS = \
	$(abspath ../components/esp-8266/spidriver) \
	$(abspath ../components/esp-8266/tft)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
