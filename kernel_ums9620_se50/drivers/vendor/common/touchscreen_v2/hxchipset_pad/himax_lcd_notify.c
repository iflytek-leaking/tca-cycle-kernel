// SPDX-License-Identifier: GPL-2.0
/*
 * Himax <-> LCD notify bridge.
 *
 * The stock sprd-drm.ko calls himax_resume_by_lcd() right after it drives the
 * panel reset line high.  The Himax "pad" driver does not provide that entry
 * point (it is an iFlytek/ZTE BSP addition), and sprd-drm.ko refuses to load
 * without it, so provide it here and forward to the driver's own resume path.
 *
 * himax_int_enable() is already provided and exported by himax_platform.c.
 */
#include "himax_common.h"

void himax_resume_by_lcd(void)
{
	I("%s: himax tp resume after lcd rst high\n", __func__);
	if (!hx_s_ts) {
		E("%s: hx_s_ts is NULL\n", __func__);
		return;
	}
	himax_chip_common_resume(hx_s_ts);
}
EXPORT_SYMBOL(himax_resume_by_lcd);
