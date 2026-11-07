ccflags-y += -DSTK_SPREADTRUM
ccflags-y += -DSTK_RETRY_I2C
obj-$(CONFIG_SAR_SENSOR_STK501XX)   += vendor_sar_stk.o
vendor_sar_stk-src += stk501xx.c stk501xx_qualcomm.c common_i2c.c common_timer.c common_gpio_qualcomm.c common_storage.c
vendor_sar_stk-objs := $(vendor_sar_stk-src:.c=.o)

obj-$(CONFIG_SAR_SENSOR_STK501XX_2)   += vendor_sar_stk_2.o
vendor_sar_stk_2-objs := stk501xx_2.o stk501xx_qualcomm_2.o common_i2c.o common_timer.o common_gpio_qualcomm.o common_storage.o
