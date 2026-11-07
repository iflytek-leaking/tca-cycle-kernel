# 触摸调优（A + B，全部默认关闭 + sysfs 可调）

## 1. 做了什么

给 Himax HX83102J 触摸驱动加了两个互不影响、运行时开关的调优项。
默认全部关闭，刷进去的行为和之前完全一样，想试再开。

| sysfs | 默认 | 作用 |
|---|---|---|
| touch_boost_enable | 0 | 触摸 Boost 开关 |
| touch_boost_freq | 1500000 | Boost 目标频率（kHz，大核 min） |
| touch_boost_ms | 150 | 无触摸后多久回落到原值（ms） |
| touch_irq_affinity | 0 | 触摸 IRQ 绑大核开关 |
| touch_irq_cpu | 7 | 绑到哪个 CPU |

节点路径：/sys/touchscreen/<attr>
（厂商已有的 sysfs_create_link(NULL, &ts->dev->kobj, "touchscreen")；
回退路径 /sys/bus/spi/devices/*/touch_boost_enable）

## 2. A：触摸 Boost

- 挂点：himax_ts_work()（触摸 threaded IRQ 上下文，进程上下文，可睡眠）
- 实现：cpufreq_cpu_get(4) 拿大核 policy ->
  freq_qos_add_request(&policy->constraints, ..., FREQ_QOS_MIN, 0) 一次 ->
  每次触摸 freq_qos_update_request(req, touch_boost_freq) +
  mod_delayed_work(system_wq, ..., touch_boost_ms)，到点回 FREQ_QOS_MIN_DEFAULT_VALUE(0)
- 走的是 cpufreq 核心标准 min 约束路径（FREQ_QOS_MIN），不是绕过驱动写寄存器；
  原厂 sprd-cpufreq-v2 认这条
- 会被 policy->max / thermal 自动 clamp，不会越过温控

## 3. B：触摸 IRQ 绑核

- 更正一个之前的说法：threaded IRQ 线程内核默认就是 SCHED_FIFO，
  优先级 MAX_USER_RT_PRIO/2（见 kernel/irq/manage.c:1306 setup_irq_thread()）。
  所以"提实时优先级"内核早做了，B 只剩绑核。
- 实现：irq_set_affinity_hint(irq, cpumask_of(touch_irq_cpu))
  （EXPORT_SYMBOL_GPL，内部会 __irq_set_affinity 真正生效）
- 关掉时 irq_set_affinity_hint(irq, NULL) 清掉 hint（不清回原亲和性；要完全还原重启即可）
- 默认关 = 完全不碰原厂 IRQ 分配

## 4. 怎么用

    su -c 'sh /data/adb/tca-tune.sh touch on'
    su -c 'sh /data/adb/tca-tune.sh touch off'
    su -c 'sh /data/adb/tca-tune.sh touch show'

    echo 1800000 > /sys/touchscreen/touch_boost_freq   # 1.8GHz
    echo 250     > /sys/touchscreen/touch_boost_ms     # 250ms 尾
    echo 1       > /sys/touchscreen/touch_irq_affinity
    echo 4       > /sys/touchscreen/touch_irq_cpu

## 5. ABI / 风险

- 不动任何结构体布局：只加一个源文件 + 一个 sysfs attribute group
- 验证：asm-offsets.h diff 为空 + verify-vendor-modules.py 160/0
- 默认关闭 -> 不开启时零行为变化
- 唯一真实代价是开启后的功耗/发热（有 thermal 兜底，且随时可关）

## 6. 没做的（原因）

| | 为什么不做 |
|---|---|
| 提高上报率 120/240Hz | 在触摸固件 / 板级 dtsi 里；本树 firmware_config/harrow 是反推重建、无 rate 项，面板 dtsi 也不在源码树 |
| 改触摸滤波/平滑算法 | 走 HX_REPORT_BY_ZTE_ALGO -> ZTE 算法，无客观指标，易改坏 |
| 关 HIMAX_DEBUG/INSPECT | hx_s_debug_data 变 NULL 后有未验证的解引用路径（如 himax_common.c:2037），收益又小 |
