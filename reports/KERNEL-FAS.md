# 内核态 FAS（Frame Aware Scheduling）

## 1. 为什么不用 fas-rs 本体

fas-rs 4.x 用 eBPF uprobe + BPF ringbuf 采帧时间:

    frame-analyzer-ebpf/src/main.rs:  static RING_BUF: RingBuf = RingBuf::with_byte_size(0x1000, 0);
    frame-analyzer/src/uprobe.rs:     aya::{maps::RingBuf, programs::UProbe}   (aya 0.13)

BPF_MAP_TYPE_RINGBUF 是 Linux 5.8 才有的,aya 还要 BTF。我们 5.4.210:

    kernel/bpf/ringbuf.c            -> 不存在
    BPF_MAP_TYPE_RINGBUF            -> uapi 里没有
    CONFIG_DEBUG_INFO_BTF           -> is not set

backport ringbuf+BTF 到 5.4 工程量太大且风险高,不值。

## 2. 我们的做法:用 5.4 已有的东西实现同一件事

帧源:内核里**已经内建**的 drm_vblank_event tracepoint
(drivers/gpu/drm/drm_trace.h, TP_PROTO(int crtc, unsigned int seq))。
它不在 include/trace/events/ 下,所以我们用运行时查找 + 注册:

    for_each_kernel_tracepoint(find, NULL);                    /* EXPORT_SYMBOL_GPL */
    tracepoint_probe_register(tp, tca_fas_vblank_probe, NULL); /* EXPORT_SYMBOL_GPL */

执行器:每个 CPU cluster 一个 FREQ_QOS_MIN 请求。cpufreq 对 min 请求按 max 聚合,
所以它和 tca_touch_boost 的触摸 boost 自然叠加,不会互相覆盖。

控制器:维护 vblank 间隔的 EMA,和目标帧预算 (1e9 / target_fps) 比较;
帧晚了就上抬 min freq,帧很宽裕就下压;超过 250ms 没帧就释放(回落)。

## 3. sysfs 接口(/sys/kernel/tca_fas/)

| 节点 | 默认 | 说明 |
|---|---|---|
| enable | 0 | 总开关 |
| target_fps | 60 | 目标帧率(10..240) |
| margin_us | 1500 | 容忍的帧超时(us,0..20000) |
| step_khz | 100000 | 每次调整步长(kHz) |
| min_khz | 0 | 请求下限(0 = 允许释放) |
| max_khz | 1800000 | 请求上限(kHz) |
| cur_khz | - | 当前请求的 min(只读) |
| frames | - | 累计 vblank 计数(只读,用来验证帧源是否在动) |
| interval_us | - | 当前 EMA 帧间隔(只读) |
| tp | - | found / missing / registered(只读) |
| tick | - | 手动打一帧(只写,用于没有 vblank 事件时验证控制器) |

## 4. 上机验证(关键)

    # 1) 确认帧源挂上了
    su -c 'cat /sys/kernel/tca_fas/tp'        # 期望: found,registered
    # 2) 开
    su -c 'echo 1 > /sys/kernel/tca_fas/enable'
    # 3) 滑动/放视频,看帧计数和间隔有没有动
    su -c 'cat /sys/kernel/tca_fas/frames'
    su -c 'cat /sys/kernel/tca_fas/interval_us'   # 期望接近 16667 (60fps)
    su -c 'cat /sys/kernel/tca_fas/cur_khz'       # 帧晚了应往上走

如果 frames 不动(tp 显示 found,registered 但计数为 0),说明 sprd-drm 没走标准
vblank 路径。此时:

    # 用 tick 手动验证控制器逻辑
    su -c 'for i in $(seq 1 200); do echo 1 > /sys/kernel/tca_fas/tick; sleep 0.016; done'

然后告诉我,我改用别的帧源(sprd-drm 自己的事件 / GPU / input 代理)。

## 5. ABI / 风险

- **不碰任何结构体布局**:独立驱动,只加 tracepoint 探针 + freq_qos 请求
- 验证:asm-offsets.h diff 为空 + verify-vendor-modules.py 160/0
- **默认关闭**(enable=0),不开时零行为变化
- 探针在 vblank 上下文(原子),只做 spin_lock + 计数,不睡眠
- 控制器在 workqueue(进程上下文)调用 cpufreq/freq_qos,可睡眠,安全
- 和触摸 boost 的关系:两个独立的 FREQ_QOS_MIN 请求,cpufreq 取 max 聚合,叠加正确