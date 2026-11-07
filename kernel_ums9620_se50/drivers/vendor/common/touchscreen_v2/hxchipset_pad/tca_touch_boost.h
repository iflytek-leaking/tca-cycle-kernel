/* SPDX-License-Identifier: GPL-2.0 */
/*
 * tca_touch_boost.h - TCA Cycle Kernel touch tuning knobs (opt-in, default off)
 */
#ifndef _TCA_TOUCH_BOOST_H
#define _TCA_TOUCH_BOOST_H

#include <linux/types.h>
#include <linux/sysfs.h>

struct attribute_group;

void tca_touch_boost_hit(void);
void tca_touch_irq_set(int irq);
extern const struct attribute_group tca_touch_boost_attr_group;

#endif /* _TCA_TOUCH_BOOST_H */
