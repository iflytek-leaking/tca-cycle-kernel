# Makefile for the common kernel device drivers.

obj-$(CONFIG_VENDOR_LEDS_AW210XX)	+= aw210xx/
obj-$(CONFIG_VENDOR_LEDS_AW2016)	+= aw2016/

ifeq ($(CONFIG_VENDOR_MISC),m)
	subdir-ccflags-y += -DCONFIG_VENDOR_MISC_COMMON
endif