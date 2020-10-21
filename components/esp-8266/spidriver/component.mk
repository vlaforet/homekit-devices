INC_DIRS += $(spidriver_ROOT)

spidriver_INC_DIR = $(spidriver_ROOT)
spidriver_SRC_DIR = $(spidriver_ROOT)

$(eval $(call component_compile_rules,spidriver))
