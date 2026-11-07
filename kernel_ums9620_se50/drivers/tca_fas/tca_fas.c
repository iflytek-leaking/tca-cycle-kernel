// SPDX-License-Identifier: GPL-2.0
/*
 * tca_fas.c - TCA Cycle Kernel in-kernel Frame Aware Scheduling (FAS) v2
 *
 * fas-rs 在用户态用 eBPF uprobe 钩 doFrame 读"帧生成耗时"；本驱动的
 * v2 使用内核接口采集两路启发式信号，并不等价于应用帧耗时：
 *
 *   节奏信号  : drm_vblank_event tracepoint（启动期查找），probe 记录
 *               时间戳并唤醒 work。vblank 是显示扫描节奏，不是应用提交/
 *               呈现帧率；多个 CRTC 的事件也可能混合。interval/janks
 *               仅描述这些事件相对预算的偏差，不能证明应用掉帧。
 *   负载信号  : 每帧窗口内各簇 busy 时间增量（kcpustat，TICK 记账
 *               4ms 粒度、EMA 平滑后可用）× cpufreq_quick_get 实测频率
 *               = 采样窗口内的簇级负载估计，包含非渲染任务。
 *
 * 只看帧间隔分不清"内容本来就是 30fps"和"拼命跑也只能 30fps"；
 * freq×busy 是负载归一化近似，受记账粒度、访存等待和采样频率影响，
 * 不保证频率不变性，也不能据此区分应用瓶颈。
 *
 * 控制律（帧驱动；实际改频在 workqueue 进程上下文）：
 *
 *   work    = f_now × busy_ratio
 *   need    = work × headroom_pct/100        // 刚好够用的频率
 *   jank    = interval > budget + margin     // 截止期护栏
 *   need   := max(need, ceiling + late_ms × gain_khz_per_ms)
 *   ceiling = EMA(need) → clamp → 25MHz 量化
 *   按簇量程的同一相对位置换算下发到所有簇（中大核曲线重合），
 *   执行器是 FREQ_QOS_MAX 天花板：performance 调速器下大核不再钉死
 *   最高频，而是沿天花板带状波动（FEAS 指纹）；schedutil 下是封顶。
 *
 * 预算自适应：慢 EMA 学帧周期，吸附到常见刷新档（24/25/30/40/45/48/50/
 * 60/72/90/96/120/144），连续 10 帧漂移才换档（防抖）；adaptive=0 退化为
 * 固定 target_fps 预算（不保证应用帧率）。
 *
 * 平衡点：ceiling 收敛到 busy ≈ 100/headroom_pct（默认 125 → ~80% 占用）
 * 的带状区间 —— 这就是 FEAS 的"不针状"波动形态。
 *
 * 机制内聚内核，策略全在 Magisk 模块（tca-cycle-tuner）：
 * /sys/kernel/tca_fas/ 暴露全部旋钮与实时状态。默认关闭；无 vblank
 * 250ms 自动释放全部约束（= 原生行为）。
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/tracepoint.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>

#define TCA_FAS_MAX_POL		4
#define TCA_FAS_TICK_MS		50	/* housekeeping（idle 检测） */
#define TCA_FAS_IDLE_MS		250
#define TCA_FAS_DEF_FPS		60U
#define TCA_FAS_DEF_MARGIN_US	1500U
#define TCA_FAS_DEF_HEADROOM	125U
#define TCA_FAS_DEF_GAIN_KHZ	40000U
#define TCA_FAS_DEF_SMOOTH	3U	/* EMA α = 1/2^N */
#define TCA_FAS_DEF_MIN_PCT	30U	/* 天花板下限 = 簇量程的 % */
#define TCA_FAS_DEF_MIN_KHZ	0U	/* 可选地板（MIN），0 = 关 */
#define TCA_FAS_DEF_MAX_KHZ	0U	/* 天花板上限，0 = 跟硬件 */
#define TCA_FAS_MAX_GAP_NS	200000000ULL
#define TCA_FAS_MIN_WALL_NS	1000000ULL
#define TCA_FAS_DRIFT_FRAMES	10U
#define TCA_FAS_QUANT_KHZ	25000U

/* ---- 旋钮（sysfs 可调，模块侧策略的落点） ---- */
static bool fas_enable;
static bool fas_adaptive = true;
static unsigned int fas_target_fps = TCA_FAS_DEF_FPS;
static unsigned int fas_margin_us = TCA_FAS_DEF_MARGIN_US;
static unsigned int fas_headroom = TCA_FAS_DEF_HEADROOM;
static unsigned int fas_gain_khz = TCA_FAS_DEF_GAIN_KHZ;
static unsigned int fas_smooth = TCA_FAS_DEF_SMOOTH;
static unsigned int fas_min_pct = TCA_FAS_DEF_MIN_PCT;
static unsigned int fas_min_khz = TCA_FAS_DEF_MIN_KHZ;
static unsigned int fas_max_khz = TCA_FAS_DEF_MAX_KHZ;

/*
 * Lock order: fas_control_lock -> fas_state_lock -> fas_lock.
 * The worker never takes fas_control_lock. Drop fas_state_lock before
 * cancel_delayed_work_sync(); no sleeping operations under fas_lock.
 */
static DEFINE_MUTEX(fas_control_lock);
static DEFINE_MUTEX(fas_state_lock);

/* ---- probe → work 共享态（fas_lock 保护） ---- */
static DEFINE_SPINLOCK(fas_lock);
static u64 fas_frames;
static u64 fas_last_vblank_ns;
static unsigned long fas_activity;

/* ---- 控制器内部态与旋钮（fas_state_lock 保护） ---- */
static u64 fas_janks;
static u64 fas_frames_done;
static u64 fas_wall_prev;
static u64 fas_rate_ns;		/* 学到的帧周期（慢 EMA） */
static unsigned int fas_drift;
static unsigned int fas_ceiling;	/* 大簇天花板（平滑后 kHz） */
static unsigned int fas_applied;	/* 量化后实际下发的大簇天花板 */
static unsigned int fas_applied_little;
static unsigned int fas_floor_applied;

/* ---- 只读统计 ---- */
static u64 fas_interval_ns;
static u64 fas_budget_ns;
static unsigned int fas_bin_fps;
static unsigned int fas_busy_pct;
static unsigned int fas_need_khz;

struct fas_cluster {
	struct cpufreq_policy *pol;
	struct freq_qos_request qmax;	/* FEAS 天花板 */
	struct freq_qos_request qmin;	/* 可选地板 */
	bool has_max, has_min;
	u64 busy_prev;
	unsigned int hw_min, hw_max;
	int leader;
};
static struct fas_cluster fas_cl[TCA_FAS_MAX_POL];
static int fas_ncl;
static int fas_big = -1;

static struct tracepoint *fas_vblank_tp;
static bool fas_tp_registered;
static struct delayed_work fas_work;
static struct kobject *fas_kobj;

/* ---- 常见刷新率档位 ---- */
static const unsigned int fas_bins[] = {
	24, 25, 30, 40, 45, 48, 50, 60, 72, 90, 96, 120, 144
};

static unsigned int fas_snap_bin(u64 period_ns)
{
	unsigned int i, best = TCA_FAS_DEF_FPS;
	u64 best_d = ~0ULL;

	for (i = 0; i < ARRAY_SIZE(fas_bins); i++) {
		u64 p = 1000000000ULL / fas_bins[i];
		u64 d = p > period_ns ? p - period_ns : period_ns - p;

		if (d < best_d) {
			best_d = d;
			best = fas_bins[i];
		}
	}
	return best;
}

/* ---- 簇 busy 时间（kcpustat，ns；TICK 记账 4ms 粒度） ---- */
static u64 fas_cl_busy(struct fas_cluster *c)
{
	u64 busy = 0;
	int cpu;

	for_each_cpu(cpu, c->pol->related_cpus) {
		u64 *s = kcpustat_cpu(cpu).cpustat;

		busy += s[CPUTIME_USER] + s[CPUTIME_NICE] + s[CPUTIME_SYSTEM] +
			s[CPUTIME_IRQ] + s[CPUTIME_SOFTIRQ] + s[CPUTIME_STEAL];
	}
	return busy;
}

/* ---- 簇发现 + QoS 请求 ---- */
static void fas_policy_cleanup(void)
{
	int i;

	for (i = 0; i < fas_ncl; i++) {
		struct fas_cluster *c = &fas_cl[i];

		if (c->has_min)
			freq_qos_remove_request(&c->qmin);
		if (c->has_max)
			freq_qos_remove_request(&c->qmax);
		cpufreq_cpu_put(c->pol);
		memset(c, 0, sizeof(*c));
	}
	fas_ncl = 0;
	fas_big = -1;
}

static int fas_policy_init(void)
{
	int cpu, i, ret;

	if (fas_ncl)
		return 0;
	for_each_possible_cpu(cpu) {
		struct cpufreq_policy *p;
		struct fas_cluster *c;

		if (fas_ncl >= TCA_FAS_MAX_POL)
			break;
		p = cpufreq_cpu_get(cpu);
		if (!p)
			continue;
		for (i = 0; i < fas_ncl; i++)
			if (fas_cl[i].pol == p)
				break;
		if (i < fas_ncl) {
			cpufreq_cpu_put(p);
			continue;
		}
		c = &fas_cl[fas_ncl++];
		c->pol = p;
		c->hw_min = p->cpuinfo.min_freq;
		c->hw_max = p->cpuinfo.max_freq;
		if (!c->hw_min || c->hw_min > c->hw_max ||
		    c->hw_max > S32_MAX) {
			ret = -EINVAL;
			goto rollback;
		}
		c->leader = cpumask_first(p->related_cpus);
		c->busy_prev = fas_cl_busy(c);
		ret = freq_qos_add_request(&p->constraints, &c->qmax,
					   FREQ_QOS_MAX, c->hw_max);
		if (ret < 0)
			goto rollback;
		c->has_max = true;
		ret = freq_qos_add_request(&p->constraints, &c->qmin,
					   FREQ_QOS_MIN, 0);
		if (ret < 0)
			goto rollback;
		c->has_min = true;
	}
	if (!fas_ncl)
		return 0;
	fas_big = 0;
	for (i = 1; i < fas_ncl; i++)
		if (fas_cl[i].hw_max > fas_cl[fas_big].hw_max)
			fas_big = i;
	pr_info("tca_fas: %d clusters, big = cpu%d (%u-%u kHz)\n", fas_ncl,
		fas_cl[fas_big].leader, fas_cl[fas_big].hw_min,
		fas_cl[fas_big].hw_max);
	return 0;

rollback:
	fas_policy_cleanup();
	return ret;
}

/* 把大簇绝对频率换算成量程千分比，同一相对位置套到所有簇（曲线重合） */
static void fas_apply(unsigned int big_khz)
{
	struct fas_cluster *b = &fas_cl[fas_big];
	u64 range = b->hw_max - b->hw_min;
	u64 pos = 0;
	int i;

	big_khz = clamp(big_khz, b->hw_min, b->hw_max);
	if (range)
		pos = (u64)(big_khz - b->hw_min) * 1000 / range;
	for (i = 0; i < fas_ncl; i++) {
		struct fas_cluster *c = &fas_cl[i];
		u64 cr = c->hw_max - c->hw_min;
		unsigned int v = i == fas_big ? big_khz :
			c->hw_min + (unsigned int)(cr * pos / 1000);

		if (c->has_max)
			freq_qos_update_request(&c->qmax, v);
		if (i == fas_big)
			fas_applied = v;
		else
			fas_applied_little = v;
	}
}

/* 可选地板（min_khz，默认 0 = 无约束） */
static void fas_apply_floor(unsigned int big_khz)
{
	struct fas_cluster *b = &fas_cl[fas_big];
	u64 range = b->hw_max - b->hw_min;
	u64 pos = 0;
	bool active = big_khz != 0;
	int i;

	big_khz = clamp(big_khz, b->hw_min, b->hw_max);
	if (range && active)
		pos = (u64)(big_khz - b->hw_min) * 1000 / range;
	for (i = 0; i < fas_ncl; i++) {
		struct fas_cluster *c = &fas_cl[i];
		u64 cr = c->hw_max - c->hw_min;
		unsigned int v = active ? (i == fas_big ? big_khz :
			c->hw_min + (unsigned int)(cr * pos / 1000)) : 0;

		if (c->has_min)
			freq_qos_update_request(&c->qmin, v);
	}
}

/* 释放全部约束（= 原生行为） */
static void fas_release(void)
{
	int i;

	for (i = 0; i < fas_ncl; i++) {
		if (fas_cl[i].has_max)
			freq_qos_update_request(&fas_cl[i].qmax, fas_cl[i].hw_max);
		if (fas_cl[i].has_min)
			freq_qos_update_request(&fas_cl[i].qmin, 0);
	}
	fas_applied = 0;
	fas_applied_little = 0;
	fas_floor_applied = 0;
	fas_ceiling = 0;
}

/* ---- vblank probe：只记时间戳 + 唤醒 work，微秒级 ---- */
static void tca_fas_vblank_probe(void *data, int crtc, unsigned int seq)
{
	unsigned long flags;

	spin_lock_irqsave(&fas_lock, flags);
	if (fas_enable) {
		fas_frames++;
		fas_last_vblank_ns = ktime_get_ns();
		fas_activity = jiffies;
		/* Enqueue before disable can close the gate and cancel. */
		mod_delayed_work(system_wq, &fas_work, 0);
	}
	spin_unlock_irqrestore(&fas_lock, flags);
}

/* ---- 控制器主循环（帧驱动，进程上下文） ---- */
static void tca_fas_work_fn(struct work_struct *work)
{
	u64 frames, vblank_ns, wall, interval, budget, margin, d;
	u64 busy_now, busy_delta, br;
	unsigned long flags, act;
	struct fas_cluster *b;
	unsigned int f_now, need, lo, hi, q, ncpu;

	mutex_lock(&fas_state_lock);
	if (!fas_enable)
		goto unlock;
	if (!fas_ncl)
		fas_policy_init();	/* cpufreq 晚就绪则重试 */
	if (!fas_ncl)
		goto resched;

	spin_lock_irqsave(&fas_lock, flags);
	frames = fas_frames;
	vblank_ns = fas_last_vblank_ns;
	act = fas_activity;
	spin_unlock_irqrestore(&fas_lock, flags);

	/* 无 vblank 250ms → 信号超时，释放约束，停 tick（下次事件唤醒） */
	if (time_after(jiffies, act + msecs_to_jiffies(TCA_FAS_IDLE_MS))) {
		if (fas_applied || fas_floor_applied)
			fas_release();
		fas_frames_done = frames;
		fas_wall_prev = 0;
		goto unlock;
	}

	d = frames - fas_frames_done;
	if (!d)
		goto resched;		/* housekeeping tick */

	b = &fas_cl[fas_big];
	if (!fas_wall_prev || vblank_ns <= fas_wall_prev) {
		b->busy_prev = fas_cl_busy(b);
		fas_wall_prev = vblank_ns;
		goto done_frame;
	}
	wall = vblank_ns - fas_wall_prev;
	fas_wall_prev = vblank_ns;
	if (wall < TCA_FAS_MIN_WALL_NS || wall > TCA_FAS_MAX_GAP_NS) {
		b->busy_prev = fas_cl_busy(b);
		goto done_frame;
	}
	interval = d > 1 ? div64_u64(wall, d) : wall;
	fas_interval_ns = interval;

	/* --- 帧率学习：慢 EMA，持续漂移才换档 --- */
	if (!fas_rate_ns) {
		fas_rate_ns = interval;
	} else {
		u64 diff = interval > fas_rate_ns ? interval - fas_rate_ns
						  : fas_rate_ns - interval;

		if (diff > fas_rate_ns / 4) {
			if (++fas_drift >= TCA_FAS_DRIFT_FRAMES) {
				fas_rate_ns = interval;
				fas_drift = 0;
			}
		} else {
			fas_drift = 0;
			fas_rate_ns = (fas_rate_ns * 15 + interval) / 16;
		}
	}

	if (fas_adaptive) {
		fas_bin_fps = fas_snap_bin(fas_rate_ns);
		budget = 1000000000ULL / fas_bin_fps;
	} else {
		fas_bin_fps = fas_target_fps;
		budget = 1000000000ULL / (fas_target_fps ? fas_target_fps : 60);
	}
	fas_budget_ns = budget;
	margin = (u64)fas_margin_us * 1000ULL;

	/* --- 负载信号：大簇 busy × 采样频率，窗口工作量的近似 --- */
	b = &fas_cl[fas_big];
	busy_now = fas_cl_busy(b);
	busy_delta = busy_now >= b->busy_prev ? busy_now - b->busy_prev : 0;
	b->busy_prev = busy_now;
	ncpu = cpumask_weight(b->pol->related_cpus);
	if (!ncpu)
		ncpu = 1;
	br = div64_u64(busy_delta * 1024, wall * (u64)ncpu);	/* 0..1024 */
	if (br > 1024)
		br = 1024;
	fas_busy_pct = (unsigned int)(br * 100 / 1024);

	f_now = cpufreq_quick_get(b->leader);
	if (!f_now)
		f_now = b->pol->cur;
	if (!f_now)
		f_now = fas_applied ? fas_applied : b->hw_max;

	need = (unsigned int)div64_u64((u64)f_now * br * fas_headroom,
				       1024 * 100ULL);

	/* --- 截止期护栏：晚了就比例快升 --- */
	if (interval > budget + margin) {
		u64 late_ms = div64_u64(interval - budget, 1000000ULL) + 1;
		unsigned int guard = fas_ceiling +
			(unsigned int)late_ms * fas_gain_khz;

		if (guard > need)
			need = guard;
		fas_janks += d;
	}
	fas_need_khz = need;

	/* --- 平滑 → 夹取 → 量化 → 下发 --- */
	if (!fas_ceiling)
		fas_ceiling = need;
	else
		fas_ceiling = ((u64)fas_ceiling * ((1U << fas_smooth) - 1) + need)
			      >> fas_smooth;

	lo = b->hw_min + (u64)(b->hw_max - b->hw_min) * fas_min_pct / 100;
	hi = fas_max_khz ? clamp(fas_max_khz, b->hw_min, b->hw_max) :
		b->hw_max;
	if (lo > hi)
		lo = hi;
	fas_ceiling = clamp(fas_ceiling, lo, hi);

	q = fas_ceiling / TCA_FAS_QUANT_KHZ * TCA_FAS_QUANT_KHZ;
	q = clamp(q, lo, hi);
	if (q != fas_applied)
		fas_apply(q);

	if (fas_min_khz != fas_floor_applied) {
		fas_apply_floor(fas_min_khz);
		fas_floor_applied = fas_min_khz;
	}

done_frame:
	fas_frames_done = frames;
resched:
	if (fas_enable)
		schedule_delayed_work(&fas_work,
				      msecs_to_jiffies(TCA_FAS_TICK_MS));
unlock:
	mutex_unlock(&fas_state_lock);
}

/* ---- tracepoint 查找/注册 ---- */
static void tca_fas_find_tp(struct tracepoint *tp, void *priv)
{
	if (!strcmp(tp->name, "drm_vblank_event"))
		fas_vblank_tp = tp;
}

static int tca_fas_register_tp(void)
{
	int ret;

	if (fas_tp_registered)
		return 0;
	if (!fas_vblank_tp)
		return -ENOENT;
	ret = tracepoint_probe_register(fas_vblank_tp, tca_fas_vblank_probe, NULL);
	if (!ret)
		fas_tp_registered = true;
	return ret;
}

/* ---- sysfs（机制全暴露，策略归 Magisk 模块） ---- */
static ssize_t enable_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_enable ? 1 : 0);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
/* Called with both mutexes held; the probe also observes this gate. */
static void fas_set_enable(bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&fas_lock, flags);
	fas_enable = enable;
	spin_unlock_irqrestore(&fas_lock, flags);
}

/* Return with fas_state_lock held, after all old work has drained. */
static void fas_quiesce(void)
{
	fas_set_enable(false);
	mutex_unlock(&fas_state_lock);
	cancel_delayed_work_sync(&fas_work);
	mutex_lock(&fas_state_lock);
}

/* Probe disabled, worker drained, fas_state_lock held. */
static void fas_reset_state(void)
{
	unsigned long flags;
	int i;

	fas_release();
	for (i = 0; i < fas_ncl; i++)
		fas_cl[i].busy_prev = fas_cl_busy(&fas_cl[i]);
	spin_lock_irqsave(&fas_lock, flags);
	fas_frames = 0;
	fas_last_vblank_ns = 0;
	fas_activity = jiffies;
	spin_unlock_irqrestore(&fas_lock, flags);
	fas_janks = 0;
	fas_frames_done = 0;
	fas_wall_prev = 0;
	fas_rate_ns = 0;
	fas_drift = 0;
	fas_interval_ns = 0;
	fas_budget_ns = 0;
	fas_bin_fps = 0;
	fas_busy_pct = 0;
	fas_need_khz = 0;
}

static ssize_t enable_store(struct kobject *k, struct kobj_attribute *a,
			    const char *buf, size_t n)
{
	bool on = buf[0] == '1' || buf[0] == 'y' || buf[0] == 'Y';
	int ret = 0;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	if (on == fas_enable)
		goto out;
	if (on) {
		ret = fas_policy_init();
		if (ret < 0)
			goto out;
		fas_reset_state();
		tca_fas_register_tp();
		fas_set_enable(true);
		schedule_delayed_work(&fas_work, 0);
		pr_info("tca_fas: enabled (%s)\n",
			fas_tp_registered ? "vblank driven" : "tracepoint missing, idle");
	} else {
		fas_quiesce();
		fas_release();
		pr_info("tca_fas: disabled, constraints released\n");
	}
out:
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret < 0 ? ret : n;
}
static struct kobj_attribute enable_attr = __ATTR_RW(enable);

static ssize_t adaptive_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_adaptive ? 1 : 0);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static ssize_t adaptive_store(struct kobject *k, struct kobj_attribute *a,
			      const char *buf, size_t n)
{
	mutex_lock(&fas_state_lock);
	fas_adaptive = buf[0] == '1' || buf[0] == 'y' || buf[0] == 'Y';
	mutex_unlock(&fas_state_lock);
	return n;
}
static struct kobj_attribute adaptive_attr = __ATTR_RW(adaptive);

#define FAS_UINT_ATTR(_name, _var, _min, _max)				\
static ssize_t _name##_show(struct kobject *k, struct kobj_attribute *a,\
			    char *b)						\
{									\
	ssize_t ret;							\
	mutex_lock(&fas_state_lock);					\
	ret = scnprintf(b, PAGE_SIZE, "%u\n", _var);			\
	mutex_unlock(&fas_state_lock);					\
	return ret;							\
}									\
static ssize_t _name##_store(struct kobject *k, struct kobj_attribute *a,\
			     const char *buf, size_t n)				\
{									\
	unsigned int v;							\
	if (kstrtouint(buf, 0, &v))					\
		return -EINVAL;						\
	mutex_lock(&fas_state_lock);					\
	_var = clamp(v, (unsigned int)_min, (unsigned int)_max);		\
	mutex_unlock(&fas_state_lock);					\
	return n;							\
}									\
static struct kobj_attribute _name##_attr = __ATTR_RW(_name)

FAS_UINT_ATTR(target_fps, fas_target_fps, 10, 240);
FAS_UINT_ATTR(margin_us, fas_margin_us, 0, 20000);
FAS_UINT_ATTR(headroom_pct, fas_headroom, 100, 300);
FAS_UINT_ATTR(gain_khz_per_ms, fas_gain_khz, 0, 500000);
FAS_UINT_ATTR(smooth_shift, fas_smooth, 1, 6);
FAS_UINT_ATTR(min_pct, fas_min_pct, 5, 90);
FAS_UINT_ATTR(min_khz, fas_min_khz, 0, 3000000);
FAS_UINT_ATTR(max_khz, fas_max_khz, 0, 3000000);

static ssize_t fps_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_bin_fps);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute fps_attr = __ATTR_RO(fps);

static ssize_t interval_us_show(struct kobject *k, struct kobj_attribute *a,
				char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%llu\n",
			 (unsigned long long)div_u64(fas_interval_ns, 1000));
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute interval_us_attr = __ATTR_RO(interval_us);

static ssize_t budget_us_show(struct kobject *k, struct kobj_attribute *a,
			      char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%llu\n",
			 (unsigned long long)div_u64(fas_budget_ns, 1000));
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute budget_us_attr = __ATTR_RO(budget_us);

static ssize_t busy_pct_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_busy_pct);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute busy_pct_attr = __ATTR_RO(busy_pct);

static ssize_t need_khz_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_need_khz);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute need_khz_attr = __ATTR_RO(need_khz);

static ssize_t ceiling_khz_show(struct kobject *k, struct kobj_attribute *a,
				char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_applied);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute ceiling_khz_attr = __ATTR_RO(ceiling_khz);

static ssize_t ceiling_little_khz_show(struct kobject *k,
				       struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%u\n", fas_applied_little);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute ceiling_little_khz_attr = __ATTR_RO(ceiling_little_khz);

static ssize_t frames_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;
	unsigned long flags;
	u64 f;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	spin_lock_irqsave(&fas_lock, flags);
	f = fas_frames;
	spin_unlock_irqrestore(&fas_lock, flags);
	ret = scnprintf(b, PAGE_SIZE, "%llu\n", (unsigned long long)f);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute frames_attr = __ATTR_RO(frames);

static ssize_t janks_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;
	u64 j;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	j = fas_janks;
	ret = scnprintf(b, PAGE_SIZE, "%llu\n", (unsigned long long)j);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute janks_attr = __ATTR_RO(janks);

static ssize_t tp_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	ret = scnprintf(b, PAGE_SIZE, "%s%s\n",
			 fas_vblank_tp ? "found" : "missing",
			 fas_tp_registered ? ",registered" : "");
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute tp_attr = __ATTR_RO(tp);

static ssize_t state_show(struct kobject *k, struct kobj_attribute *a, char *b)
{
	ssize_t ret;
	unsigned long flags;
	u64 f, j;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	spin_lock_irqsave(&fas_lock, flags);
	f = fas_frames;
	spin_unlock_irqrestore(&fas_lock, flags);
	j = fas_janks;
	ret = scnprintf(b, PAGE_SIZE,
		"enable=%u adaptive=%u fps=%u interval=%lluus budget=%lluus "
		"busy=%u%% need=%ukHz ceil=%ukHz little=%ukHz floor=%ukHz "
		"frames=%llu janks=%llu drift=%u tp=%s\n",
		fas_enable ? 1 : 0, fas_adaptive ? 1 : 0, fas_bin_fps,
		(unsigned long long)div_u64(fas_interval_ns, 1000),
		(unsigned long long)div_u64(fas_budget_ns, 1000),
		fas_busy_pct, fas_need_khz, fas_applied, fas_applied_little,
		fas_floor_applied, (unsigned long long)f, (unsigned long long)j,
		fas_drift, fas_tp_registered ? "ok" : "no");
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return ret;
}
static struct kobj_attribute state_attr = __ATTR_RO(state);

static ssize_t tick_store(struct kobject *k, struct kobj_attribute *a,
			  const char *buf, size_t n)
{
	/* 手动帧脉冲：无 vblank 环境下离线验证控制器 */
	tca_fas_vblank_probe(NULL, 0, 0);
	return n;
}
static struct kobj_attribute tick_attr = __ATTR_WO(tick);

static ssize_t reset_store(struct kobject *k, struct kobj_attribute *a,
			   const char *buf, size_t n)
{
	bool on;

	mutex_lock(&fas_control_lock);
	mutex_lock(&fas_state_lock);
	on = fas_enable;
	fas_quiesce();
	fas_reset_state();
	fas_set_enable(on);
	if (on)
		schedule_delayed_work(&fas_work, 0);
	mutex_unlock(&fas_state_lock);
	mutex_unlock(&fas_control_lock);
	return n;
}
static struct kobj_attribute reset_attr = __ATTR_WO(reset);

static struct attribute *tca_fas_attrs[] = {
	&enable_attr.attr,
	&adaptive_attr.attr,
	&target_fps_attr.attr,
	&margin_us_attr.attr,
	&headroom_pct_attr.attr,
	&gain_khz_per_ms_attr.attr,
	&smooth_shift_attr.attr,
	&min_pct_attr.attr,
	&min_khz_attr.attr,
	&max_khz_attr.attr,
	&fps_attr.attr,
	&interval_us_attr.attr,
	&budget_us_attr.attr,
	&busy_pct_attr.attr,
	&need_khz_attr.attr,
	&ceiling_khz_attr.attr,
	&ceiling_little_khz_attr.attr,
	&frames_attr.attr,
	&janks_attr.attr,
	&tp_attr.attr,
	&state_attr.attr,
	&tick_attr.attr,
	&reset_attr.attr,
	NULL,
};
static const struct attribute_group tca_fas_attr_group = {
	.attrs = tca_fas_attrs,
};

static int __init tca_fas_init(void)
{
	INIT_DELAYED_WORK(&fas_work, tca_fas_work_fn);
	for_each_kernel_tracepoint(tca_fas_find_tp, NULL);

	fas_kobj = kobject_create_and_add("tca_fas", kernel_kobj);
	if (!fas_kobj)
		return -ENOMEM;
	if (sysfs_create_group(fas_kobj, &tca_fas_attr_group)) {
		kobject_put(fas_kobj);
		return -ENOMEM;
	}

	pr_info("tca_fas: v2 ready (load x deadline -> QoS ceiling), "
		"drm_vblank_event %s, default off\n",
		fas_vblank_tp ? "found" : "NOT found");
	return 0;
}
late_initcall(tca_fas_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCA Cycle Kernel in-kernel FEAS-style Frame Aware Scheduling v2");
