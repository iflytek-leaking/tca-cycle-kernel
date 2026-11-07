# SPDX-License-Identifier: GPL-2.0
# TCA Cycle Kernel — SE50 (UMS9620 / QogirN6Pro 2H10) 专用设备树。
#
# 保留下来的只有：
#   ums9620.dts                 -> ums9620.dtb      (base)
#   ums9620-2h10-overlay.dts    -> *.dtbo           (SE50 板级 overlay)
#
# 已删除（原厂 BSP 里与本机无关的板型/SoC）：
#   ums9620-1h10 / 2h10_uob / 2h10_uob_marlin3 / 2h10_ufi / 2h10_feimao(飞猫)
#   ums312 / ums512 / ums9230 / ums9520 / sc9863a / sp9863a / ump518
#   sharkl3 / sharkl5 / sharkl5Pro / sharkl64 / qogirl6 / qogirn6pro-haps
#   anthurium / coconut / harrow / peony / staple / tulip / unicorn 板级目录
#   以及本目录下未使用的 lcd/ 与 tp/ 变体

ifneq ($(VENDOR_BOARD_NAME),)
	dts-dirs += $(VENDOR_BOARD_NAME)
else

dtb-$(CONFIG_ARCH_SPRD) += ums9620.dtb

ifeq ($(BSP_BUILD_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_SPRD) += ums9620-2h10-overlay.dtbo
ums9620-2h10-overlay.dtbo-base := ums9620.dtb
endif

endif
