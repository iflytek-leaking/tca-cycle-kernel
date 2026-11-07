#
# Makefile for vendor fingerprint driver.
#

#zlog interface
ifeq ($(CONFIG_VENDOR_LOG_EXCEPTION),m)
	ccflags-y += -DCONFIG_VENDOR_LOG_EXCEPTION
	ccflags-y += -I$(srctree)/include/vendor/comdef/
endif

obj-$(CONFIG_VENDOR_PLATFORM_FINGERPRINT) += vendor_fp.o
vendor_fp-y := vendor_fp_entry.o

########### cdfinger ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_CDFINGER)  += \
	cdfinger/cdfinger_tee.o

########### chipone ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_CHIPONE)   += \
	chipone/fpsensor_spi.o

########### egis ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_EGIS)      += \
	egis/et520.o \
	egis/ets_navi_input.o

########### goodix ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_GOODIX)    += \
	goodix/gf_spi.o \
	goodix/platform.o \
	goodix/netlink.o

########### silead ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_SILEAD)    += \
	silead/silead_fp.o \
	silead/silead_fp_platform.o

########### sunwave ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_SUNWAVE)   += \
	sunwave/sf_ctl.o \
	sunwave/sf_hw.o

########### fpc ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_FPC)   += \
	fpc/fpc_spi.o

vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_FPC1020)   += \
	fpc1020/fpc1020.o

########### focaltech ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_FOCALTECH)   += \
	focaltech/ff_ctl.o \
	focaltech/ff_hw.o

########### focaltech_v2 ###############
vendor_fp-$(CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2)   += \
	focaltech_v2/ff_core.o

BOARD_VENDOR_RAMDISK_KERNEL_MODULES += 	$(BSP_KERNEL_MODULES_OUT)/vendor_fp.ko
