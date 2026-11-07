// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal Himax touch notification shim for the ZTE UMS9620 kernel.
 *
 * The stock (iFlytek se50) kernel built the Himax HX83102J touch driver into
 * vmlinux and exported himax_resume_by_lcd() and himax_int_enable(); the stock
 * sprd-drm.ko imports both.  The ZTE tree has a different Himax driver version
 * and no HX83102J IC file, so it cannot provide them.  Export the two symbols
 * here (with the exact stock prototypes) so the stock display module loads.
 *
 * Touch input is NOT provided by this module.
 */
#include <linux/module.h>
#include <linux/kernel.h>

void himax_resume_by_lcd(void)
{
	pr_info("himax tp resume after lcd rst high (shim; touch driver absent)\n");
}
EXPORT_SYMBOL(himax_resume_by_lcd);

void himax_int_enable(int enable)
{
	/* no touch controller to gate */
}
EXPORT_SYMBOL(himax_int_enable);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Himax touch notify shim for sprd-drm (HX83102J driver absent)");
