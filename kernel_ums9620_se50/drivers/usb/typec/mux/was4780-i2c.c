// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc WAS4780 Type-C analog switch / audio mux driver (I2C)
 *
 * Reconstructed from the stock, unstripped vendor module
 *   modules/was4780-i2c.ko
 * (Unisoc UMS9620 / VENDOR mifi f50, Linux 5.4.210 aarch64 Android13 GKI).
 *
 * The register map and the switch sequences are FSA4480-compatible; the
 * vendor file is a descendant of drivers/soc/sprd/fsa4480/fsa4480-i2c.c.
 * Register values were recovered from the .ko disassembly (objdump/Ghidra)
 * and cross-checked against include/linux/regmap.h struct offsets.
 *
 * DT (se50.dts:1168):
 *   WAS4780-i2c@42 {
 *       compatible = "sprd,WAS4780-i2c";
 *       reg = <0x42>;
 *       dio_en_gpio = <...>;
 *       extcon = <...>;
 *   };
 *
 * Exported API (all EXPORT_SYMBOL, as confirmed from __ksymtab):
 *   WAS4780_switch_event()
 *   WAS4780_reg_notifier()
 *   WAS4780_unreg_notifier()
 *   was4780_tcpc_event_changed()
 *   was4780_tcpc_get_micgnd_status()
 *   was4780_tcpc_micgnd_changed()
 */

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>

#define WAS4780_I2C_NAME		"was4780-driver"

/* Register map (FSA4480 compatible) */
#define WAS4780_SWITCH_SETTINGS		0x04
#define WAS4780_SWITCH_CONTROL		0x05
#define WAS4780_SWITCH_STATUS1		0x06
#define WAS4780_SWITCH_STATUS2		0x07
#define WAS4780_SLOW_L			0x08
#define WAS4780_SLOW_R			0x09
#define WAS4780_SLOW_MIC		0x0a
#define WAS4780_SLOW_SENSE		0x0b
#define WAS4780_SLOW_GND		0x0c
#define WAS4780_DELAY_L_R		0x0d
#define WAS4780_DELAY_L_MIC		0x0e
#define WAS4780_DELAY_L_SENSE		0x0f
#define WAS4780_DELAY_L_AGND		0x10
#define WAS4780_RESET			0x1e

/*
 * Vendor power_supply property used to learn the Type-C analog mode.
 * The stock .ko reads property 0x4b (one past POWER_SUPPLY_PROP_SERIAL_NUMBER
 * in the VENDOR/mainline enum); the vendor kernel appends a private TYPEC_MODE
 * property there.  Values returned by the supply:
 *   0x4c -> audio accessory attached
 *   0x4d -> audio accessory detached / other
 */
#define WAS4780_PROP_TYPEC_MODE		0x4b
#define WAS4780_TYPEC_MODE_AUDIO	0x4c
#define WAS4780_TYPEC_MODE_NONE		0x4d

/* Switch events accepted by WAS4780_switch_event() */
enum was4780_function {
	WAS4780_MIC_GND_SWAP = 0,
	WAS4780_USBC_ORIENTATION_CC1,
	WAS4780_USBC_ORIENTATION_CC2,
	WAS4780_USBC_DISPLAYPORT_DISCONNECTED,
	WAS4780_EVENT_MAX,
};

struct was4780_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head was4780_notifier;
	struct mutex notification_lock;
};

struct was4780_reg_val {
	u8 reg;
	u8 val;
};

/*
 * The stock module keeps the private data in a file-scope pointer; the
 * exported tcpc helpers operate on it directly.
 */
static struct was4780_priv *was4780_priv1;

static const struct regmap_config WAS4780_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = WAS4780_RESET,
};

static const struct was4780_reg_val WAS4780_reg_i2c_defaults[] = {
	{ WAS4780_SLOW_L, 0x00 },
	{ WAS4780_SLOW_R, 0x00 },
	{ WAS4780_SLOW_MIC, 0x00 },
	{ WAS4780_SLOW_SENSE, 0x00 },
	{ WAS4780_SLOW_GND, 0x00 },
	{ WAS4780_DELAY_L_R, 0x00 },
	{ WAS4780_DELAY_L_MIC, 0x00 },
	{ WAS4780_DELAY_L_SENSE, 0x00 },
	{ WAS4780_DELAY_L_AGND, 0x09 },
	{ WAS4780_SWITCH_SETTINGS, 0x98 },
};

static void WAS4780_usbc_update_settings(struct was4780_priv *priv,
					 u32 switch_control, u32 switch_enable);
static int WAS4780_usbc_analog_setup_switches(struct was4780_priv *priv);
static int WAS4780_validate_display_port_settings(struct was4780_priv *priv);

/*
 * was4780_tcpc_micgnd_changed - enable/disable MIC-GND swap
 * @micgnd_on: true to swap (control bits 2:0 = 7), false to restore.
 */
void was4780_tcpc_micgnd_changed(bool micgnd_on)
{
	struct was4780_priv *priv = was4780_priv1;
	u32 ctrl_val = 0;
	u32 reg6 = 0, reg7 = 0;

	if (!priv || !priv->regmap)
		return;

	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);
	dev_dbg(priv->dev,
		"%s: ctrl_val= %d 0x06 = %x 0x07 = %x\n",
		__func__, 0, reg6, reg7);

	regmap_read(priv->regmap, WAS4780_SWITCH_CONTROL, &ctrl_val);
	if (micgnd_on)
		ctrl_val |= 0x7;
	else
		ctrl_val &= ~0x7;
	regmap_write(priv->regmap, WAS4780_SWITCH_CONTROL, ctrl_val);

	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);
	dev_dbg(priv->dev,
		"%s: ctrl_val= %d 0x06 = %x 0x07 = %x\n",
		__func__, ctrl_val, reg6, reg7);
}
EXPORT_SYMBOL(was4780_tcpc_micgnd_changed);

/*
 * was4780_tcpc_get_micgnd_status - true when MIC-GND swap is active
 * (control register bits 2:0 == 7).
 */
bool was4780_tcpc_get_micgnd_status(void)
{
	struct was4780_priv *priv = was4780_priv1;
	u32 ctrl_val = 0;

	if (!priv || !priv->regmap)
		return 0;

	regmap_read(priv->regmap, WAS4780_SWITCH_CONTROL, &ctrl_val);

	return (ctrl_val & 0x7) == 0x7;
}
EXPORT_SYMBOL(was4780_tcpc_get_micgnd_status);

/*
 * was4780_tcpc_event_changed - apply the switch state requested by tcpc
 * @event: 0 -> switch the AUX/SBU path to DPDN (CC1); non-zero -> restore.
 */
void was4780_tcpc_event_changed(int event)
{
	struct was4780_priv *priv = was4780_priv1;
	u32 reg6 = 0, reg7 = 0;
	int retry;

	if (!priv || !priv->regmap)
		return;

	if (event == 0) {
		WAS4780_usbc_update_settings(priv, 0x18, 0x9f);
		usleep_range(260, 300);
		regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
		regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);

		if ((reg6 & 0x5) != 0x5) {
			for (retry = 1; ; retry++) {
				pr_err("Have not switch to dpdn correctly:0x06(0x%02x),0x07(0x%02x), try again...\n",
				       reg6, reg7);
				WAS4780_usbc_update_settings(priv, 0x18, 0x9f);
				usleep_range(260, 300);
				regmap_read(priv->regmap,
					    WAS4780_SWITCH_STATUS1, &reg6);
				regmap_read(priv->regmap,
					    WAS4780_SWITCH_STATUS2, &reg7);
				if (retry > 4)
					break;
				if ((reg6 & 0x5) == 0x5)
					break;
			}
		}
		dev_dbg(priv->dev,
			"%s: 0x06 = %x 0x07 = %x\n", __func__, reg6, reg7);
	} else {
		WAS4780_usbc_update_settings(priv, 0x00, 0x9f);
		regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
		regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);
		dev_dbg(priv->dev,
			"%s: 0x06 = %x 0x07 = %x\n", __func__, reg6, reg7);
	}
}
EXPORT_SYMBOL(was4780_tcpc_event_changed);

static void WAS4780_update_reg_defaults(struct regmap *regmap)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(WAS4780_reg_i2c_defaults); i++)
		regmap_write(regmap, WAS4780_reg_i2c_defaults[i].reg,
			     WAS4780_reg_i2c_defaults[i].val);
}

static void WAS4780_usbc_update_settings(struct was4780_priv *priv,
					 u32 switch_control, u32 switch_enable)
{
	u32 reg6 = 0, reg7 = 0;

	if (!priv->regmap) {
		dev_err(priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(priv->regmap, WAS4780_SWITCH_SETTINGS, 0x80);
	regmap_write(priv->regmap, WAS4780_SWITCH_CONTROL, switch_control);
	/* WAS4780 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(priv->regmap, WAS4780_SWITCH_SETTINGS, switch_enable);
	usleep_range(50, 55);

	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);
	dev_dbg(priv->dev,
		"%s: switch_control = %x switch_enable = %x 0x06 = %x 0x07 = %x\n",
		__func__, switch_control, switch_enable, reg6, reg7);
}

/*
 * WAS4780_unreg_notifier - unregister a notifier block with WAS4780
 * @nb:   notifier block
 * @node: phandle node to the WAS4780 device
 */
int WAS4780_unreg_notifier(struct notifier_block *nb,
			   struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct was4780_priv *priv;
	union power_supply_propval val = { 0 };
	int rc = -EINVAL;

	if (!client)
		return -EINVAL;

	priv = i2c_get_clientdata(client);
	if (!priv || !priv->dev)
		return -EINVAL;

	dev_dbg(priv->dev, "%s\n", __func__);

	mutex_lock(&priv->notification_lock);
	if (priv->usb_psy) {
		rc = power_supply_get_property(priv->usb_psy,
				(enum power_supply_property)WAS4780_PROP_TYPEC_MODE,
				&val);
		if (rc)
			goto unlock;
		if (val.intval == WAS4780_TYPEC_MODE_AUDIO)
			WAS4780_usbc_update_settings(priv, 0x18, 0x98);
	}
	rc = blocking_notifier_chain_unregister(&priv->was4780_notifier, nb);
unlock:
	mutex_unlock(&priv->notification_lock);

	return rc;
}
EXPORT_SYMBOL(WAS4780_unreg_notifier);

static int WAS4780_validate_display_port_settings(struct was4780_priv *priv)
{
	u32 switch_status = 0;

	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &switch_status);

	if (switch_status != 0x1c && switch_status != 0x23) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
		       switch_status);
		return -EIO;
	}

	return 0;
}

/*
 * WAS4780_switch_event - configure the switch position based on an event
 * @node:  phandle node to the WAS4780 device
 * @event: enum was4780_function
 */
int WAS4780_switch_event(struct device_node *node,
			 enum was4780_function event)
{
	struct i2c_client *client;
	struct was4780_priv *priv;
	u32 reg5 = 0;
	u32 switch_control = 0;

	client = of_find_i2c_device_by_node(node);
	if (!client)
		return -EINVAL;

	priv = i2c_get_clientdata(client);
	if (!priv || !priv->regmap)
		return -EINVAL;

	switch (event) {
	case WAS4780_MIC_GND_SWAP:
		regmap_read(priv->regmap, WAS4780_SWITCH_CONTROL, &reg5);
		switch_control = ((reg5 & 0x7) == 0x7) ? 0x0 : 0x7;
		break;
	case WAS4780_USBC_ORIENTATION_CC1:
		WAS4780_usbc_update_settings(priv, 0x18, 0xf8);
		return WAS4780_validate_display_port_settings(priv);
	case WAS4780_USBC_ORIENTATION_CC2:
		WAS4780_usbc_update_settings(priv, 0x78, 0xf8);
		return WAS4780_validate_display_port_settings(priv);
	case WAS4780_USBC_DISPLAYPORT_DISCONNECTED:
		WAS4780_usbc_update_settings(priv, 0x18, 0x98);
		return 0;
	default:
		dev_dbg(priv->dev, "%s: unknown event %d\n", __func__, event);
		return 0;
	}

	WAS4780_usbc_update_settings(priv, switch_control, 0x9f);

	return 0;
}
EXPORT_SYMBOL(WAS4780_switch_event);

/*
 * WAS4780_reg_notifier - register a notifier block with the WAS4780 driver
 * @nb:   notifier block
 * @node: phandle node to the WAS4780 device
 *
 * On success the current analog mode is applied so the caller sees the
 * switches in the right state right away.
 */
int WAS4780_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct was4780_priv *priv;
	int rc;

	if (!client)
		return -EINVAL;

	priv = i2c_get_clientdata(client);
	if (!priv)
		return -EINVAL;

	dev_dbg(priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);

	rc = blocking_notifier_chain_register(&priv->was4780_notifier, nb);
	if (rc)
		return rc;

	rc = WAS4780_usbc_analog_setup_switches(priv);
	if (rc) {
		blocking_notifier_chain_unregister(&priv->was4780_notifier, nb);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(WAS4780_reg_notifier);

static int WAS4780_usbc_analog_setup_switches(struct was4780_priv *priv)
{
	union power_supply_propval val = { 0 };
	int rc = 0;

	if (!priv || !priv->dev)
		return -EINVAL;

	mutex_lock(&priv->notification_lock);

	if (!priv->usb_psy) {
		rc = -ENODEV;
		goto unlock;
	}

	rc = power_supply_get_property(priv->usb_psy,
			(enum power_supply_property)WAS4780_PROP_TYPEC_MODE, &val);
	if (rc) {
		dev_err(priv->dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto unlock;
	}

	if (val.intval == WAS4780_TYPEC_MODE_NONE) {
		blocking_notifier_call_chain(&priv->was4780_notifier,
					     WAS4780_TYPEC_MODE_NONE, NULL);
		WAS4780_usbc_update_settings(priv, 0x18, 0x98);
	} else if (val.intval == WAS4780_TYPEC_MODE_AUDIO) {
		WAS4780_usbc_update_settings(priv, 0x00, 0x9f);
		blocking_notifier_call_chain(&priv->was4780_notifier,
					     val.intval, NULL);
	}

unlock:
	mutex_unlock(&priv->notification_lock);
	return rc;
}

static void WAS4780_usbc_analog_work_fn(struct work_struct *work)
{
	struct was4780_priv *priv =
		container_of(work, struct was4780_priv, usbc_analog_work);

	if (!priv) {
		pr_err("%s: invalid container\n", __func__);
		return;
	}

	WAS4780_usbc_analog_setup_switches(priv);
	pm_relax(priv->dev);
}

static int WAS4780_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct was4780_priv *priv;
	u32 reg6 = 0, reg7 = 0;
	int rc;

	priv = devm_kmalloc(&client->dev, sizeof(*priv),
			    GFP_KERNEL | __GFP_ZERO);
	if (!priv)
		return -ENOMEM;

	was4780_priv1 = priv;
	priv->dev = &client->dev;

	priv->regmap = devm_regmap_init_i2c(client, &WAS4780_regmap_config);
	if (IS_ERR_OR_NULL(priv->regmap)) {
		rc = priv->regmap ? PTR_ERR(priv->regmap) : -EINVAL;
		dev_err(priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		power_supply_put(priv->usb_psy);
		devm_kfree(&client->dev, priv);
		was4780_priv1 = NULL;
		return rc;
	}

	mutex_init(&priv->notification_lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&priv->was4780_notifier);
	INIT_WORK(&priv->usbc_analog_work, WAS4780_usbc_analog_work_fn);
	i2c_set_clientdata(client, priv);

	/* Best effort: the "usb" supply reports the Type-C analog mode. */
	priv->usb_psy = power_supply_get_by_name("usb");
	if (!priv->usb_psy)
		dev_dbg(priv->dev, "%s: no \"usb\" power supply\n", __func__);

	/* Soft reset, then load the analog-switch defaults. */
	regmap_write(priv->regmap, WAS4780_RESET, 0x01);
	usleep_range(260, 300);
	WAS4780_update_reg_defaults(priv->regmap);

	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS1, &reg6);
	regmap_read(priv->regmap, WAS4780_SWITCH_STATUS2, &reg7);
	dev_info(priv->dev, "%s, get reg 0x06(0x%02x), 0x07(0x%02x)\n",
		 __func__, reg6, reg7);

	return 0;
}

static int WAS4780_remove(struct i2c_client *client)
{
	struct was4780_priv *priv = i2c_get_clientdata(client);

	if (!priv)
		return -EINVAL;

	WAS4780_usbc_update_settings(priv, 0x18, 0x98);
	cancel_work_sync(&priv->usbc_analog_work);
	pm_relax(priv->dev);
	power_supply_unreg_notifier(&priv->psy_nb);
	power_supply_put(priv->usb_psy);
	mutex_destroy(&priv->notification_lock);
	i2c_set_clientdata(client, NULL);
	was4780_priv1 = NULL;

	return 0;
}

static const struct of_device_id WAS4780_i2c_dt_match[] = {
	{ .compatible = "sprd,WAS4780-i2c" },
	{ }
};

static struct i2c_driver WAS4780_i2c_driver = {
	.driver = {
		.name = WAS4780_I2C_NAME,
		.of_match_table = WAS4780_i2c_dt_match,
	},
	.probe = WAS4780_probe,
	.remove = WAS4780_remove,
};

static int __init WAS4780_init(void)
{
	int rc;

	rc = i2c_register_driver(THIS_MODULE, &WAS4780_i2c_driver);
	if (rc)
		pr_err("WAS4780: Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(WAS4780_init);

static void __exit WAS4780_exit(void)
{
	i2c_del_driver(&WAS4780_i2c_driver);
}
module_exit(WAS4780_exit);

MODULE_DESCRIPTION("WAS4780 I2C driver");
MODULE_LICENSE("GPL v2");
