# 音频延迟优化（A: TEO idle governor + C: 运行时档位）

## 0. 先划线:内核能做什么、不能做什么

| | 谁决定 | 我们能动吗 |
|---|---|---|
| 音频缓冲/周期大小(= 绝对延迟) | Android HAL + ro.audio.* | ❌ 用户态,不关内核的事 |
| 音频 SoC 驱动 | 原厂 .ko(snd-soc-sprd-*) | ❌ Plan A 不能改 |
| 调度抖动 / 爆音(xrun) | 内核 | ✅ 能做 |
| 空闲退出延迟(C-state) | 内核 idle governor | ✅ 能做 |

所以内核侧优化的是 **抖动 / 爆音 / 唤醒延迟**,不是「按下到出声」的绝对延迟。

## 1. A:TEO idle governor(已开)

drivers/cpuidle/governors/teo.c 本来就在树里,但没选(原厂用 menu)。
TEO(Timer Events Oriented)是面向定时器事件的 governor,**空闲退出延迟低于 menu**,
对音频/交互的唤醒抖动更友好。

改动:

    # CONFIG_CPU_IDLE_GOV_MENU is not set   ← 必须显式关,否则两个 governor 同时注册靠 rating 抢默认
    CONFIG_CPU_IDLE_GOV_TEO=y

**代价**:空闲时更倾向浅 C-state → 待机功耗略增。

## 2. C:tca-tune.sh audio 档位

    su -c 'sh /data/adb/tca-tune.sh audio'            # 应用音频低延迟档
    su -c 'sh /data/adb/tca-tune.sh audio idle on'    # 额外锁 cpu_dma_latency=0(禁深 C-state,费电)
    su -c 'sh /data/adb/tca-tune.sh audio idle off'   # 释放
    su -c 'sh /data/adb/tca-tune.sh audio off'        # 还原(= stock)

档位做的事:

| 项 | 值 | 说明 |
|---|---|---|
| kernel/sched_rt_runtime_us | 995000 | 放开 RT 节流,AudioFlinger 的 fast mixer 是 SCHED_FIFO |
| kernel/sched_schedstats | 0 | 关调度统计,减热路径开销 |
| kernel/sched_wakeup_granularity_ns | 1000000 | 更倾向快速唤醒 |
| kernel/sched_migration_cost_ns | 250000 | 减少迁移抖动 |
| 大核 scaling_min_freq | 1.0 GHz | 播放期间大核频率地板 |
| 音频 DMA IRQ | 绑 CPU4-7 | best-effort,按 mcdt/vbc/agdsp/audio/sprd*dma 名字匹配 |
| cpu_dma_latency | 0(可选) | 需要常驻 fd,由 audio idle on 起一个后台 holder 持有 |

save() 现在也会存 sched_rt_* 和每个 policy 的 scaling_governor/min/max,
stock 能完整还原。

## 3. 明确不做的

- **改 CONFIG_HZ 到 1000** —— 原厂模块按 HZ=250 编译,msecs_to_jiffies() 等宏在模块编译期
  就展开成 250 刻度;换 1000 会让**所有原厂模块的延时计算错 4 倍**。踩 Plan A 的雷,绝不动。
- 改 HAL buffer / 音频驱动 —— 不在内核侧。

## 4. 验证

- [x] TEO 编进 vmlinux(teo_governor),MENU 移除
- [x] ABI 不变(asm-offsets diff 为空);160 原厂模块 0 未解析 PASS
- [x] tca-tune.sh 语法 OK(sh -n)
- [ ] 上机:cat /sys/devices/system/cpu/cpuidle/current_governor 应为 teo
- [ ] 上机:播放音频 + 滑动,对比开/关 TEO 的爆音和跟手