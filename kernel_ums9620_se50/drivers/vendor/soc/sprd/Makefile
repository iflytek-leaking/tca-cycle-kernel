#
# Makefile for the kernel device drivers.
#
subdir-ccflags-y += -I$(srctree)/drivers/vendor/common/zlog/zlog_common
ifeq ($(CONFIG_VENDOR_LOG_EXCEPTION),m)
		subdir-ccflags-y+= -DCONFIG_VENDOR_LOG_EXCEPTION
endif

obj-y += board_state/
obj-y += vendor_boot_reason/
obj-y += vendor_uidip_debug/
obj-y += bsp_exception_report/
