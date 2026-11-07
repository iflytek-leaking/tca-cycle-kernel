# 内核调优：BFQ / fq / BBR + 滑动跟手

## 1. 这一版加了什么

全部是**纯新增**（新调度器、新 qdisc、新拥塞控制算法），**不动任何结构体布局**，
所以原厂 160 个 vendor 模块的 ABI **完全不受影响**。

| 配置 | 作用 |
|---|---|
| `CONFIG_IOSCHED_BFQ=y` | BFQ I/O 调度器（Budget Fair Queueing，交互/软实时优先） |
| `CONFIG_BFQ_GROUP_IOSCHED=y` | BFQ 的 cgroup 分层（配 `BLK_CGROUP`） |
| `CONFIG_NET_SCH_FQ=y` | `fq` 网络 qdisc —— **BBR 的推荐搭档**（配合 TCP 层 pacing） |
| `CONFIG_NET_SCH_FQ_CODEL=y` | `fq_codel`，抗 bufferbloat，作为系统默认 qdisc |
| `CONFIG_NET_SCH_CAKE=y` | `cake`（更强的 AQM，可手动切） |
| `CONFIG_TCP_CONG_ADVANCED=y` | 打开拥塞控制选择菜单 |
| `CONFIG_TCP_CONG_BBR=y` | **BBR v1**（5.4 主线内建版本） |
| `CONFIG_DEFAULT_BBR=y` | 默认拥塞控制 = BBR |
| `CONFIG_NET_SCH_DEFAULT=y` + `CONFIG_DEFAULT_FQ_CODEL=y` | 默认 qdisc = fq_codel（原来是无脑 `pfifo_fast`） |

### 踩过的坑：`DEFAULT_TCP_CONG` 是个 choice

`net/ipv4/Kconfig` 里 `DEFAULT_TCP_CONG` 是 string，但它的值由 **choice**
`DEFAULT_BIC/DEFAULT_CUBIC/.../DEFAULT_BBR` 决定：

```
choice
    prompt "Default TCP congestion control"
    default DEFAULT_CUBIC
    config DEFAULT_BBR
        bool "BBR" if TCP_CONG_BBR=y
endchoice

config DEFAULT_TCP_CONG
    string
    default "bbr" if DEFAULT_BBR
    default "cubic"
```

只 `--set-str DEFAULT_TCP_CONG bbr` 没用（`olddefconfig` 会保已有值/被 choice 覆盖）。
**必须** `--enable DEFAULT_BBR --disable DEFAULT_CUBIC`。

## 2. ABI 复核（每次都要做）

```
diff /tmp/asm-offsets.before.h build/se50-clang-out/include/generated/asm-offsets.h
→ 完全一致（task_struct 尺寸与所有字段偏移未变）
```

`tools/verify-vendor-modules.py` → **160 模块 / 0 未解析 / PASS**

## 3. 运行时调优脚本

`tools/tca-tune.sh` —— 全部通过 `/proc/sys` 与 `/sys` 生效，**不写盘、不刷机、随时还原**。

```bash
adb push tools/tca-tune.sh /data/local/tmp/ && adb shell su -c 'sh /data/local/tmp/tca-tune.sh show'
su -c 'sh /data/local/tmp/tca-tune.sh smooth'   # 滑动跟手（日常）
su -c 'sh /data/local/tmp/tca-tune.sh game'     # 游戏（大核锁 performance）
su -c 'sh /data/local/tmp/tca-tune.sh net'      # 只调网络
su -c 'sh /data/local/tmp/tca-tune.sh io'       # 只调 I/O
su -c 'sh /data/local/tmp/tca-tune.sh stock'    # 还原
```

### smooth 里那几个数为什么这么取

8 核（4×A76 + 4×A55）时，内核在 `sched_init_granularity()` 里把源码里的
**归一化值**乘上 `1 + ilog2(ncpus) = 4`，所以开机后实际是：

| 旋钮 | 内核默认（8 核） | smooth 设为 | 理由 |
|---|---|---|---|
| `sched_wakeup_granularity_ns` | 4 ms | **1 ms** | 唤醒任务更愿意抢占当前任务 → 触摸/渲染线程更快上 CPU，**跟手手感主要来自这里** |
| `sched_min_granularity_ns` | 3 ms | **1.5 ms** | 时间片更细，交互任务更快被轮到 |
| `sched_migration_cost_ns` | 500 µs | **250 µs** | 更愿意把任务迁到空闲核（本来负载均衡太快在手机上容易抖） |
| `sched_schedstats` | 1 | **0** | 关掉调度统计，省一点热路径开销 |
| `sched_energy_aware` | 1 | **1** | 保持 EAS（A76/A55 的能耗模型），别关 |
| `vm.swappiness` | 100 | 100 | 有 zram 时高一点更划算 |
| `vm.watermark_scale_factor` | 10 | 150 | 提前回收，减少卡顿式直接回收 |
| I/O | mq-deadline | **BFQ** + `read_ahead_kb=512` + `rq_affinity=2` | BFQ 按权重分配带宽、优先交互；`rq_affinity=2` 让 I/O 完成中断落到**发起 I/O 的核**（大核） |
| 网络 | cubic + pfifo_fast | **BBR + fq_codel** | BBR 不靠丢包判断拥塞；fq_codel 抗 bufferbloat |

> 注意：`default_qdisc` 只对**新建**接口生效，脚本里同时用 `tc qdisc replace` 把现有接口也切了。

### 开机自动应用

```bash
su -c 'mkdir -p /data/adb/service.d && cp /data/local/tmp/tca-tune.sh /data/adb/service.d/99-tca-tune.sh && chmod 755 /data/adb/service.d/99-tca-tune.sh'
# 然后在 99-tca-tune.sh 末尾加一行: sh /data/adb/service.d/99-tca-tune.sh smooth
```

## 4. 日志一键抓取（顺便做的）

`tools/tca-grab-logs.sh` —— 把 **pstore（内核 panic 现场）+ tombstones + dmesg + logcat
+ KSU 日志 + 调优状态** 打成一个 tar.gz：

```bash
su -c 'sh /data/local/tmp/tca-grab-logs.sh'
# 产出 /sdcard/tca-logs-<时间戳>.tar.gz
```

**关键区分**：内核 panic 只留 pstore，**不产生 tombstone**；tombstone 是用户态进程收信号时写的。
之前那 16 个 tombstone 全是旧的和无关的（10 个是同一个 App 自己崩）。

## 5. 还没做的（等你决定）

* **BBR v2 / v3**：现在编进去的是 5.4 主线内建的 **BBR v1**。要 v2/v3 得从 `google/bbr`
  或高版本 ACK 往回移植（v3 依赖不少新 TCP 基础设施），工作量不小但可行。
* **触摸 boost**：目前 `sprd-cpufreq-v2.ko` 里有 `sprd,cpufreq-boost` 逻辑，
  但原厂 DT 里没搜到这个属性。要更极致的跟手，可以写个小驱动：
  在触摸中断时把大核 `scaling_min_freq` 抬到某个档、80~120 ms 后回落。
  这是**真正能立刻感觉到**的那一项，但需要重编。
* **KSM / frontswap**：后台留存还能再挤一点。
