obj-y += bsp_exception_report.o
ifeq ($(CONFIG_VENDOR_LOG_EXCEPTION),m)
subdir-ccflags-y+= -I$(srctree)/include/vendor/comdef/
endif
ifeq ($(CONFIG_VENDOR_LOG_EXCEPTION),y)
subdir-ccflags-y+= -I$(srctree)/include/vendor/comdef/
endif
