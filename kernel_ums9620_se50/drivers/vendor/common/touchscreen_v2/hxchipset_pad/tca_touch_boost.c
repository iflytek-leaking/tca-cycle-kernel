// SPDX-License-Identifier: GPL-2.0
/*
 * tca_touch_boost.c - TCA Cycle Kernel touch / scheduling knobs (opt-in, default off)
 *
 * Modelled on Uperf's scene model (touch -> raise the foreground's effective
 * capacity + frequency for the touch duration), but done in-kernel:
 *
 *   touch_boost_enable / touch_boost_freq / touch_boost_ms
 *       cpufreq FREQ_QOS_MIN on the big cluster for touch_boost_ms.
 *
 *   touch_idle_latency_us (0 = off, default)
 *       Hold a cpu_dma_latency QoS for the touch duration so the IRQ core
 *       stays out of deep C-states (cuts touch IRQ wakeup jitter).
 *
 *   touch_irq_affinity / touch_irq_cpu
 *       Pin the touch IRQ to one big core (threaded IRQ is already SCHED_FIFO).
 *
 *   touch_uclamp_enable / touch_uclamp_pct / touch_uclamp_path
 *       Uperf's "touch scene" on the kernel side: for the touch duration raise
 *       the cpuctl cgroup's uclamp.min (percent, see kernel/sched/core.c
 *       cpu_uclamp_min_write), so EAS picks bigger cores and schedutil lifts
 *       frequency via the Unisoc uclamp_transform_boost() path.  The file is
 *       written with filp_open()/kernel_read()/kernel_write() (all exported);
 *       cgroup_get_from_fd() cannot be used because this device's cpuctl is a
 *       v1 hierarchy.  Default path: /dev/cpuctl/top-app/uclamp.min
 *
 * Everything defaults to off, so the shipped kernel behaves exactly like
 * before until a knob is flipped.  No struct layout is touched.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "tca_touch_boost.h"

#define TCA_BOOST_BIG_CPU	4
#define TCA_BOOST_DEF_FREQ_KHZ	1500000U
#define TCA_BOOST_DEF_MS	150U
#define TCA_BOOST_MIN_FREQ_KHZ	300000U
#define TCA_BOOST_MAX_FREQ_KHZ	3000000U
#define TCA_BOOST_MIN_MS	10U
#define TCA_BOOST_MAX_MS	2000U
#define TCA_UCLAMP_DEF_PCT	50U
#define TCA_UCLAMP_PATH_LEN	160U
#define TCA_UCLAMP_RETRY_MS	1000U

/* Serializes hits, expiry, sysfs and managed teardown, including file I/O. */
static DEFINE_MUTEX(tca_lock);
static struct device *tca_owner;
static bool tca_stopping;
static bool tca_expiry_pending;
static unsigned long tca_expires;

static bool tca_boost_enabled;		/* default off */
static unsigned int tca_boost_freq = TCA_BOOST_DEF_FREQ_KHZ;
static unsigned int tca_boost_ms = TCA_BOOST_DEF_MS;

static struct cpufreq_policy *tca_policy;
static struct freq_qos_request tca_qos;
static bool tca_qos_added;

static bool tca_uclamp_enabled;		/* default off */
static unsigned int tca_uclamp_pct = TCA_UCLAMP_DEF_PCT;
static char tca_uclamp_path[TCA_UCLAMP_PATH_LEN] = "/dev/cpuctl/top-app/uclamp.min";
static bool tca_uclamp_active;
static char tca_uclamp_saved[24];

static bool tca_irq_affinity_enabled;	/* default off */
static unsigned int tca_irq_cpu = 7;
static int tca_irq = -1;

static unsigned int tca_idle_latency_us;	/* 0 = off (default) */
static struct pm_qos_request tca_idle_qos;
static bool tca_idle_qos_added;
static bool tca_idle_active;

static int tca_uclamp_write(const char *val)
{
	struct file *f;
	loff_t pos = 0;
	ssize_t n;

	f = filp_open(tca_uclamp_path, O_WRONLY, 0);
	if (IS_ERR(f))
		return (int)PTR_ERR(f);
	n = kernel_write(f, val, strlen(val), &pos);
	filp_close(f, NULL);
	return n < 0 ? (int)n : (n == strlen(val) ? 0 : -EIO);
}

static int tca_uclamp_read(char *buf, size_t len)
{
	struct file *f;
	loff_t pos = 0;
	ssize_t n;

	if (len < 2)
		return -EINVAL;
	f = filp_open(tca_uclamp_path, O_RDONLY, 0);
	if (IS_ERR(f))
		return (int)PTR_ERR(f);
	n = kernel_read(f, buf, len - 1, &pos);
	filp_close(f, NULL);
	if (n <= 0)
		return n < 0 ? (int)n : -EIO;
	buf[n] = '\0';
	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
		buf[--n] = '\0';
	return 0;
}

static void tca_unboost_work_fn(struct work_struct *work);
static DECLARE_DELAYED_WORK(tca_unboost_work, tca_unboost_work_fn);

/* All helpers below require tca_lock. */
static void tca_freq_release(void)
{
	if (tca_qos_added)
		freq_qos_update_request(&tca_qos, FREQ_QOS_MIN_DEFAULT_VALUE);
}

static int tca_uclamp_release(void)
{
	int ret = 0;

	if (tca_uclamp_active) {
		ret = tca_uclamp_write(tca_uclamp_saved);
		if (!ret)
			tca_uclamp_active = false;
	}
	return ret;
}

static void tca_idle_release(void)
{
	if (tca_idle_active) {
		pm_qos_update_request(&tca_idle_qos, PM_QOS_DEFAULT_VALUE);
		tca_idle_active = false;
	}
}

static void tca_release_all(void)
{
	tca_freq_release();
	tca_uclamp_release();
	tca_idle_release();
}

/* Never cancel another enabled knob's expiry on an individual off write. */
static void tca_off_update(void)
{
	if (!tca_boost_enabled && !tca_uclamp_enabled &&
	    !tca_idle_latency_us) {
		tca_expiry_pending = false;
		tca_release_all();
		/* Non-sync is safe under the lock; a running worker checks state. */
		cancel_delayed_work(&tca_unboost_work);
	}
	/* Keep the saved value/path and retry transient restoration failures. */
	if (tca_uclamp_active && !tca_uclamp_enabled)
		schedule_delayed_work(&tca_unboost_work,
				      msecs_to_jiffies(TCA_UCLAMP_RETRY_MS));
}

static void tca_unboost_work_fn(struct work_struct *work)
{
	unsigned long now;

	mutex_lock(&tca_lock);
	if (tca_stopping)
		goto out;

	if (!tca_uclamp_enabled)
		tca_uclamp_release();
	/* A queued old worker may acquire the lock after a newer hit. */
	now = jiffies;
	if (tca_expiry_pending && time_before(now, tca_expires)) {
		mod_delayed_work(system_wq, &tca_unboost_work,
				 tca_expires - now);
		goto out;
	}
	tca_expiry_pending = false;
	tca_release_all();
	if (tca_uclamp_active)
		mod_delayed_work(system_wq, &tca_unboost_work,
				 msecs_to_jiffies(TCA_UCLAMP_RETRY_MS));
out:
	mutex_unlock(&tca_lock);
}

/*
 * The caller removes sysfs and stops IRQ/poll work before devres teardown.
 * Register lazily from sysfs before enabling anything: no caller ABI change
 * or extra module init/exit is needed for this composite driver.
 */
static void tca_touch_cleanup(void *data)
{
	struct device *dev = data;

	mutex_lock(&tca_lock);
	tca_stopping = true;
	tca_boost_enabled = false;
	tca_uclamp_enabled = false;
	tca_idle_latency_us = 0;
	tca_irq_affinity_enabled = false;
	tca_expiry_pending = false;
	mutex_unlock(&tca_lock);

	/* The worker takes tca_lock; do not hold it while draining work. */
	cancel_delayed_work_sync(&tca_unboost_work);

	mutex_lock(&tca_lock);
	tca_release_all();
	if (tca_uclamp_active)
		dev_warn(dev, "touch uclamp restoration failed during removal\n");
	if (tca_qos_added) {
		freq_qos_remove_request(&tca_qos);
		tca_qos_added = false;
	}
	if (tca_policy) {
		cpufreq_cpu_put(tca_policy);
		tca_policy = NULL;
	}
	if (tca_idle_qos_added) {
		pm_qos_remove_request(&tca_idle_qos);
		tca_idle_qos_added = false;
	}
	/* free_irq() has already cleared the affinity hint. */
	tca_irq = -1;
	tca_owner = NULL;
	mutex_unlock(&tca_lock);
}

static int tca_prepare(struct device *dev)
{
	int ret;

	if (tca_owner)
		return tca_owner == dev && !tca_stopping ? 0 : -EBUSY;
	/* Not _or_reset: its failure callback would recurse into tca_lock. */
	ret = devm_add_action(dev, tca_touch_cleanup, dev);
	if (ret)
		return ret;
	tca_owner = dev;
	tca_stopping = false;
	/* Retry any restoration that failed during a previous unbind. */
	if (tca_uclamp_active)
		schedule_delayed_work(&tca_unboost_work,
				      msecs_to_jiffies(TCA_UCLAMP_RETRY_MS));
	return 0;
}

/* Called by both the threaded IRQ and polling work (may sleep). */
void tca_touch_boost_hit(void)
{
	unsigned long delay;

	mutex_lock(&tca_lock);
	if (!tca_owner || tca_stopping)
		goto out;

	if (tca_boost_enabled) {
		if (!tca_policy) {
			tca_policy = cpufreq_cpu_get(TCA_BOOST_BIG_CPU);
			if (tca_policy &&
			    freq_qos_add_request(&tca_policy->constraints, &tca_qos,
						 FREQ_QOS_MIN,
						 FREQ_QOS_MIN_DEFAULT_VALUE) >= 0) {
				tca_qos_added = true;
			} else if (tca_policy) {
				cpufreq_cpu_put(tca_policy);
				tca_policy = NULL;
			}
		}
		if (tca_qos_added)
			freq_qos_update_request(&tca_qos, tca_boost_freq);
	}

	if (tca_uclamp_enabled && !tca_uclamp_active) {
		if (tca_uclamp_read(tca_uclamp_saved,
				    sizeof(tca_uclamp_saved)) == 0) {
			char req[8];

			snprintf(req, sizeof(req), "%u", tca_uclamp_pct);
			tca_uclamp_active = true;
			if (tca_uclamp_write(req))
				tca_uclamp_release();
		}
	}

	if (tca_idle_latency_us) {
		if (!tca_idle_qos_added) {
			/* pm_qos_add_request 返回 void，合法 class 不会失败 */
			pm_qos_add_request(&tca_idle_qos,
					   PM_QOS_CPU_DMA_LATENCY,
					   PM_QOS_DEFAULT_VALUE);
			tca_idle_qos_added = true;
		}
		if (tca_idle_qos_added) {
			pm_qos_update_request(&tca_idle_qos,
						       (s32)tca_idle_latency_us);
			tca_idle_active = true;
		}
	}

	if (tca_boost_enabled || tca_uclamp_enabled || tca_idle_latency_us) {
		delay = msecs_to_jiffies(tca_boost_ms);
		tca_expires = jiffies + delay;
		tca_expiry_pending = true;
		mod_delayed_work(system_wq, &tca_unboost_work, delay);
	}
out:
	mutex_unlock(&tca_lock);
}

/* Remember the touch IRQ so the affinity knob can act on it. */
void tca_touch_irq_set(int irq)
{
	mutex_lock(&tca_lock);
	if (irq < 0 && tca_irq >= 0)
		irq_set_affinity_hint(tca_irq, NULL);
	tca_irq = irq;
	if (tca_irq_affinity_enabled && irq >= 0 && cpu_online(tca_irq_cpu))
		irq_set_affinity_hint(irq, cpumask_of(tca_irq_cpu));
	mutex_unlock(&tca_lock);
}

static ssize_t touch_boost_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", tca_boost_enabled ? 1 : 0);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_boost_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool on = (buf[0] == '1');
	int ret;

	mutex_lock(&tca_lock);
	if (on) {
		ret = tca_prepare(dev);
		if (ret) {
			mutex_unlock(&tca_lock);
			return ret;
		}
	}
	tca_boost_enabled = on;
	if (!on) {
		tca_freq_release();
		tca_off_update();
	}
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_boost_enable);

static ssize_t touch_boost_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tca_boost_freq);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_boost_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int v;

	if (kstrtouint(buf, 10, &v))
		return -EINVAL;
	v = clamp(v, TCA_BOOST_MIN_FREQ_KHZ, TCA_BOOST_MAX_FREQ_KHZ);
	mutex_lock(&tca_lock);
	tca_boost_freq = v;
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_boost_freq);

static ssize_t touch_boost_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tca_boost_ms);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_boost_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int v;

	if (kstrtouint(buf, 10, &v))
		return -EINVAL;
	v = clamp(v, TCA_BOOST_MIN_MS, TCA_BOOST_MAX_MS);
	mutex_lock(&tca_lock);
	tca_boost_ms = v;
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_boost_ms);

static ssize_t touch_uclamp_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", tca_uclamp_enabled ? 1 : 0);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_uclamp_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool on = (buf[0] == '1');
	int ret;

	mutex_lock(&tca_lock);
	if (on) {
		ret = tca_prepare(dev);
		if (ret) {
			mutex_unlock(&tca_lock);
			return ret;
		}
	}
	tca_uclamp_enabled = on;
	if (!on) {
		tca_uclamp_release();
		tca_off_update();
	}
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_uclamp_enable);

static ssize_t touch_uclamp_pct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tca_uclamp_pct);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_uclamp_pct_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int v;

	if (kstrtouint(buf, 10, &v))
		return -EINVAL;
	v = clamp(v, 1U, 100U);
	mutex_lock(&tca_lock);
	tca_uclamp_pct = v;
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_uclamp_pct);

static ssize_t touch_uclamp_path_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%s\n", tca_uclamp_path);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_uclamp_path_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	size_t n = count;
	int ret;

	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
		n--;
	if (n == 0 || n >= TCA_UCLAMP_PATH_LEN)
		return -EINVAL;
	mutex_lock(&tca_lock);
	ret = tca_uclamp_release();
	if (ret) {
		/* Do not lose the old path/value while it still needs restoring. */
		mutex_unlock(&tca_lock);
		return ret;
	}
	memcpy(tca_uclamp_path, buf, n);
	tca_uclamp_path[n] = '\0';
	tca_uclamp_active = false;
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_uclamp_path);

static ssize_t touch_irq_affinity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			 tca_irq_affinity_enabled ? 1 : 0);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_irq_affinity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool on = (buf[0] == '1');
	int ret;

	mutex_lock(&tca_lock);
	if (on) {
		ret = tca_prepare(dev);
		if (ret) {
			mutex_unlock(&tca_lock);
			return ret;
		}
	}
	tca_irq_affinity_enabled = on;
	if (tca_irq >= 0) {
		if (on && cpu_online(tca_irq_cpu))
			irq_set_affinity_hint(tca_irq, cpumask_of(tca_irq_cpu));
		else
			irq_set_affinity_hint(tca_irq, NULL);
	}
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_irq_affinity);

static ssize_t touch_irq_cpu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tca_irq_cpu);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_irq_cpu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int v;

	if (kstrtouint(buf, 10, &v) || v >= nr_cpu_ids)
		return -EINVAL;
	mutex_lock(&tca_lock);
	tca_irq_cpu = v;
	if (tca_irq_affinity_enabled && tca_irq >= 0 && cpu_online(v))
		irq_set_affinity_hint(tca_irq, cpumask_of(v));
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_irq_cpu);

static ssize_t touch_idle_latency_us_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&tca_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tca_idle_latency_us);
	mutex_unlock(&tca_lock);
	return ret;
}

static ssize_t touch_idle_latency_us_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int v;
	int ret;

	if (kstrtouint(buf, 10, &v) || v > 1000)
		return -EINVAL;
	mutex_lock(&tca_lock);
	if (v) {
		ret = tca_prepare(dev);
		if (ret) {
			mutex_unlock(&tca_lock);
			return ret;
		}
	}
	tca_idle_latency_us = v;
	if (!v) {
		tca_idle_release();
		tca_off_update();
	}
	mutex_unlock(&tca_lock);
	return count;
}
static DEVICE_ATTR_RW(touch_idle_latency_us);

static struct attribute *tca_touch_attrs[] = {
	&dev_attr_touch_boost_enable.attr,
	&dev_attr_touch_boost_freq.attr,
	&dev_attr_touch_boost_ms.attr,
	&dev_attr_touch_uclamp_enable.attr,
	&dev_attr_touch_uclamp_pct.attr,
	&dev_attr_touch_uclamp_path.attr,
	&dev_attr_touch_irq_affinity.attr,
	&dev_attr_touch_irq_cpu.attr,
	&dev_attr_touch_idle_latency_us.attr,
	NULL,
};
const struct attribute_group tca_touch_boost_attr_group = {
	.attrs = tca_touch_attrs,
};
