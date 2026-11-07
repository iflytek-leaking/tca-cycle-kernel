
# Makefile for the common kernel device drivers.
obj-$(CONFIG_TOUCHSCREEN_VENDOR_V2)	+= touchscreen_v2/
obj-$(CONFIG_NXP_TFA9894_SMARTPA)	+= nxp_tfa9894/
obj-$(CONFIG_POWER_VENDOR_COMMON)	+= power/
# TCA: these five are unconditional obj-y, so enabling CONFIG_VENDOR_COMMON_COMPILE
# (needed only to reach touchscreen_v2/) also built them into vmlinux even though the
# iFlytek SE50 stock config has no VENDOR_COMMON_COMPILE at all and its own tree does not
# compile them.  Their function is provided by the stock modules (sprd_sensor.ko,
# snd-soc-sprd-pa-aw883xx.ko, ...).  Building them in caused a kernel crash on screen-off.
#obj-y   += vcfg/
#for AWINIC AW881XXSmart PA
obj-$(CONFIG_SND_SMARTPA_AW881XX) += awinic/
obj-$(CONFIG_SND_SOC_TFA98XX) += tfa9873/
obj-$(CONFIG_VENDOR_SQC_CHARGER)   += sqc/
obj-$(CONFIG_VENDOR_SQC_CHARGER_V2)   += sqc_v2/
obj-$(CONFIG_VENDOR_SQC_CHARGER_V8)   += sqc_v8/
obj-$(CONFIG_VENDOR_SQC_CHARGER_BASE)   += sqc_base/
obj-$(CONFIG_VENDOR_SQC_CHARGER_V7)   += sqc_v7/
obj-$(CONFIG_VENDOR_DEV_MONITOR_SYSTEM)   += zlog/
#obj-y   += sound/

#for sensor
#obj-y   += sensor/

#for crypto_chip
obj-$(CONFIG_VENDOR_CRYPTO_CHIP) += crypto_chip/

#for breathing light leds
obj-$(CONFIG_VENDOR_BREATHING_LIGHT_LEDS) += leds/

#for zcard_det
#obj-y   += zcard_det/

#lcd
obj-$(CONFIG_VENDOR_LCDBL_I2C_CTRL_VSP_VSN) += vendor_lcdbl/

##### Fingerprint #####
obj-$(CONFIG_VENDOR_PLATFORM_FINGERPRINT)    += fingerprint/

#obj-y += stats_info/
