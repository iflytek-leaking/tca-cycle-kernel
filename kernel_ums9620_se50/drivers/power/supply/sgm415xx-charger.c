// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the SGM41513 family charger (SGM41513/41516/41541/41542/41543).
 *
 * This is the Unisoc integrated variant which talks to the SGM41513 series
 * charger IC over SMBus byte accesses and integrates with sprd-charger-manager,
 * sprd_battery_info, the USB PHY and extcon.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/alarmtimer.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/extcon.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/usb/phy.h>

#include <linux/power/sgm41511_reg.h>

#define SGM41513_BATTERY_NAME			"sc27xx-fgu"
#define SGM41513_OTG_VALID_MS			500
#define SGM41513_FEED_WATCHDOG_VALID_MS		50
#define SGM41513_OTG_ALARM_TIMER_S		15
#define SGM41513_WAKE_UP_MS			1000
#define SGM41513_CURRENT_WORK_MS		msecs_to_jiffies(100)
#define SGM41513_OTG_RETRY_TIMES		10

#define SGM41513_ROLE_MASTER			1
#define SGM41513_ROLE_SLAVE			2

#define SGM41513_DEFAULT_PART_ID		0xd
#define SGM41513_REG_NUM			16

#define SGM41513_REG_0D				0x0d
#define SGM41513_REG_JEITA_MASK			BIT(0)

#define SGM41513_REG_IINLIM_LSB			100000
#define SGM41513_LIMIT_CURRENT_MAX		3200000
#define SGM41513_LIMIT_CURRENT_MIN		100000
#define SGM41513_LIMIT_CURRENT_OFFSET		100000

#define SGM41513_REG_ICHG_LSB			60	/* mA */
#define SGM41513_ICHG_CURRENT_UA_LSB		60000	/* uA */

#define SGM41513_REG_ITERM_LSB			60	/* mA */
#define SGM41513_REG_ITERM_BASE			60	/* mA */
#define SGM41513_ITERM_CURRENT_MAX_UA		960000

#define SGM41513_TERMINA_VOLTAGE_MAX		4624

#define SGM41513_VINPDM_VOLTAGE_MAX		5400

#define SGM41513_FCHG_OVP_6V			6000
#define SGM41513_FCHG_OVP_9V			9000
#define SGM41513_FAST_CHARGER_VOLTAGE_MAX	10500000
#define SGM41513_NORMAL_CHARGER_VOLTAGE_MAX	6500000

#define SGM41513_DISABLE_PIN_MASK		BIT(0)
#define SGM41513_DISABLE_PIN_MASK_2721		BIT(15)
#define BIT_DP_DM_BC_ENB			BIT(0)

#define SGM41513_OTG_DELAY_MS			1500

struct sgm41513_charger_info;

struct sgm41513_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm41513_dump_reg;
	struct device_attribute attr_sgm41513_lookup_reg;
	struct device_attribute attr_sgm41513_sel_reg_id;
	struct device_attribute attr_sgm41513_reg_val;
	struct attribute *attrs[5];
	struct sgm41513_charger_info *info;
};

struct sgm41513_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};

struct sgm41513_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct sgm41513_charge_current cur;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	struct work_struct charger_work;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work cur_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *typec_extcon;
	struct alarm otg_timer;
	struct sgm41513_charger_sysfs *sysfs;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 slave_charger_pd;
	u32 slave_charger_pd_mask;
	u32 new_charge_limit_cur;
	u32 current_charge_limit_cur;
	u32 new_input_limit_cur;
	u32 current_input_limit_cur;
	u32 last_limit_cur;
	u32 usb_limit;
	u32 limit_cur;
	u32 actual_limit_cur;
	u32 role;
	u32 part_id;
	int termina_cur;
	int termina_vol;
	int reg_id;
	unsigned int irq_gpio;
	bool charging;
	bool need_disable_Q1;
	bool disable_power_path;
	bool otg_enable;
	bool is_wireless_charge;
	bool is_charger_online;
};

struct sgm41513_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm41513_charger_reg_tab reg_tab[SGM41513_REG_NUM + 1] = {
	{0,  0x00, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1,  0x01, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2,  0x02, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3,  0x03, "IPRECHG/ITERM"},
	{4,  0x04, "VREG/TOPOFF_TIMER/VRECHG"},
	{5,  0x05, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6,  0x06, "OVP/BOOSTV/VINDPM"},
	{7,  0x07, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8,  0x08, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9,  0x09, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, 0x0a, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, 0x0b, "REG_RST/PN/DEV_REV"},
	{12, 0x0c, "JEITA_VSET_L/JEITA_ISET_L_EN/JEITA_ISET_H/JEITA_VT2/JEITA_VT3"},
	{13, 0x0d, "EN_PUMPX/PUMPX_UP/PUMPX_DN/DP_VSET/DM_VSET/JEITA_EN"},
	{14, 0x0e, "INPUT_DET_DONE"},
	{15, 0x0f, "VREG_FT/STAT_SET/VINDPM_OS"},
	{16, 0x00, "null"},
};

static const int sgm41513_part_id_0 = 0x0;
static const int sgm41513_part_id_1 = 0x1;
static const int sgm41513_part_id_d = 0xd;

static const struct of_device_id sgm41513_charger_of_match[];

static void sgm41513_charger_stop_charge(struct sgm41513_charger_info *info);
static int sgm41513_charger_start_charge(struct sgm41513_charger_info *info);
static void sgm41513_dump_register(struct sgm41513_charger_info *info);
static void sgm41513_check_wireless_charge(struct sgm41513_charger_info *info,
					   bool enable);
static int sgm41513_charger_set_limit_current(struct sgm41513_charger_info *info,
					      u32 limit_cur);
static int sgm41513_charger_set_current(struct sgm41513_charger_info *info,
					u32 cur);
static int sgm41513_charger_set_termina_cur(struct sgm41513_charger_info *info,
					    int cur);
static int sgm41513_charger_get_charge_voltage(struct sgm41513_charger_info *info,
					       u32 *charge_vol);
static int sgm41513_charger_set_status(struct sgm41513_charger_info *info,
				       int val, u32 input_vol);

static int sgm41513_read(struct sgm41513_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm41513_write(struct sgm41513_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm41513_update_bits(struct sgm41513_charger_info *info, u8 reg,
				u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41513_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41513_write(info, reg, v);
}

static void power_path_control(struct sgm41513_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;
	char *match;
	char result[5];

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		info->disable_power_path = false;
		return;
	}

	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		info->disable_power_path = false;
		return;
	}

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		info->disable_power_path = true;

	match = strstr(cmd_line, "androidboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("androidboot.mode=")),
		       sizeof(result) - 1);
		result[sizeof(result) - 1] = '\0';
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;
	}
}

static bool sgm41513_charger_is_bat_present(struct sgm41513_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n",
			ret);

	return present;
}

static int sgm41513_charger_is_fgu_present(struct sgm41513_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

/*
 * sgml41513_charger_set_limit_current() - program the input current limit (REG00)
 *
 * The mapping is chip revision dependent: revision 0/1 uses a segmented
 * transfer curve, later revisions use a flat 100mA/LSB curve.
 */
static int sgm41513_charger_set_limit_current(struct sgm41513_charger_info *info,
					      u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (info->part_id < 2) {
		if (limit_cur > 0xc34 * 1024)
			limit_cur = SGM41513_LIMIT_CURRENT_MAX;

		info->limit_cur = limit_cur;
		reg_val = (limit_cur - SGM41513_LIMIT_CURRENT_MIN) /
			  SGM41513_LIMIT_CURRENT_OFFSET;

		ret = sgm41513_update_bits(info, SGM4151X_REG_00,
					   REG00_IINLIM_MASK, reg_val);
		if (ret)
			dev_err(info->dev, "set sgm41513 limit cur failed\n");

		info->actual_limit_cur =
			(reg_val & 0xff) * SGM41513_LIMIT_CURRENT_OFFSET +
			SGM41513_LIMIT_CURRENT_OFFSET;
		dev_info(info->dev, "%s, limit_cur %d, reg_val 0x%02x\n",
			 __func__, limit_cur, reg_val);
		return ret;
	}

	if (limit_cur < SGM41513_LIMIT_CURRENT_MIN)
		reg_val = 0;
	else if (limit_cur < 0x2f4d61)
		reg_val = (limit_cur - SGM41513_LIMIT_CURRENT_MIN) /
			  SGM41513_LIMIT_CURRENT_OFFSET;
	else {
		reg_val = 0x1e;
		if (limit_cur > 3799999)
			reg_val = 0x1f;
	}

	info->limit_cur = limit_cur;
	dev_info(info->dev, "%s, limit_cur %d, reg_val 0x%02x\n",
		 __func__, limit_cur, reg_val);

	ret = sgm41513_update_bits(info, SGM4151X_REG_00, REG00_IINLIM_MASK,
				   reg_val);
	if (ret)
		dev_err(info->dev, "set sgm41513 limit cur failed\n");

	if (reg_val == 0x1f)
		info->actual_limit_cur = 3800000;
	else
		info->actual_limit_cur =
			reg_val * SGM41513_LIMIT_CURRENT_OFFSET +
			SGM41513_LIMIT_CURRENT_OFFSET;

	return ret;
}

/*
 * sgml41513_charger_set_termina_cur() - program the termination current (REG03)
 *
 * @cur is expressed in mA.
 */
static int sgm41513_charger_set_termina_cur(struct sgm41513_charger_info *info,
					    int cur)
{
	u32 ua = cur * 1000;
	u8 reg_val;

	if (info->part_id < 2) {
		if (ua < 10000)
			reg_val = 0;
		else if (ua < 15000)
			reg_val = 1;
		else if (ua < 20000)
			reg_val = 2;
		else if (ua < 30000)
			reg_val = 3;
		else if (ua < 40000)
			reg_val = 4;
		else if (ua < 50000)
			reg_val = 5;
		else if (ua < 60000)
			reg_val = 6;
		else if (ua < 80000)
			reg_val = 7;
		else if (ua < 100000)
			reg_val = 8;
		else if (ua < 120000)
			reg_val = 9;
		else if (ua < 140000)
			reg_val = 10;
		else if (ua < 160000)
			reg_val = 0xb;
		else if (ua < 180000)
			reg_val = 0xc;
		else if (ua < 200000)
			reg_val = 0xd;
		else if (ua < 240000)
			reg_val = 0xe;
		else
			reg_val = 0xf;
	} else {
		if (ua > SGM41513_ITERM_CURRENT_MAX_UA)
			ua = SGM41513_ITERM_CURRENT_MAX_UA;
		if (ua < SGM41513_REG_ITERM_BASE * 1000)
			ua = SGM41513_REG_ITERM_BASE * 1000;
		reg_val = (ua - SGM41513_REG_ITERM_BASE * 1000) /
			  (SGM41513_REG_ITERM_LSB * 1000);
	}

	return sgm41513_update_bits(info, SGM4151X_REG_03, REG03_ITERM_MASK,
				    reg_val & REG03_ITERM_MASK);
}

static int sgm41513_charger_set_current(struct sgm41513_charger_info *info,
					u32 cur)
{
	u8 reg_val;

	if (info->part_id < 2) {
		if (cur < 40000)
			reg_val = cur / 5000;
		else if (cur < 110000)
			reg_val = (cur - 40000) / 10000 + 8;
		else if (cur < 270000)
			reg_val = (cur - 110000) / 20000 + 0xf;
		else if (cur < 540000)
			reg_val = (cur - 270000) / 30000 + 0x17;
		else if (cur < 1500000)
			reg_val = (cur - 540000) / 60000 + 0x20;
		else if (cur < 3000000)
			reg_val = (cur - 1500000) / 120000 + 0x30;
		else
			reg_val = 0x3d;
	} else {
		if (cur > 0x39ad9f)
			cur = 0x39ada0;
		reg_val = cur / SGM41513_ICHG_CURRENT_UA_LSB;
	}

	dev_info(info->dev, "%s, set current: %duA, reg_val: 0x%02x\n",
		 __func__, cur, reg_val & 0xff);

	return sgm41513_update_bits(info, SGM4151X_REG_02, REG02_ICHG_MASK,
				    reg_val & REG02_ICHG_MASK);
}

static int sgm41513_charger_get_current(struct sgm41513_charger_info *info,
					u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41513_read(info, SGM4151X_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	if (info->part_id < 2) {
		if (reg_val <= 8)
			*cur = reg_val * 5000;
		else if (reg_val < 0x10)
			*cur = reg_val * 10000 - 40000;
		else if (reg_val < 0x18)
			*cur = reg_val * 20000 - 190000;
		else if (reg_val < 0x21)
			*cur = reg_val * 30000 - 420000;
		else if (reg_val < 0x31)
			*cur = reg_val * 60000 - 1380000;
		else if (reg_val <= 0x3c)
			*cur = reg_val * 120000 - 4260000;
		else
			*cur = 3000000;
	} else {
		*cur = reg_val * SGM41513_ICHG_CURRENT_UA_LSB;
	}

	return 0;
}

static int sgm41513_charger_set_vindpm(struct sgm41513_charger_info *info,
				       u32 vol)
{
	u8 reg_val;

	if (vol < REG06_VINDPM_BASE)
		reg_val = 0x0;
	else if (vol > SGM41513_VINPDM_VOLTAGE_MAX)
		reg_val = 0x0f;
	else
		reg_val = (vol - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;

	return sgm41513_update_bits(info, SGM4151X_REG_06, REG06_VINDPM_MASK,
				    reg_val << REG06_VINDPM_SHIFT);
}

static int sgm41513_charger_set_termina_vol(struct sgm41513_charger_info *info,
					    u32 vol)
{
	u8 reg_val;
	int ret;

	if (vol < REG04_VREG_BASE)
		reg_val = 0x0;
	else if (vol >= SGM41513_TERMINA_VOLTAGE_MAX)
		reg_val = 0x18;
	else
		reg_val = (vol - REG04_VREG_BASE) / REG04_VREG_LSB;

	ret = sgm41513_update_bits(info, SGM4151X_REG_04, REG04_VREG_MASK,
				   reg_val << REG04_VREG_SHIFT);
	if (!ret)
		info->termina_vol = (reg_val == 0xf) ? 0x1100 :
				    reg_val * 0x20 + 0xf10;

	return ret;
}

static int sgm41513_charger_set_ovp(struct sgm41513_charger_info *info, u8 ovp)
{
	return sgm41513_update_bits(info, SGM4151X_REG_06, REG06_OVP_MASK,
				    ovp << REG06_OVP_SHIFT);
}

static int sgm41513_charger_en_chg_timer(struct sgm41513_charger_info *info,
					 bool enable)
{
	u8 val;
	int ret;

	ret = sgm41513_read(info, SGM4151X_REG_05, &val);
	if (ret)
		goto err;

	if (enable)
		val |= REG05_EN_TIMER_MASK;
	else
		val &= ~REG05_EN_TIMER_MASK;

	ret = sgm41513_write(info, SGM4151X_REG_05, val);
	if (ret)
		goto err;

	if (!enable)
		dev_info(info->dev, "SGM41513 EN_TIMER is disabled\n");

	return 0;

err:
	dev_err(info->dev, "%s: disable SGM41513 chg_timer failed, ret = %d\n",
		__func__, ret);
	return ret;
}

static int sgm41513_charger_hw_init(struct sgm41513_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt;
	int termination_cur;
	int ret;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;

		voltage_max_microvolt = 4200;
		termination_cur = 100;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	info->termina_cur = termination_cur;

	/* 1. soft reset */
	ret = sgm41513_update_bits(info, SGM4151X_REG_0B, REG0B_REG_RESET_MASK,
				   REG0B_REG_RESET << REG0B_REG_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm41513 failed\n");
		return ret;
	}

	/* 2. newer revisions: disable JEITA (REG0D bit 0) */
	if (info->part_id >= 2) {
		ret = sgm41513_update_bits(info, SGM41513_REG_0D,
					   SGM41513_REG_JEITA_MASK, 0);
		if (ret) {
			dev_err(info->dev, "disable jeita failed\n");
			return ret;
		}
	}

	/* 3. close watchdog timer */
	ret = sgm41513_update_bits(info, SGM4151X_REG_05, REG05_WDT_MASK, 0);
	if (ret) {
		dev_err(info->dev, "close watchdog timer failed\n");
		return ret;
	}

	/* 4. reset watchdog */
	ret = sgm41513_update_bits(info, SGM4151X_REG_01,
				   REG01_WDT_RESET_MASK,
				   REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset watchdog failed\n");
		return ret;
	}

	/* 5. exit HIZ mode */
	ret = sgm41513_update_bits(info, SGM4151X_REG_00, REG00_ENHIZ_MASK, 0);
	if (ret) {
		dev_err(info->dev, "exit HIZ mode failed\n");
		return ret;
	}

	/* 6. charge voltage upper bound / terminate voltage domain */
	ret = sgm41513_update_bits(info, SGM4151X_REG_04, 0xF8, 0xB8);
	if (ret) {
		dev_err(info->dev, "set limit current failed\n");
		return ret;
	}

	/* 7. mask Iindpm/Vindpm interrupts */
	ret = sgm41513_update_bits(info, SGM4151X_REG_0A, REG0A_INT_MASK_MASK,
				   0x03);
	if (ret) {
		dev_err(info->dev, "mask Iindpm/Vindpm failed\n");
		return ret;
	}

	/* 8. OVP threshold */
	if (info->role == SGM41513_ROLE_MASTER) {
		ret = sgm41513_charger_set_ovp(info, 0x3);	/* 0xC0 */
		if (ret) {
			dev_err(info->dev, "set sgm41513 ovp failed\n");
			return ret;
		}
	} else if (info->role == SGM41513_ROLE_SLAVE) {
		ret = sgm41513_charger_set_ovp(info, 0x2);	/* 0x80 */
		if (ret) {
			dev_err(info->dev, "set sgm41513 slave ovp failed\n");
			return ret;
		}
	}

	/* 9. VINDPM */
	ret = sgm41513_charger_set_vindpm(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41513 vindpm vol failed\n");
		return ret;
	}

	/* 10. VREG */
	ret = sgm41513_charger_set_termina_vol(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41513 terminal vol failed\n");
		return ret;
	}

	/* 11. termination current */
	ret = sgm41513_charger_set_termina_cur(info, termination_cur);
	if (ret) {
		dev_err(info->dev, "set sgm41513 terminal cur failed\n");
		return ret;
	}

	/* 12. enable recharge */
	ret = sgm41513_update_bits(info, SGM4151X_REG_04, REG04_VRECHG_MASK,
				   REG04_VRECHG_100MV << REG04_VRECHG_SHIFT);
	if (ret)
		dev_err(info->dev, "set sgm41513 recharge failed\n");

	/* 13. disable the charge safety timer */
	ret = sgm41513_charger_en_chg_timer(info, false);
	if (ret)
		dev_err(info->dev, "failed to disable chg_timer\n");

	/* 14. input current limit */
	ret = sgm41513_charger_set_limit_current(info, info->cur.unknown_cur);
	if (ret)
		dev_err(info->dev, "set sgm41513 limit current failed\n");

	info->current_charge_limit_cur = SGM41513_ICHG_CURRENT_UA_LSB;
	info->current_input_limit_cur = SGM41513_REG_IINLIM_LSB;
	info->new_charge_limit_cur = SGM41513_ICHG_CURRENT_UA_LSB;
	info->new_input_limit_cur = SGM41513_REG_IINLIM_LSB;
	info->limit_cur = info->cur.unknown_cur;

	sgm41513_dump_register(info);

	return ret;
}

static int sgm41513_charger_get_charge_voltage(struct sgm41513_charger_info *info,
					       u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM41513_BATTERY_NAME\n");
		return -ENODEV;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static int sgm41513_charger_check_power_path_status(struct sgm41513_charger_info *info)
{
	int ret = 0;
	u8 val;

	if (info->disable_power_path)
		return 0;

	ret = sgm41513_read(info, SGM4151X_REG_00, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg0(%d)\n",
			__func__, __LINE__, ret);
		return ret;
	}

	if (val & REG00_ENHIZ_MASK) {
		dev_info(info->dev, "%s:line%d, exit hiz mode\n",
			 __func__, __LINE__);
		ret = sgm41513_update_bits(info, SGM4151X_REG_00,
					   REG00_ENHIZ_MASK, 0);
		if (ret < 0) {
			dev_err(info->dev, "%s:line%d, failed to exit hiz(%d)\n",
				__func__, __LINE__, ret);
			return ret;
		}
	}

	return ret;
}

static int sgm41513_charger_start_charge(struct sgm41513_charger_info *info)
{
	int ret;

	ret = sgm41513_update_bits(info, SGM4151X_REG_00, REG00_ENHIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm41513_update_bits(info, SGM4151X_REG_05, REG05_WDT_MASK, 0);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm41513 watchdog\n");
		return ret;
	}

	if (info->role == SGM41513_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	} else if (info->role == SGM41513_ROLE_MASTER) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sgm41513 charge failed\n");
			return ret;
		}

		ret = sgm41513_update_bits(info, SGM4151X_REG_01,
					   REG01_CHG_CONFIG_MASK,
					   REG01_CHG_ENABLE <<
					   REG01_CHG_CONFIG_SHIFT);
		if (ret) {
			dev_err(info->dev, "enable sgm41513 charge en failed\n");
			return ret;
		}
	}

	ret = sgm41513_charger_set_limit_current(info, info->limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm41513_charger_set_termina_cur(info, info->termina_cur);
	if (ret)
		dev_err(info->dev, "set sgm41513 terminal cur failed\n");

	return ret;
}

static void sgm41513_charger_stop_charge(struct sgm41513_charger_info *info)
{
	int ret;

	if (info->role == SGM41513_ROLE_SLAVE) {
		ret = sgm41513_update_bits(info, SGM4151X_REG_00,
					   REG00_ENHIZ_MASK,
					   REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		gpiod_set_value_cansleep(info->gpiod, 1);
	} else if (info->role == SGM41513_ROLE_MASTER) {
		if (info->need_disable_Q1) {
			ret = sgm41513_update_bits(info, SGM4151X_REG_00,
						   REG00_ENHIZ_MASK,
						   REG00_HIZ_ENABLE <<
						   REG00_ENHIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
			info->need_disable_Q1 = false;
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sgm41513 charge failed\n");

		if (info->is_wireless_charge) {
			ret = sgm41513_update_bits(info, SGM4151X_REG_01,
						   REG01_CHG_CONFIG_MASK, 0);
			if (ret)
				dev_err(info->dev,
					"disable sgm41513 charge en failed\n");
		}
	}

	if (info->disable_power_path) {
		ret = sgm41513_update_bits(info, SGM4151X_REG_00,
					   REG00_ENHIZ_MASK,
					   REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = sgm41513_update_bits(info, SGM4151X_REG_05, REG05_WDT_MASK, 0);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm41513 watchdog\n");
}

static void sgm41513_check_wireless_charge(struct sgm41513_charger_info *info,
					   bool enable)
{
	int ret;

	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = sgm41513_charger_set_current(info,
						   info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n",
				__func__);

		ret = sgm41513_charger_set_current(info,
						   info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n",
				__func__);

		pm_wakeup_dev_event(info->dev, SGM41513_WAKE_UP_MS, 0);
		schedule_delayed_work(&info->cur_work,
				      SGM41513_CURRENT_WORK_MS);
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = SGM41513_ICHG_CURRENT_UA_LSB;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = SGM41513_REG_IINLIM_LSB;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = SGM41513_ICHG_CURRENT_UA_LSB;
		info->current_charge_limit_cur = SGM41513_ICHG_CURRENT_UA_LSB;
		info->new_input_limit_cur = SGM41513_REG_IINLIM_LSB;
		info->current_input_limit_cur = SGM41513_REG_IINLIM_LSB;
	}
}

static void sgm41513_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sgm41513_charger_info *info = container_of(dwork,
			struct sgm41513_charger_info, cur_work);
	int ret;
	bool need_return = false;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = sgm41513_charger_set_current(info,
						   info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n",
				__func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = sgm41513_charger_set_limit_current(info,
							 info->new_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n",
				__func__);
		return;
	}

	if (info->current_charge_limit_cur + SGM41513_ICHG_CURRENT_UA_LSB <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += SGM41513_ICHG_CURRENT_UA_LSB;
	else
		need_return = true;

	if (info->current_input_limit_cur + SGM41513_REG_IINLIM_LSB <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += SGM41513_REG_IINLIM_LSB;
	else if (need_return)
		return;

	ret = sgm41513_charger_set_current(info,
					   info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = sgm41513_charger_set_limit_current(info,
						 info->current_input_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev,
		 "set charge_limit_cur %duA, input_limit_curr %duA\n",
		 info->current_charge_limit_cur, info->current_input_limit_cur);

	schedule_delayed_work(&info->cur_work, SGM41513_CURRENT_WORK_MS);
}

static void sgm41513_charger_work(struct work_struct *work)
{
	struct sgm41513_charger_info *info = container_of(work,
			struct sgm41513_charger_info, charger_work);
	bool present;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	present = sgm41513_charger_is_bat_present(info);

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy ? info->usb_phy->chg_type : 0);

	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static int sgm41513_charger_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct sgm41513_charger_info *info = container_of(nb,
			struct sgm41513_charger_info, usb_notify);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	info->usb_limit = limit;

	if (info->role != SGM41513_ROLE_SLAVE) {
		pm_wakeup_dev_event(info->dev, SGM41513_WAKE_UP_MS, 0);
		queue_work(system_wq, &info->charger_work);
	}

	return NOTIFY_OK;
}

static void sgm41513_dump_register(struct sgm41513_charger_info *info)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x0; addr <= 0x0f; addr++) {
		ret = sgm41513_read(info, addr, &val);
		if (ret == 0)
			dev_info(info->dev,
				 "dump reg %s,%d 0x%x = 0x%x\n",
				 __func__, __LINE__, addr, val);
	}
}

static int sgm41513_charger_set_status(struct sgm41513_charger_info *info,
				       int val, u32 input_vol)
{
	int ret = 0;

	if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		ret = sgm41513_charger_set_ovp(info, 0x2);	/* 9V */
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		ret = sgm41513_charger_set_ovp(info, 0x1);	/* 6V */
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == SGM41513_ROLE_MASTER) {
			if (input_vol > SGM41513_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) &&
		   (info->role == SGM41513_ROLE_MASTER)) {
		if (input_vol > SGM41513_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sgm41513_check_wireless_charge(info, false);
		sgm41513_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		sgm41513_check_wireless_charge(info, true);
		ret = sgm41513_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static int sgm41513_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sgm41513_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur = 0, enabled = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->last_limit_cur == 0 && !info->is_wireless_charge) {
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else if (info->charging) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging)
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (info->usb_phy->chg_state == USB_CHARGER_PRESENT);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		cur = info->usb_phy->chg_type;
		if (cur != SDP_TYPE && cur != DCP_TYPE && cur != CDP_TYPE)
			cur = 0;
		val->intval = cur;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41513_charger_get_current(info, &cur);
			if (ret)
				goto out;
			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			u8 reg_val;

			ret = sgm41513_read(info, SGM4151X_REG_00, &reg_val);
			if (ret < 0)
				goto out;

			cur = (reg_val & REG00_IINLIM_MASK) *
			      SGM41513_REG_IINLIM_LSB;
			val->intval = (cur < 3100000) ?
				      cur + SGM41513_LIMIT_CURRENT_OFFSET :
				      SGM41513_LIMIT_CURRENT_MAX;
		}
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev,
				"get sgm41513 charge status failed\n");
			goto out;
		}
		val->intval = !(enabled & info->charger_pd_mask);
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41513_charger_usb_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sgm41513_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 input_vol = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (psp == POWER_SUPPLY_PROP_STATUS) {
		ret = sgm41513_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev,
				"failed to get charge voltage! ret = %d\n", ret);
		}
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_dev_event(info->dev,
					    SGM41513_WAKE_UP_MS, 0);
			schedule_delayed_work(&info->cur_work,
					      SGM41513_CURRENT_WORK_MS * 2);
			break;
		}

		ret = sgm41513_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_dev_event(info->dev,
					    SGM41513_WAKE_UP_MS, 0);
			schedule_delayed_work(&info->cur_work,
					      SGM41513_CURRENT_WORK_MS * 2);
			break;
		}

		ret = sgm41513_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm41513_charger_set_status(info, val->intval, input_vol);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm41513_charger_set_termina_vol(info,
						       val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev,
				"failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			sgm41513_check_wireless_charge(info, true);
			ret = sgm41513_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm41513_check_wireless_charge(info, false);
			sgm41513_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (val->intval == POWER_SUPPLY_TYPE_USB_TYPE_C) {
			info->is_wireless_charge = false;
			ret = sgm41513_charger_set_ovp(info, 0x2);
		} else if (val->intval ==
			   POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = sgm41513_charger_set_ovp(info, 0x3);
		} else {
			info->is_wireless_charge =
				(val->intval ==
				 POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP);
			ret = sgm41513_charger_set_ovp(info, 0x1);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (val->intval == true) {
			schedule_delayed_work(&info->wdt_work, 0);
		} else {
			info->actual_limit_cur = 0;
			cancel_delayed_work_sync(&info->wdt_work);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41513_charger_property_is_writeable(struct power_supply *psy,
						  enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sgm41513_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc sgm41513_charger_desc = {
	.name			= "sgm41513_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41513_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41513_usb_props),
	.get_property		= sgm41513_charger_usb_get_property,
	.set_property		= sgm41513_charger_usb_set_property,
	.property_is_writeable	= sgm41513_charger_property_is_writeable,
};

static const struct power_supply_desc sgm41513_slave_charger_desc = {
	.name			= "sgm41513_slave_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41513_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41513_usb_props),
	.get_property		= sgm41513_charger_usb_get_property,
	.set_property		= sgm41513_charger_usb_set_property,
	.property_is_writeable	= sgm41513_charger_property_is_writeable,
};

static int sgm41513_charger_feed_watchdog(struct sgm41513_charger_info *info)
{
	int ret;

	ret = sgm41513_update_bits(info, SGM4151X_REG_01,
				   REG01_WDT_RESET_MASK,
				   REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret)
		dev_err(info->dev, "reset sgm41513 failed\n");

	return ret;
}

static void sgm41513_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41513_charger_info *info = container_of(dwork,
			struct sgm41513_charger_info, wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = sgm41513_charger_feed_watchdog(info);
	if (ret) {
		dev_err(info->dev, "%s reset sgm41513 wtd failed\n",
			__func__);
		return;
	}

	sgm41513_dump_register(info);
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

static ssize_t sgm41513_register_value_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_reg_val);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE,
				"%s  sgm41513_sysfs->info is null\n",
				__func__);

	ret = sgm41513_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev,
			"fail to get  SGM41513_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE,
				"fail to get  SGM41513_REG_0x%.2x value\n",
				reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "SGM41513_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm41513_register_value_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_reg_val);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm41513_sysfs->info is null\n", __func__);
		return count;
	}

	ret = kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm41513_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev,
			"fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
			val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n",
		 val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm41513_register_id_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_sel_reg_id);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm41513_sysfs->info is null\n", __func__);
		return count;
	}

	ret = kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n",
			sgm41513_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM41513_REG_NUM) {
		dev_err(info->dev,
			"%s store register id fail, id = %d is out of range\n",
			sgm41513_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n",
		 sgm41513_sysfs->name, id);
	return count;
}

static ssize_t sgm41513_register_id_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_sel_reg_id);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE,
				"%s sgm41513_sysfs->info is null\n",
				__func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n",
			info->reg_id);
}

static ssize_t sgm41513_register_table_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_lookup_reg);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[1024];

	if (!info)
		return snprintf(buf, PAGE_SIZE,
				"%s sgm41513_sysfs->info is null\n",
				__func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM41513_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s];\n",
			       reg_tab[i].id, reg_tab[i].addr,
			       reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sgm41513_dump_register_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_dump_reg);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE,
				"%s sgm41513_sysfs->info is null\n",
				__func__);

	sgm41513_dump_register(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", sgm41513_sysfs->name);
}

static int sgm41513_register_sysfs(struct sgm41513_charger_info *info)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs;
	int ret;

	sgm41513_sysfs = devm_kzalloc(info->dev, sizeof(*sgm41513_sysfs),
				      GFP_KERNEL);
	if (!sgm41513_sysfs)
		return -ENOMEM;

	info->sysfs = sgm41513_sysfs;
	sgm41513_sysfs->name = "sgm41513_sysfs";
	sgm41513_sysfs->info = info;
	sgm41513_sysfs->attrs[0] = &sgm41513_sysfs->attr_sgm41513_dump_reg.attr;
	sgm41513_sysfs->attrs[1] = &sgm41513_sysfs->attr_sgm41513_lookup_reg.attr;
	sgm41513_sysfs->attrs[2] = &sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr;
	sgm41513_sysfs->attrs[3] = &sgm41513_sysfs->attr_sgm41513_reg_val.attr;
	sgm41513_sysfs->attrs[4] = NULL;
	sgm41513_sysfs->attr_g.name = "debug";
	sgm41513_sysfs->attr_g.attrs = sgm41513_sysfs->attrs;

	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_dump_reg.attr);
	sgm41513_sysfs->attr_sgm41513_dump_reg.attr.name = "sgm41513_dump_reg";
	sgm41513_sysfs->attr_sgm41513_dump_reg.attr.mode = 0444;
	sgm41513_sysfs->attr_sgm41513_dump_reg.show = sgm41513_dump_register_show;

	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_lookup_reg.attr);
	sgm41513_sysfs->attr_sgm41513_lookup_reg.attr.name =
		"sgm41513_lookup_reg";
	sgm41513_sysfs->attr_sgm41513_lookup_reg.attr.mode = 0444;
	sgm41513_sysfs->attr_sgm41513_lookup_reg.show =
		sgm41513_register_table_show;

	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr);
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr.name =
		"sgm41513_sel_reg_id";
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr.mode = 0644;
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.show =
		sgm41513_register_id_show;
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.store =
		sgm41513_register_id_store;

	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_reg_val.attr);
	sgm41513_sysfs->attr_sgm41513_reg_val.attr.name = "sgm41513_reg_val";
	sgm41513_sysfs->attr_sgm41513_reg_val.attr.mode = 0644;
	sgm41513_sysfs->attr_sgm41513_reg_val.show =
		sgm41513_register_value_show;
	sgm41513_sysfs->attr_sgm41513_reg_val.store =
		sgm41513_register_value_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj,
				 &sgm41513_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);
	u8 val;
	int i, len = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < SGM41513_REG_NUM; i++) {
		if (sgm41513_read(info, i, &val) == 0)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"Reg[%.2X] = 0x%.2x\n", i, val);
	}

	return len;
}

static ssize_t registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);
	unsigned int addr, val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (sscanf(buf, "%x %x", &addr, &val) == 2 && addr < SGM41513_REG_NUM) {
		sgm41513_write(info, addr, val);
	} else {
		dev_err(info->dev, "invalid parameter written\n");
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(registers, 0644, registers_show, registers_store);

static irqreturn_t sgm41513_int_handler(int irq, void *dev_id)
{
	struct sgm41513_charger_info *info = dev_id;
	u8 val;
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	ret = sgm41513_read(info, SGM4151X_REG_0A, &val);
	if (ret < 0) {
		dev_err(info->dev, "set limit cur failed\n");
		val = 0;
	}

	dev_info(info->dev, "interrupt occurs! get regA: 0x%02x\n", val);

	if ((val & REG0A_VBUS_GD_MASK) &&
	    (sgm41513_charger_set_limit_current(info,
						info->actual_limit_cur) != 0))
		dev_err(info->dev, "%s, set input limit cur failed\n",
			__func__);

	sgm41513_dump_register(info);

	return IRQ_HANDLED;
}

/*
 * sgm415xx_disable_pfm() - enable/disable the PFM mode of the charger
 *
 * Exported helper; @en != 0 disables PFM, @en == 0 enables it.
 */
int sgm415xx_disable_pfm(struct sgm41513_charger_info *info, int en)
{
	u8 val;
	int ret;

	if (!info)
		return -EINVAL;

	dev_err(info->dev, "%s, enter\n", __func__);

	ret = sgm41513_read(info, SGM4151X_REG_01, &val);
	if (ret < 0)
		goto err;

	ret = sgm41513_write(info, SGM4151X_REG_01,
			     (val & ~REG01_PFM_DIS_MASK) |
			     ((en ? 1 : 0) << REG01_PFM_DIS_SHIFT));
	if (ret == 0)
		return 0;

err:
	dev_err(info->dev, "fail to set pfm to %d, ret = %d\n", en, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(sgm415xx_disable_pfm);

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sgm41513_charger_check_otg_valid(struct sgm41513_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm41513_read(info, SGM4151X_REG_01, &value);
	if (ret) {
		dev_err(info->dev,
			"get sgm41513 charger otg valid status failed\n");
		return status;
	}

	if (value & REG01_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);

	return status;
}

static bool sgm41513_charger_check_otg_fault(struct sgm41513_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm41513_read(info, SGM4151X_REG_09, &value);
	if (ret) {
		dev_err(info->dev,
			"get sgm41513 charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG09_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sgm41513_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41513_charger_info *info = container_of(dwork,
			struct sgm41513_charger_info, otg_work);
	bool otg_valid;
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	otg_valid = sgm41513_charger_check_otg_valid(info);
	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41513_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm41513_update_bits(info, SGM4151X_REG_01,
						   REG01_OTG_CONFIG_MASK,
						   REG01_OTG_ENABLE <<
						   REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev,
					"restart sgm41513 charger otg failed\n");
		}
		otg_valid = sgm41513_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM41513_OTG_RETRY_TIMES);

	if (retry >= SGM41513_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41513_OTG_DELAY_MS));
}

static int sgm41513_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		goto out;
	}

	ret = sgm41513_update_bits(info, SGM4151X_REG_01,
				   REG01_OTG_CONFIG_MASK,
				   REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable sgm41513 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		goto out;
	}

	sgm415xx_disable_pfm(info, 1);

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41513_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41513_OTG_VALID_MS));
	ret = 0;

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41513_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	ret = sgm41513_update_bits(info, SGM4151X_REG_01,
				   REG01_OTG_CONFIG_MASK,
				   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable sgm41513 otg failed\n");
		goto out;
	}

	/* Enable charger detection function to identify the charger type */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, 0);
	if (ret)
		dev_err(info->dev, "enable BC1.2 failed\n");

	sgm415xx_disable_pfm(info, 0);

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41513_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	ret = sgm41513_read(info, SGM4151X_REG_01, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sgm41513 otg status\n");
		mutex_unlock(&info->lock);
		return ret;
	}
	val &= REG01_OTG_CONFIG_MASK;
	val = (val >> REG01_OTG_CONFIG_SHIFT) & 0x01;
	mutex_unlock(&info->lock);

	return val;
}

static const struct regulator_ops sgm41513_charger_vbus_ops = {
	.enable = sgm41513_charger_enable_otg,
	.disable = sgm41513_charger_disable_otg,
	.is_enabled = sgm41513_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41513_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41513_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm41513_charger_register_vbus_regulator(struct sgm41513_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm41513_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}
#else
static int sgm41513_charger_register_vbus_regulator(struct sgm41513_charger_info *info)
{
	return 0;
}
#endif

static int sgm41513_charger_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm41513_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	const struct of_device_id *match;
	struct gpio_desc *desc;
	u8 reg_val;
	u32 part_id;
	u32 min = 0, max = 0;
	int ret;

	dev_info(dev, "%s, enter\n", __func__);

	if (!adapter) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->dev = dev;

	ret = sgm41513_read(info, SGM4151X_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(dev, "Failed to get vendor id, ret = %d\n", ret);
		return ret;
	}
	part_id = (reg_val & REG0B_PN_MASK) >> REG0B_PN_SHIFT;

	match = of_match_node(sgm41513_charger_of_match, dev->of_node);
	if (!match || !match->data) {
		info->part_id = SGM41513_DEFAULT_PART_ID;
		dev_info(dev,
			 "Can not get of_match data, set to default 0x%02x\n",
			 info->part_id);
	} else {
		info->part_id = *(const int *)match->data;
		dev_info(dev,
			 "Get match data succeed, get vendor id 0x%02x\n",
			 info->part_id);
	}

	if (info->part_id != part_id) {
		dev_err(dev, "The vendor id is 0x%x\n", part_id);
		return -EINVAL;
	}

	i2c_set_clientdata(client, info);
	power_path_control(info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->typec_extcon = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(info->typec_extcon)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->typec_extcon);
	}

	if (sgm41513_charger_is_fgu_present(info))
		dev_err(dev, "warning!!!!!!======> sc27xx_fgu not ready.\n");

	if (device_property_present(dev, "role-slave")) {
		info->role = SGM41513_ROLE_SLAVE;
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			return PTR_ERR(info->gpiod);
		}
	} else {
		info->role = SGM41513_ROLE_MASTER;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL,
						    "sprd,ump962x-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK_2721;
	else
		info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK;

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2, &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	if (info->role == SGM41513_ROLE_SLAVE && IS_ERR(info->gpiod)) {
		ret = of_property_read_u32_index(regmap_np, "reg", 3,
						 &info->slave_charger_pd);
		if (ret) {
			dev_err(dev, "failed to get slave_charger_pd reg\n");
			return ret;
		}
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	mutex_lock(&info->lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == SGM41513_ROLE_MASTER)
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41513_charger_desc,
							   &charger_cfg);
	else
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41513_slave_charger_desc,
							   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_unlock;
	}

	ret = sgm41513_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm41513_charger_hw_init\n");
		goto err_psy;
	}

	sgm41513_charger_stop_charge(info);
	sgm41513_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);
	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sgm41513_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41513_charger_feed_watchdog_work);
	INIT_DELAYED_WORK(&info->cur_work, sgm41513_current_work);
	INIT_WORK(&info->charger_work, sgm41513_charger_work);

	if (info->role == SGM41513_ROLE_MASTER) {
		ret = sgm41513_charger_register_vbus_regulator(info);
		if (ret) {
			dev_err(dev, "failed to register vbus regulator.\n");
			goto err_psy;
		}

		/* 0.5A OTG boost output current limit (REG02 bit 7) */
		ret = sgm41513_update_bits(info, SGM4151X_REG_02,
					   REG02_BOOST_LIM_MASK, 0);
		if (ret)
			dev_err(dev,
				"set otg boost current limit failed ret = %d\n",
				ret);
	}

	info->usb_notify.notifier_call = sgm41513_charger_usb_change;
	ret = atomic_notifier_chain_register(&info->usb_phy->notifier,
					     &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy;
	}

	ret = sgm41513_register_sysfs(info);
	if (ret) {
		dev_err(dev, "register sysfs fail, ret = %d\n", ret);
		goto err_notifier;
	}

	info->irq_gpio = of_get_named_gpio_flags(dev->of_node, "irq-gpio", 0,
						 NULL);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(dev, info->irq_gpio, GPIOF_DIR_IN,
					    "sgm41513_int");
		if (!ret) {
			desc = gpio_to_desc(info->irq_gpio);
			client->irq = gpiod_to_irq(desc);
		} else {
			dev_err(dev, "int request failed, ret = %d\n", ret);
		}

		if (client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(dev, client->irq, NULL,
							sgm41513_int_handler,
							IRQF_TRIGGER_FALLING |
							IRQF_ONESHOT,
							"sgm41513 interrupt",
							info);
			if (ret)
				dev_err(dev, "Failed irq = %d ret = %d\n",
					client->irq, ret);
			else
				irq_set_irq_wake(client->irq, 1);
		}
	} else {
		dev_err(dev, "failed to get irq gpio\n");
	}

	mutex_unlock(&info->lock);

	if (info->usb_phy->chg_state == USB_CHARGER_PRESENT) {
		usb_phy_get_charger_current(info->usb_phy, &min, &max);
		info->last_limit_cur = min;
		if (info->role != SGM41513_ROLE_SLAVE)
			queue_work(system_wq, &info->charger_work);
	}

	ret = device_create_file(dev, &dev_attr_registers);
	if (ret)
		dev_err(dev, "fail to create resigters sysfs\n");

	dev_info(dev, "%s:line%d: probe success\n", __func__, __LINE__);
	sgm41513_dump_register(info);

	return 0;

err_notifier:
	atomic_notifier_chain_unregister(&info->usb_phy->notifier,
					 &info->usb_notify);
err_psy:
err_unlock:
	mutex_unlock(&info->lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm41513_charger_shutdown(struct i2c_client *client)
{
	struct sgm41513_charger_info *info = i2c_get_clientdata(client);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);

		ret = sgm41513_update_bits(info, SGM4151X_REG_01,
					   REG01_OTG_CONFIG_MASK,
					   REG01_OTG_DISABLE <<
					   REG01_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev,
				"disable sgm41513 otg failed ret = %d\n", ret);

		ret = sgm41513_update_bits(info, SGM4151X_REG_00,
					   REG00_ENHIZ_MASK,
					   REG00_HIZ_ENABLE <<
					   REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");

		/* Enable charger detection function */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n",
				ret);
	}
}

static int sgm41513_charger_remove(struct i2c_client *client)
{
	struct sgm41513_charger_info *info = i2c_get_clientdata(client);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	device_remove_file(info->dev, &dev_attr_registers);
	if (info->sysfs)
		sysfs_remove_group(&info->psy_usb->dev.kobj,
				   &info->sysfs->attr_g);
	atomic_notifier_chain_unregister(&info->usb_phy->notifier,
					 &info->usb_notify);
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	cancel_delayed_work_sync(&info->cur_work);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sgm41513_charger_suspend(struct device *dev)
{
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->cur_work);

	ret = sgm41513_charger_feed_watchdog(info);
	if (ret)
		dev_warn(info->dev, "reset sgm41513 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(SGM41513_OTG_ALARM_TIMER_S, 0);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sgm41513_charger_resume(struct device *dev)
{
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	ret = sgm41513_charger_feed_watchdog(info);
	if (ret)
		dev_warn(info->dev, "reset sgm41513 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops sgm41513_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41513_charger_suspend,
				sgm41513_charger_resume)
};

static const struct i2c_device_id sgm41513_i2c_id[] = {
	{"sgm41513_chg", 0},
	{"sgm41513_slave_chg", 0},
	{}
};

static const struct of_device_id sgm41513_charger_of_match[] = {
	{ .compatible = "sgm,sgm41513_chg",  .data = &sgm41513_part_id_0 },
	{ .compatible = "sgm,sgm41513x_chg", .data = &sgm41513_part_id_1 },
	{ .compatible = "sgm,sgm41541_chg",  .data = &sgm41513_part_id_d },
	{ .compatible = "sgm,sgm41542_chg",  .data = &sgm41513_part_id_d },
	{ .compatible = "sgm,sgm41543_chg",  .data = &sgm41513_part_id_d },
	{ .compatible = "sgm,sgm41543D_chg", .data = &sgm41513_part_id_d },
	{ .compatible = "sgm,sgm41516_chg",  .data = &sgm41513_part_id_d },
	{ .compatible = "sgm,sgm41516D_chg", .data = &sgm41513_part_id_d },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm41513_charger_of_match);

static struct i2c_driver sgm41513_master_charger_driver = {
	.driver = {
		.name = "sgm41513_chg",
		.of_match_table = sgm41513_charger_of_match,
		.pm = &sgm41513_charger_pm_ops,
	},
	.probe = sgm41513_charger_probe,
	.shutdown = sgm41513_charger_shutdown,
	.remove = sgm41513_charger_remove,
	.id_table = sgm41513_i2c_id,
};

module_i2c_driver(sgm41513_master_charger_driver);

MODULE_DESCRIPTION("SGM41513 Charger Driver");
MODULE_LICENSE("GPL v2");
