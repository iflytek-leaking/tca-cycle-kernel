// SPDX-License-Identifier: GPL-2.0
/*
 * CW221X fuel gauge driver (Unisoc integrated version).
 *
 * Reconstructed from the vendor cw221x_battery.ko, which has Rockchip /
 * Cellwise lineage but was integrated by Unisoc:
 *   - the registered power_supply is called "sc27xx-fgu" (not "cw221X-bat")
 *     so that the Unisoc charger-manager can find the fuel gauge by name;
 *   - the design voltage is obtained through sprd_battery_info;
 *   - max-bat-cap / bat-rsense / io-channels are read from DT;
 *   - a raw ADC channel selects between the built-in gf/lf profiles.
 *
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 * Author: Xu Shengfei <xsf@rock-chips.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/property.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/workqueue.h>

/* Module parameters. */
static int debug;
module_param_named(debug, debug, int, 0644);
MODULE_PARM_DESC(debug, "Set to one to enable debugging messages.");

#define cw_printk(fmt, arg...)						\
	do {								\
		if (debug)						\
			pr_info("FG_CW221X: %s-%d: " fmt,		\
				__func__, __LINE__, ##arg);		\
	} while (0)

#define CW_I2C_NAME			"cw221X"
#define CW_PSY_NAME			"sc27xx-fgu"
#define CW_WORKQUEUE_NAME		"cwfg_gauge"

#define SIZE_OF_PROFILE			80
#define CW_PROFILE_ADC_REF		0x51c
#define CW_PROFILE_ADC_DELTA		199

#define CW_DEFAULT_BAT_CAP		10000000	/* uAh */
#define CW_DEFAULT_RSENSE		1500		/* mOhm */

#define CW_QUEUE_DELAY_NORMAL		1250		/* ms */
#define CW_QUEUE_DELAY_DEBUG		250		/* ms */
#define CW_QUEUE_START_JIFFIES		13

/* CW221X registers. */
#define REG_CHIP_ID			0x00
#define REG_VCELL_H			0x02
#define REG_VCELL_L			0x03
#define REG_SOC_INT			0x04
#define REG_SOC_DECIMAL			0x05
#define REG_TEMP			0x06
#define REG_MODE_CONFIG			0x08
#define REG_GPIO_CONFIG			0x0A
#define REG_SOC_ALERT			0x0B
#define REG_TEMP_MAX			0x0C
#define REG_TEMP_MIN			0x0D
#define REG_CURRENT_H			0x0E
#define REG_CURRENT_L			0x0F
#define REG_BAT_PROFILE			0x10
#define REG_T_HOST_H			0xA0
#define REG_T_HOST_L			0xA1
#define REG_USER_CONF			0xA2
#define REG_CYCLE_H			0xA4
#define REG_CYCLE_L			0xA5
#define REG_SOH				0xA6
#define REG_IC_STATE			0xA7
#define REG_FW_VERSION			0xAB

/* Mode / state values. */
#define CONFIG_MODE_RESTART		0x30
#define CONFIG_MODE_ACTIVE		0x00
#define CONFIG_MODE_SLEEP		0xF0
#define CONFIG_UPDATE_FLG		0x80
#define IC_VCHIP_ID			0xA0
#define IC_READY_MARK			0x0C

#define GPIO_ENABLE_MIN_TEMP		0
#define GPIO_ENABLE_MAX_TEMP		0
#define GPIO_ENABLE_SOC_CHANGE		0
#define GPIO_SOC_IRQ_VALUE		0x0
#define DEFINED_MAX_TEMP		45
#define DEFINED_MIN_TEMP		0

/* Delays / retries. */
#define CW_SLEEP_20MS			20
#define CW_SLEEP_10MS			10
#define CW_SLEEP_100MS			100
#define CW_SLEEP_200MS			200
#define CW_SLEEP_COUNTS			50
#define CW_RETRY_COUNT			2
#define CW_TRUE				1

/* Measurement conversion. */
#define CW_UI_FULL			100
#define CW_VOL_UNIT			1000
#define CW_CURRENT_UNIT			1000
#define COMPLEMENT_CODE_U16		0x8000
#define CW2215_MARK			0x80
#define CW2217_MARK			0x40
#define CW2218_MARK			0x00

#define CW_DEBUG_TEMP_INVALID		0x455252

#define CW221X_NOT_ACTIVE		1
#define CW221X_PROFILE_NOT_READY	2
#define CW221X_PROFILE_NEED_UPDATE	3

/*
 * Built-in profiles (80 bytes each). The DT profile is used when present
 * and valid; otherwise the raw ADC value decides between these two curves.
 * lf_config_profile_info is byte-for-byte identical to the Rockchip driver,
 * gf_config_profile_info is the second curve shipped in the vendor module.
 */
static unsigned char lf_config_profile_info[SIZE_OF_PROFILE] = {
	0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0xB2,
	0xC2, 0xCA, 0xC2, 0xBD, 0x9C, 0x5C, 0x38, 0xFF, 0xFF, 0xC4,
	0x86, 0x74, 0x60, 0x55, 0x4F, 0x4D, 0x4B, 0x80, 0xC0, 0xDB,
	0xCD, 0xD0, 0xCE, 0xD2, 0xD3, 0xD2, 0xD0, 0xCE, 0xC3, 0xD5,
	0xB9, 0xC9, 0xC5, 0xA3, 0x92, 0x8A, 0x80, 0x72, 0x63, 0x62,
	0x74, 0x90, 0xA6, 0x7E, 0x5F, 0x48, 0x80, 0x00, 0xAB, 0x10,
	0x00, 0xA1, 0xFB, 0x00, 0x00, 0x00, 0x64, 0x1E, 0xB1, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4D,
};

static unsigned char gf_config_profile_info[SIZE_OF_PROFILE] = {
	0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBD, 0xA8,
	0xA7, 0xC2, 0xA6, 0xA0, 0xDE, 0xC9, 0xB8, 0xFF, 0xFF, 0xD3,
	0x8B, 0x76, 0x60, 0x52, 0x4C, 0x4A, 0x48, 0xC1, 0x9F, 0xDC,
	0x49, 0xE1, 0xD4, 0xD1, 0xD1, 0xCF, 0xCE, 0xCA, 0xC6, 0xC2,
	0xBD, 0xC2, 0xCA, 0xA7, 0x91, 0x88, 0x7D, 0x72, 0x65, 0x67,
	0x7C, 0x91, 0xA4, 0x85, 0x7C, 0x46, 0x20, 0x00, 0xAB, 0x10,
	0x00, 0xA2, 0x19, 0x00, 0x00, 0x00, 0x64, 0x21, 0xB1, 0x1E,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97,
};

struct cw_battery {
	struct i2c_client *client;
	struct device *dev;
	struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;

	struct power_supply *cw_bat;
	u8 *bat_profile;
	int chip_id;
	int voltage;
	int ic_soc_h;
	int ic_soc_l;
	int ui_soc;
	int temp;
	long cw_current;
	int cycle;
	int soh;
	int fw_version;

	int adc_raw;
	struct iio_channel *adc_chan;
	struct iio_channel *charge_vol_chan;
	u32 max_bat_cap;
	u32 rsense;
	int voltage_max_design;
	int debug_temp;
	int unknown_soc_count;
	int dump_done;
};

/* CW221X iic read function */
static int cw_read(struct i2c_client *client, unsigned char reg,
		   unsigned char buf[])
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "IIC error %d\n", ret);

	return ret;
}

/* CW221X iic write function */
static int cw_write(struct i2c_client *client, unsigned char reg,
		    unsigned char const buf[])
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, &buf[0]);
	if (ret < 0)
		dev_err(&client->dev, "IIC error %d\n", ret);

	return ret;
}

/* CW221X iic read word function: read twice to get a stable value. */
static int cw_read_word(struct i2c_client *client, unsigned char reg,
			unsigned char buf[])
{
	unsigned char reg_val[2] = {0, 0};
	unsigned int temp_val_buff;
	unsigned int temp_val_second;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, reg_val);
	if (ret < 0)
		dev_err(&client->dev, "IIC error %d\n", ret);
	temp_val_buff = (reg_val[0] << 8) + reg_val[1];

	msleep(CW_SLEEP_10MS);
	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, reg_val);
	if (ret < 0)
		dev_err(&client->dev, "IIC error %d\n", ret);
	temp_val_second = (reg_val[0] << 8) + reg_val[1];

	if (temp_val_buff != temp_val_second) {
		msleep(CW_SLEEP_10MS);
		ret = i2c_smbus_read_i2c_block_data(client, reg, 2, reg_val);
		if (ret < 0) {
			dev_err(&client->dev, "IIC error %d\n", ret);
			return ret;
		}
	}

	buf[0] = reg_val[0];
	buf[1] = reg_val[1];

	return ret;
}

/* CW221X iic write profile function */
static int cw_write_profile(struct i2c_client *client, unsigned char const buf[])
{
	int ret = 0;
	int i;

	for (i = 0; i < SIZE_OF_PROFILE; i++) {
		ret = cw_write(client, REG_BAT_PROFILE + i, &buf[i]);
		if (ret < 0) {
			dev_err(&client->dev, "IIC error %d\n", ret);
			return ret;
		}
	}

	return ret;
}

/*
 * CW221X Active function.
 * Write 0x30 to exit shutdown mode, then 0x00 to restart the gauge and
 * enter active mode.
 */
static int cw221X_active(struct cw_battery *cw_bat)
{
	unsigned char reg_val = CONFIG_MODE_RESTART;
	int ret;

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_20MS); /* Here delay must be >= 20 ms */

	reg_val = CONFIG_MODE_ACTIVE;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_10MS);

	return 0;
}

/*
 * CW221X Sleep function.
 */
static int cw221X_sleep(struct cw_battery *cw_bat)
{
	unsigned char reg_val = CONFIG_MODE_RESTART;
	int ret;

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_20MS); /* Here delay must be >= 20 ms */

	reg_val = CONFIG_MODE_SLEEP;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_10MS);

	return 0;
}

/* The 0x00 register is fixed to 0xA0 in shutdown and active mode. */
static int cw_get_chip_id(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int ret;

	ret = cw_read(cw_bat->client, REG_CHIP_ID, &reg_val);
	if (ret < 0)
		return ret;

	cw_bat->chip_id = reg_val; /* This value must be 0xA0! */
	pr_info("CW: chip_id = 0x%x\n", cw_bat->chip_id);

	return 0;
}

/*
 * VCELL (0x02/0x03) is an unsigned 14-bit value with 312.5uV resolution.
 * VCELL(mV) = raw * 5 / 16.
 */
static int cw_get_voltage(struct cw_battery *cw_bat)
{
	unsigned char reg_val[2] = {0, 0};
	unsigned int voltage;
	int ret;

	ret = cw_read_word(cw_bat->client, REG_VCELL_H, reg_val);
	if (ret < 0)
		return ret;

	voltage = (reg_val[0] << 8) + reg_val[1];
	voltage = voltage * 5 / 16;
	cw_bat->voltage = voltage;

	return 0;
}

/*
 * SOC (0x04/0x05): the high byte is the integer percent, the low byte is
 * the fractional part (1/256 %). Use the high byte as ui_soc.
 */
static int cw_get_capacity(struct cw_battery *cw_bat)
{
	unsigned char reg_val[2] = {0, 0};
	int soc_h;
	int soc_l;
	int ui_soc;
	int ret;

	ret = cw_read_word(cw_bat->client, REG_SOC_INT, reg_val);
	if (ret < 0)
		return ret;
	soc_h = reg_val[0];
	soc_l = reg_val[1];
	ui_soc = soc_h;
	if (ui_soc >= CW_UI_FULL) {
		cw_printk("CW221x[%d]: UI_SOC = %d larger 100!\n", __LINE__, ui_soc);
		ui_soc = CW_UI_FULL;
	}
	cw_bat->ic_soc_h = soc_h;
	cw_bat->ic_soc_l = soc_l;
	cw_bat->ui_soc = ui_soc;

	return 0;
}

/*
 * TEMP (0x06) is an unsigned 8-bit register, LSB 0.5 degree, in 0.1C:
 * TEMP = raw * 5 - 400.
 */
static int cw_get_temp(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int ret;

	ret = cw_read(cw_bat->client, REG_TEMP, &reg_val);
	if (ret < 0)
		return ret;

	cw_bat->temp = (int)reg_val * 5 - 400;

	return 0;
}

/* get complement code function, unsigned short must be U16 */
static long get_complement_code(unsigned short raw_code)
{
	long complement_code;
	int dir;

	if (raw_code & COMPLEMENT_CODE_U16) {
		dir = -1;
		raw_code = (0xFFFF - raw_code) + 1;
	} else {
		dir = 1;
	}
	complement_code = (long)raw_code * dir;

	return complement_code;
}

/*
 * CURRENT (0x0E/0x0F) is a signed 16-bit two's complement value.
 * CW2215/CW2217: raw * 1600 / rsense, CW2218: raw * 3815 / rsense.
 */
static int cw_get_current(struct cw_battery *cw_bat)
{
	unsigned char reg_val[2] = {0, 0};
	unsigned short current_reg;
	long long cw_current;
	int ret;

	ret = cw_read_word(cw_bat->client, REG_CURRENT_H, reg_val);
	if (ret < 0)
		return ret;

	current_reg = (reg_val[0] << 8) + reg_val[1];
	cw_current = get_complement_code(current_reg);

	if ((cw_bat->fw_version & 0xC0) != 0) {
		cw_current = cw_current * 1600 / cw_bat->rsense;
	} else if (cw_bat->fw_version != 0) {
		cw_current = cw_current * 3815 / cw_bat->rsense;
	} else {
		cw_bat->cw_current = 0;
		dev_err(cw_bat->dev, "error! cw221x firmware read error!\n");
		return 0;
	}

	cw_bat->cw_current = cw_current;

	return 0;
}

/* CYCLECNT (0xA4/0xA5), LSB of the low byte is 1/16 cycle. */
static int cw_get_cycle_count(struct cw_battery *cw_bat)
{
	unsigned char reg_val[2] = {0, 0};
	int cycle;
	int ret;

	ret = cw_read_word(cw_bat->client, REG_CYCLE_H, reg_val);
	if (ret < 0)
		return ret;

	cycle = (reg_val[0] << 8) + reg_val[1];
	cw_bat->cycle = cycle / 16;

	return 0;
}

/* SOH (0xA6), 0x00..0x64. */
static int cw_get_soh(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int ret;

	ret = cw_read(cw_bat->client, REG_SOH, &reg_val);
	if (ret < 0)
		return ret;

	cw_bat->soh = reg_val;

	return 0;
}

/*
 * FW_VERSION (0xAB): bit[7:6] = 01 CW2217, 10 CW2215, 00 CW2218.
 */
static int cw_get_fw_version(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int ret;

	ret = cw_read(cw_bat->client, REG_FW_VERSION, &reg_val);
	if (ret < 0)
		return ret;

	cw_bat->fw_version = reg_val;

	return 0;
}

static int cw_update_data(struct cw_battery *cw_bat)
{
	int ret = 0;

	ret += cw_get_voltage(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_temp(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_cycle_count(cw_bat);
	ret += cw_get_soh(cw_bat);
	cw_printk("vol = %d current = %ld cap = %d temp = %d\n",
		  cw_bat->voltage, cw_bat->cw_current, cw_bat->ui_soc, cw_bat->temp);

	return ret;
}

static int cw_init_data(struct cw_battery *cw_bat)
{
	int ret = 0;

	ret = cw_get_fw_version(cw_bat);
	if (ret != 0)
		return ret;

	ret += cw_get_chip_id(cw_bat);
	ret += cw_get_voltage(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_temp(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_cycle_count(cw_bat);
	ret += cw_get_soh(cw_bat);

	cw_printk("chip_id = %d vol = %d cur = %ld cap = %d temp = %d fw_version = %d\n",
		  cw_bat->chip_id, cw_bat->voltage, cw_bat->cw_current,
		  cw_bat->ui_soc, cw_bat->temp, cw_bat->fw_version);

	return ret;
}

/* Dump registers 0x00..0xFF, used by the debug == 2 mode. */
static void cw_dump_regs(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int i, ret;

	cw_printk("%s\n", __func__);
	for (i = 0; i <= 0xFF; i++) {
		ret = cw_read(cw_bat->client, (unsigned char)i, &reg_val);
		if (ret < 0) {
			dev_err(cw_bat->dev, "%s, read reg 0x%02x failed\n",
				__func__, i);
			continue;
		}
		cw_printk("%s, reg[0x%02x] = 0x%02x\n", __func__, i, reg_val);
	}
}

/*
 * Read the 80-byte profile from DT. Returns 0 on success, a negative
 * errno when there is no usable DT profile.
 */
static int cw221x_parse_properties(struct cw_battery *cw_bat)
{
	struct device *dev = cw_bat->dev;
	int length;
	int ret;

	length = device_property_count_u8(dev, "cellwise,battery-profile");
	if (length < 0) {
		dev_warn(dev, "No battery-profile found, using current flash contents\n");
		return length;
	}
	if (length != SIZE_OF_PROFILE) {
		dev_err(dev, "battery-profile must be %d bytes\n", SIZE_OF_PROFILE);
		return -EINVAL;
	}

	cw_bat->bat_profile = devm_kzalloc(dev, SIZE_OF_PROFILE, GFP_KERNEL);
	if (!cw_bat->bat_profile)
		return -ENOMEM;

	ret = device_property_read_u8_array(dev, "cellwise,battery-profile",
					    cw_bat->bat_profile, SIZE_OF_PROFILE);

	return ret;
}

static void cw_config_profile_init(struct cw_battery *cw_bat)
{
	int distance;
	int i, ret;

	ret = cw221x_parse_properties(cw_bat);
	if (ret) {
		distance = CW_PROFILE_ADC_REF - cw_bat->adc_raw;
		if (distance < 0)
			distance = -distance;
		if (distance <= CW_PROFILE_ADC_DELTA) {
			cw_bat->bat_profile = gf_config_profile_info;
			cw_printk("%s, the driver gf profile:\n", __func__);
		} else {
			cw_bat->bat_profile = lf_config_profile_info;
			cw_printk("%s, the driver lf profile:\n", __func__);
		}
	}

	for (i = 0; i < SIZE_OF_PROFILE; i++)
		cw_printk("[%d]: 0x%x\n", i, cw_bat->bat_profile[i]);
}

/* CW221X update profile function, often called during initialization. */
static int cw_config_start_ic(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int count = 0;
	int i, ret;

	ret = cw221X_sleep(cw_bat);
	if (ret < 0)
		return ret;

	/* update new battery info */
	ret = cw_write_profile(cw_bat->client, cw_bat->bat_profile);
	if (ret < 0)
		return ret;

	cw_printk("the driver profile:\n");
	for (i = 0; i < SIZE_OF_PROFILE; i++)
		cw_printk("[%d]: 0x%x\n", i, cw_bat->bat_profile[i]);

	/* set UPDATE_FLAG and SOC interrupt value */
	reg_val = CONFIG_UPDATE_FLG | GPIO_SOC_IRQ_VALUE;
	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if (ret < 0)
		return ret;

	/* close all interrupts */
	reg_val = 0;
	ret = cw_write(cw_bat->client, REG_GPIO_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	ret = cw221X_active(cw_bat);
	if (ret < 0)
		return ret;

	while (CW_TRUE) {
		msleep(CW_SLEEP_100MS);
		cw_read(cw_bat->client, REG_IC_STATE, &reg_val);
		if (IC_READY_MARK == (reg_val & IC_READY_MARK))
			break;
		count++;
		if (count >= CW_SLEEP_COUNTS) {
			cw221X_sleep(cw_bat);
			return -1;
		}
	}

	return 0;
}

/*
 * Get the CW221X running state and decide whether the profile must be
 * updated.
 */
static int cw221X_get_state(struct cw_battery *cw_bat)
{
	unsigned char reg_val;
	int i, ret;

	ret = cw_read(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	if (reg_val != CONFIG_MODE_ACTIVE)
		return CW221X_NOT_ACTIVE;

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if (ret < 0)
		return ret;
	if (0x00 == (reg_val & CONFIG_UPDATE_FLG))
		return CW221X_PROFILE_NOT_READY;

	for (i = 0; i < SIZE_OF_PROFILE; i++) {
		ret = cw_read(cw_bat->client, (REG_BAT_PROFILE + i), &reg_val);
		if (ret < 0)
			return ret;
		cw_printk("fuelgauge: 0x%2x = 0x%2x\n",
			  REG_BAT_PROFILE + i, reg_val);
		if (cw_bat->bat_profile[i] != reg_val)
			break;
	}
	if (i != SIZE_OF_PROFILE)
		return CW221X_PROFILE_NEED_UPDATE;

	return 0;
}

/* CW221X init function, often called during initialization. */
static int cw_init(struct cw_battery *cw_bat)
{
	int ret;

	ret = cw_get_chip_id(cw_bat);
	if (ret < 0) {
		dev_err(cw_bat->dev, "iic read write error\n");
		return ret;
	}
	if (cw_bat->chip_id != IC_VCHIP_ID) {
		dev_err(cw_bat->dev, "not cw221X\n");
		return -ENODEV;
	}

	ret = cw221X_get_state(cw_bat);
	if (ret < 0) {
		dev_err(cw_bat->dev, "iic read write error\n");
		return ret;
	}

	if (ret != 0) {
		cw_printk("need update profile\n");
		ret = cw_config_start_ic(cw_bat);
		if (ret < 0)
			return ret;
	} else {
		cw_printk("not need update profile\n");
	}
	cw_printk("cw221X init success!\n");

	return 0;
}

/*
 * Fetch the design voltage through the Unisoc battery info helper.
 *
 * NOTE: this tree's struct sprd_battery_info has no "voltage_max_design"
 * member; the closest / intended value is the maximum design voltage,
 * constant_charge_voltage_max_uv. It is exposed as
 * POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN.
 */
static void cw221x_init_sprd_bat_info(struct cw_battery *cw_bat)
{
	struct sprd_battery_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	ret = sprd_battery_get_battery_info(cw_bat->cw_bat, &info);
	if (ret == 0) {
		cw_bat->voltage_max_design = info.constant_charge_voltage_max_uv;
		dev_info(cw_bat->dev, "%s, set voltage_max_design to %d\n",
			 __func__, cw_bat->voltage_max_design);
	} else {
		dev_err(cw_bat->dev,
			"failed to get sprd battery information:%d\n", ret);
	}
	sprd_battery_put_battery_info(cw_bat->cw_bat, &info);
}

static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	static int soc;
	int ret, delay;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	ret = cw_update_data(cw_bat);
	if (ret < 0)
		dev_err(cw_bat->dev, "i2c read error when update data\n");

	if (cw_bat->ui_soc != soc) {
		soc = cw_bat->ui_soc;
		power_supply_changed(cw_bat->cw_bat);
	}

	if ((cw_bat->ui_soc < 1) && power_supply_is_system_supplied() &&
	    (cw_bat->cw_current < 0)) {
		if (++cw_bat->unknown_soc_count > 2)
			power_supply_changed(cw_bat->cw_bat);
	} else {
		cw_bat->unknown_soc_count = 0;
	}

	if (debug == 2) {
		if (!cw_bat->dump_done) {
			dev_info(cw_bat->dev,
				 "Enable log record, will print bat info per second\n");
			cw_dump_regs(cw_bat);
			cw_bat->dump_done = 1;
		}
		delay = CW_QUEUE_DELAY_DEBUG;
	} else {
		if (cw_bat->dump_done) {
			dev_info(cw_bat->dev, "Disable log record\n");
			cw_bat->dump_done = 0;
		}
		delay = CW_QUEUE_DELAY_NORMAL;
	}

	queue_delayed_work(cw_bat->cwfg_workqueue,
			   &cw_bat->battery_delay_work,
			   msecs_to_jiffies(delay));
}

static int cw_battery_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);

	if (psp == POWER_SUPPLY_PROP_CAPACITY) {
		dev_dbg(cw_bat->dev,
			"fgu is embedded on battery, just return, the expected value is %d\n",
			val->intval);
		return 0;
	}

	return -EINVAL;
}

static int cw_battery_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (cw_bat->ui_soc == CW_UI_FULL * 1000)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else if (power_supply_is_system_supplied())
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = (cw_bat->voltage <= 0) ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = cw_bat->cycle;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = cw_bat->ui_soc;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if ((cw_bat->ui_soc < 1) && !power_supply_is_system_supplied())
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else if (cw_bat->ui_soc <= 20)
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (cw_bat->ui_soc <= 70)
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		else if (cw_bat->ui_soc <= 90)
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
		else
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		cw_get_voltage(cw_bat);
		val->intval = cw_bat->voltage * CW_VOL_UNIT;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = cw_bat->voltage_max_design;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		cw_get_current(cw_bat);
		val->intval = cw_bat->cw_current * CW_CURRENT_UNIT;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = iio_read_channel_processed(cw_bat->charge_vol_chan,
						 &val->intval);
		if (ret < 0) {
			dev_err(cw_bat->dev, "get charge voltage failed: %d\n", ret);
			break;
		}
		cw_printk("get charge voltage(vbus): %dmV\n", val->intval);
		val->intval *= CW_VOL_UNIT;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = cw_bat->max_bat_cap;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if ((cw_bat->debug_temp + 100U) < 0xc9U)
			val->intval = cw_bat->debug_temp;
		else
			val->intval = cw_bat->temp;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property cw_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

/* "registers" is a device attribute created on the i2c device. */
static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct cw_battery *cw_bat = dev_get_drvdata(dev);
	unsigned int i, off = 0;
	unsigned char reg_val;
	int ret, len;
	char tmp[32];

	for (i = 0; i < 0xAC; i++) {
		ret = cw_read(cw_bat->client, (unsigned char)i, &reg_val);
		if (ret < 0)
			continue;
		len = scnprintf(tmp, sizeof(tmp), "Reg[%.2X] = 0x%.2x\n",
				i, reg_val);
		if (off + len >= PAGE_SIZE)
			break;
		memcpy(buf + off, tmp, len);
		off += len;
	}

	return off;
}

static ssize_t registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct cw_battery *cw_bat = dev_get_drvdata(dev);
	unsigned int reg, val;
	unsigned char data;
	int ret;

	if ((sscanf(buf, "%x %x", &reg, &val) != 2) || (reg >= 0xAC)) {
		dev_err(dev, "invalid parameter written\n");
		return -EINVAL;
	}

	data = (unsigned char)val;
	ret = cw_write(cw_bat->client, (unsigned char)reg, &data);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(registers);

/* "debug_temp" is a power_supply attribute created on the psy device. */
static ssize_t debug_temp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct cw_battery *cw_bat =
		power_supply_get_drvdata(to_power_supply(dev));

	cw_printk("%s, get debug temp: %d\n", __func__, cw_bat->debug_temp);

	return scnprintf(buf, PAGE_SIZE, "%d", cw_bat->debug_temp);
}

static ssize_t debug_temp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cw_battery *cw_bat =
		power_supply_get_drvdata(to_power_supply(dev));
	int temp = 0xFF;

	if (sscanf(buf, "%d", &temp) != 1) {
		dev_err(cw_bat->dev, "%s, fail to convert debug temp val:%s\n",
			__func__, buf);
		return -EINVAL;
	}

	if ((temp + 100U) > 200U) {
		dev_info(cw_bat->dev,
			 "%s, invalid temperature: %d, disable debug_temp!\n",
			 __func__, temp);
		temp = CW_DEBUG_TEMP_INVALID;
	}
	cw_bat->debug_temp = temp;

	return count;
}
static DEVICE_ATTR_RW(debug_temp);

static int cw221X_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct power_supply_config psy_cfg = {};
	struct power_supply_desc *psy_desc;
	struct cw_battery *cw_bat;
	int loop = 0;
	int ret;

	cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
	if (!cw_bat)
		return -ENOMEM;

	i2c_set_clientdata(client, cw_bat);
	cw_bat->client = client;
	cw_bat->dev = &client->dev;
	cw_bat->debug_temp = CW_DEBUG_TEMP_INVALID;
	cw_bat->rsense = CW_DEFAULT_RSENSE;

	dev_dbg(cw_bat->dev, "cw221X driver version-%d\n", 20220830);

	/* Raw ADC value used to choose the built-in profile. */
	cw_bat->adc_raw = -1;
	cw_bat->adc_chan = iio_channel_get(cw_bat->dev, NULL);
	if (!IS_ERR_OR_NULL(cw_bat->adc_chan)) {
		ret = iio_read_channel_raw(cw_bat->adc_chan, &cw_bat->adc_raw);
		if (ret < 0)
			dev_err(cw_bat->dev, "Failed to read adc channel: %d\n", ret);
	} else {
		cw_printk("%s, failed to get adc channel\n", __func__);
		cw_bat->adc_chan = NULL;
	}

	/* VBUS / charge voltage channel, fatal when unavailable. */
	cw_bat->charge_vol_chan = devm_iio_channel_get(cw_bat->dev,
						       "charge-vol-channel");
	if (IS_ERR(cw_bat->charge_vol_chan)) {
		dev_err(cw_bat->dev, "failed to get charge IIO channel: %ld\n",
			PTR_ERR(cw_bat->charge_vol_chan));
		return -EPROBE_DEFER;
	}

	cw_config_profile_init(cw_bat);

	ret = cw_init(cw_bat);
	while ((loop++ < CW_RETRY_COUNT) && (ret != 0)) {
		msleep(CW_SLEEP_200MS);
		ret = cw_init(cw_bat);
	}
	if (ret) {
		dev_err(cw_bat->dev, "cw221X init fail!\n");
		return ret;
	}

	ret = cw_init_data(cw_bat);
	if (ret) {
		dev_err(cw_bat->dev, "cw221X init data fail!\n");
		return ret;
	}

	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;
	psy_cfg.drv_data = cw_bat;
	psy_cfg.of_node = cw_bat->dev->of_node;
	psy_desc->name = CW_PSY_NAME;
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = cw_battery_properties;
	psy_desc->num_properties = ARRAY_SIZE(cw_battery_properties);
	psy_desc->get_property = cw_battery_get_property;
	psy_desc->set_property = cw_battery_set_property;
	cw_bat->cw_bat = devm_power_supply_register(cw_bat->dev, psy_desc,
						    &psy_cfg);
	if (IS_ERR(cw_bat->cw_bat)) {
		ret = PTR_ERR(cw_bat->cw_bat);
		dev_err(cw_bat->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	cw_bat->cwfg_workqueue = alloc_ordered_workqueue(CW_WORKQUEUE_NAME,
							 WQ_MEM_RECLAIM);
	if (!cw_bat->cwfg_workqueue) {
		dev_err(cw_bat->dev, "failed to alloc workqueue\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->cwfg_workqueue,
			   &cw_bat->battery_delay_work,
			   CW_QUEUE_START_JIFFIES);

	if (device_property_read_u32(cw_bat->dev, "max-bat-cap",
				     &cw_bat->max_bat_cap)) {
		cw_bat->max_bat_cap = CW_DEFAULT_BAT_CAP;
		dev_err(cw_bat->dev,
			"cannot get max capacity preseted, set to %u\n",
			cw_bat->max_bat_cap);
	}
	dev_info(cw_bat->dev, "get bat max capacity: %u\n", cw_bat->max_bat_cap);

	if (device_property_read_u32(cw_bat->dev, "bat-rsense",
				     &cw_bat->rsense) || !cw_bat->rsense) {
		cw_bat->rsense = CW_DEFAULT_RSENSE;
		dev_err(cw_bat->dev, "cannot get rsense, use default(%u)\n",
			cw_bat->rsense);
	}
	dev_info(cw_bat->dev, "get bat rsense: %u\n", cw_bat->rsense);

	cw221x_init_sprd_bat_info(cw_bat);

	if (device_create_file(cw_bat->dev, &dev_attr_registers))
		dev_err(cw_bat->dev, "fail to create registers sysfs\n");
	if (device_create_file(&cw_bat->cw_bat->dev, &dev_attr_debug_temp))
		dev_err(cw_bat->dev, "fail to create debug_temp sysfs\n");

	if (debug != 2)
		cw_bat->dump_done = 0;

	cw_printk("cw221X driver probe success!\n");

	return 0;
}

static int cw221X_remove(struct i2c_client *client)
{
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	device_remove_file(cw_bat->dev, &dev_attr_registers);
	device_remove_file(&cw_bat->cw_bat->dev, &dev_attr_debug_temp);
	cancel_delayed_work_sync(&cw_bat->battery_delay_work);
	destroy_workqueue(cw_bat->cwfg_workqueue);

	return 0;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	queue_delayed_work(cw_bat->cwfg_workqueue,
			   &cw_bat->battery_delay_work,
			   msecs_to_jiffies(20));
	return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
	.suspend = cw_bat_suspend,
	.resume = cw_bat_resume,
};
#endif

static const struct i2c_device_id cw221X_id_table[] = {
	{ CW_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cw221X_id_table);

static const struct of_device_id cw221X_match_table[] = {
	{ .compatible = "cellwise,cw221X", },
	{ },
};
MODULE_DEVICE_TABLE(of, cw221X_match_table);

static struct i2c_driver cw221X_driver = {
	.driver = {
		.name = CW_I2C_NAME,
#ifdef CONFIG_PM
		.pm = &cw_bat_pm_ops,
#endif
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cw221X_match_table),
	},
	.probe = cw221X_probe,
	.remove = cw221X_remove,
	.id_table = cw221X_id_table,
};

module_i2c_driver(cw221X_driver);

MODULE_AUTHOR("Xu Shengfei <xsf@rock-chips.com>");
MODULE_DESCRIPTION("CW221X FGADC Device Driver V0.1");
MODULE_LICENSE("GPL");
MODULE_ALIAS("of:N*T*Ccellwise,cw221X");
