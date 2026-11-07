# UMS9620 / ums9620_se50 的「狗」与卡死重启：机制梳理 + 抓现场手册

> 背景：GSI（LineageOS 15 / A15）下出现「不定期卡死 + 重启」「触摸延迟飘到天上」。
> 怀疑方向：内核里有看门狗在咬。本文把这份 dump/源码里能确认的狗全部列出来，并给出验证「是狗咬的，还是真卡死」的方法。
> 依据：`stock/stock.config`、`stock/base.dts`、`kernel_ums9620_se50/` 源码、`out/lp` 原厂分区、`se50_t30l/system.img`（GSI）。

---

## 1. 结论（先说重点）

1. **存在两只硬件狗 + 一套「全员报到」喂狗机制**，而且这套机制的设计目标恰恰就是「某个 CPU 挂住 → 40 秒后复位并把现场 dump 出来」。
2. **真正起作用的不是 Android 的 `watchdogd`**，而是展锐的 `sprd_wdf.ko`：它给**每个 CPU** 起一个高优先级线程 + 每核 hrtimer，**所有 CPU 都按时报到才 reload FIQ 看门狗**；任何一个 CPU 被卡住（关中断、自旋锁死锁、RT 饿死、驱动死锁）→ 喂不上 → 40 s 后 FIQ WDT 触发 → `sprd_wdh` 打印整套现场 → 再 `do_kernel_restart("panic")` 重启。
3. 所以「卡死重启」很可能是**先卡死、狗再咬**：狗是症状，卡死是病。要抓的是狗咬之前的现场——**那套 dump 会落在 ramoops/pstore 里**（`CONFIG_PSTORE_RAM=y` + `ramoops@fff80000`），即 `/sys/fs/pstore/console-ramoops-0`，可以用 `logcat -L` / 直接 `cat` 取出来。
4. 另外还有 `CONFIG_PANIC_ON_OOPS=y`（一 oops 就重启）和 `androidboot.init_fatal_panic=true`（init 致命就 panic）两条独立的重启路径，也要一起排除。
5. 触摸延迟大概率是同一根因的早期表现（CPU/IRQ 被占死、显示提交阻塞），不是触摸驱动本身。

---

## 2. 本机的全部「狗 / 复位源」

| # | 机制 | 谁在喂 / 触发条件 | 超时 | 重启前会留下什么 | 证据 |
|---|---|---|---|---|---|
| 1 | **FIQ WDT**（`sprd_wdt_fiq.ko`，在 vendor_boot ramdisk 里，first-stage 加载） | 被 `sprd_wdf.ko` 的「全员报到」机制 reload；也作为 `/dev/watchdog` 被 `watchdogd` 喂 | min 3 s / **max 60 s**，默认 timeout=60 | FIQ 中断 → `sprd_wdh` dump（wdt fiq assert / task stats / 各核栈 / 中断 / IO）→ `do_kernel_restart("panic")` | `drivers/watchdog/sprd_wdt_fiq.c`：`SPRD_WDT_FIQ_MAX_TIMEOUT 60`、`sprd_wdt_fiq_info.identity="Spreadtrum Watchdog Timer"` |
| 2 | **wdf 全员报到喂狗**（`sprd_wdf.ko`，vendor_dlkm `modules.load` 第 100 行） | 每 CPU 一个 `hang_debug/N` 线程（FIFO 高优先级）+ 每核 hrtimer（interval=8 s）；**所有在线 CPU 都跑过一轮** → `wdd->ops->start()` reload FIQ WDT | **g_timeout=40 s**，pretimeout=20 s | 见 #1（它就是触发 #1 的那只手） | `drivers/soc/sprd/debug/hang_debug/sprd_wdf.c`：`g_interval=8`、`g_timeout=40`、`g_pretimeout=20`；`cpu_feed_bitmap/cpu_feed_mask` 逻辑 |
| 3 | **PMIC WDT**（`sprd_pmic_wdt.ko`，vendor_dlkm `modules.load` 第 92 行） | 由 uboot 传的 **`androidboot.wdten=`** 决定是否武装；通过 SIPC 让 PMIC/CM4 看门 | 由 PMIC/CM4 侧决定 | dmesg `sprd_pmic_wdt` / `cm4 watchdog on/off` 字样 | `drivers/watchdog/sprd_pmic_wdt.c`：`sprd_pimc_wdt_en()` 解析 `androidboot.wdten`；`sprd_pmic_wdt_enable()` 发 "watchdog on" / "watchdog rstoff" |
| 4 | **Android watchdogd** | **vendor init 主动启动**：`/vendor/etc/init/hw/init.ums9620_se50.rc` 第 7 行 `on init: start watchdogd`，第 91–95 行 `service watchdogd /system/bin/watchdogd 10 30`（10 s 喂一次、30 s 超时），`oneshot`，`seclabel u:r:watchdogd:s0` | 30 s（由参数指定） | `dmesg`/`logcat` 里 `init: Service watchdogd ... exited`；它 oneshot 且**退出后不会自动重启** | 本机 vendor rc；GSI 的 `/system/bin/watchdogd` 与 `watchdogd` SELinux 域都在（已核对 GSI 的 `plat_sepolicy.cil` 有 17 处 watchdogd） |
| 5 | **framework Watchdog（system_server）** | system_server 主线程/关键线程阻塞（HAL 卡死时常见）→ 杀进程/软重启 | 60 s 级别 | `dumpsys dropbox` 的 `system_server_watchdog`、`/data/system/dropbox/*watchdog*`、tombstone | AOSP 通用 |
| 6 | **panic / oops / init fatal** | `CONFIG_PANIC_ON_OOPS=y`；bootargs 里 `androidboot.init_fatal_panic=true` | 立即 | pstore console-ramoops 里有 oops/panic/init fatal 栈 | `stock/stock.config`；vendor_boot DTB `/chosen/bootargs` |

> 注意：`se50.dts` 里的 `cm-wdt-interval = <0x3c>`（60 s）是 **charger-manager 的充电看门狗**，不是 SoC WDT，别混淆。

---

## 3. 关键结论：GSI 场景下「没喂」是结果，不是根因

- GSI 只换 `system`，`vendor_boot` / `vendor` / `odm` / `vendor_dlkm` / `boot` 都没动：
  - 两只硬件狗的内核模块照旧加载；
  - `start watchdogd` 的 vendor rc 照旧执行；
  - GSI 的 `system` 里有 `/system/bin/watchdogd`，A15 的 plat sepolicy 里也有 `watchdogd` 域。
- 因此**正常运行时喂狗链路是完整的**；一旦出现「狗咬」，说明在它咬之前某个 CPU 已经挂住超过 40 s（或 watchdogd 被杀/被卡住 30 s）。
- 「卡死 + 重启」的顺序与 #2 的机制完全吻合：**先有 hang，40 s 后狗复位**。要抓的是 hang 的现场。

---

## 4. 抓现场：先取「上一轮死因」

设备重启后立刻执行（root：stock 上 `su`，GSI 上 `adb root`）：

```sh
# 1) uboot 报告的复位原因
adb shell getprop ro.boot.bootreason
adb shell getprop | grep -iE 'bootreason|watchdog|reset'

# 2) pstore：上一轮内核 console 日志（最关键）
adb shell ls -l /sys/fs/pstore/
adb shell cat /sys/fs/pstore/console-ramoops-0      # 文本，可能很大
adb pull /sys/fs/pstore/console-ramoops-0 ./console-ramoops-0.txt
adb shell logcat -L -d > lastboot_pstore_logcat.txt  # pstore 里的 logcat(pmsg)
adb shell cat /sys/fs/pstore/pmsg-ramoops-0 > pmsg.txt 2>/dev/null

# 3) 在 pstore 日志里找狗咬痕
adb shell cat /sys/fs/pstore/console-ramoops-0 | grep -nE \
  'sprd_wdt|wdt fiq|FIQ|watchdog|hang|wdt.*assert|panic|Oops|BUG:|init:.*fatal|CPU[0-9].*stuck|hard LOCKUP|soft lockup|rcu.*stall'

# 4) framework 狗 / 崩溃
adb shell dumpsys dropbox --print > dropbox.txt 2>/dev/null
adb shell dumpsys dropbox | grep -iE 'watchdog|system_server|crash' | head
adb shell ls -l /data/system/dropbox 2>/dev/null | grep -iE 'watchdog|crash' | head
adb shell ls -l /data/tombstones 2>/dev/null | tail
adb shell ls -l /data/minidump 2>/dev/null | tail   # 展锐 minidumpd 收集的现场

# 5) 狗现在的状态
adb shell cat /proc/cmdline | tr ' ' '\n' | grep -E 'wdten|dswdten|init_fatal_panic'
adb shell ps -A | grep -E 'watchdogd|hang_debug'
adb shell cat /proc/sprd_hang_debug/wdf_timeout      # interval/pretimeout/timeout + 喂狗 bitmap
adb shell ls -l /dev/watchdog*
adb shell dmesg | grep -iE 'sprd_wdt|sprd_pmic_wdt|hang_debug|watchdog'
```

判读：

- `bootreason=watchdog`（或 pstore 里出现 `wdt fiq assert` / `sprd_wdt`）→ **确认是狗咬的**，接着看 dump 里各 CPU 的 PC/LR、`sprd_dump_task_stats` 里哪个任务在 D/R 状态、哪个 CPU 没报到（bitmap）。常见指向：`sprd-drm` / `dpu` / `gsp` / `mali` / kworker / 中断风暴。
- 只有 `panic`/`Oops`/`BUG:` → 是内核 oops 触发 `panic_on_oops`，与狗无关。
- 只有 `system_server_watchdog` → framework 狗，通常是某个 HAL（相机/音频/显示）卡死拖死 system_server。
- pstore 为空：ramoops 32 KB 可能被后一次启动覆盖，或复位方式把它清了；此时用 `logcat -L`、UART（`console=ttyS1,115200n8`，且 `CONFIG_SPRD_HANG_DEBUG_UART=y`，hang 信息会打串口）作为兜底。

---

## 5. 实验：把狗先关掉，确认「卡死」还在不在

`/proc/sprd_hang_debug/wdt_disable` 是 wdf 模块提供的开关，写 1 会 **真正 stop 掉 FIQ WDT**（代码里 `wdd->ops->stop()`，不是「停止喂」）：

```sh
# 关掉 FIQ WDT（临时，重启失效）
adb shell "echo 1 > /proc/sprd_hang_debug/wdt_disable"
adb shell cat /proc/sprd_hang_debug/wdt_disable      # 应输出 1

# 重新打开（按默认 40s 恢复）
adb shell "echo 0 > /proc/sprd_hang_debug/wdt_disable"
```

- 若关闭后复现时变成**永久卡死、不再重启** → 证明狗只是负责重启，根因是 hang。
- 卡住时立刻抓栈（root）：
  ```sh
  adb shell "echo 1 > /proc/sys/kernel/sysrq"
  adb shell "echo w > /proc/sysrq-trigger"   # dump 所有 CPU 上任务栈到 console
  adb shell "echo t > /proc/sysrq-trigger"   # dump 任务列表
  adb shell "echo l > /proc/sysrq-trigger"   # 各 CPU 栈回溯
  adb shell dmesg | tail -400
  ```
- 想延长观察窗口可以调 wdf 超时（单位秒，>5；会同步改 FIQ WDT 的 timeout，注意 FIQ 上限 60）：
  ```sh
  adb shell "echo 60 > /proc/sprd_hang_debug/wdf_timeout"
  adb shell cat /proc/sprd_hang_debug/wdf_timeout
  ```
- 想看喂狗细节（`feed wdt cpu_feed_bitmap` 是 pr_debug）：
  ```sh
  adb shell "echo 8 > /proc/sys/kernel/printk"
  adb shell dmesg | grep -i 'feed wdt'
  ```

---

## 6. 触摸延迟单独量（避免和卡死混为一谈）

```sh
# 触摸事件本身的时间戳（哪个 event 是 himax 触摸：cat /proc/bus/input/devices）
adb shell getevent -lt /dev/input/eventX

# 帧延迟/掉帧
adb shell dumpsys SurfaceFlinger --latency
adb shell dumpsys SurfaceFlinger | head -50

# 现场 trace（复现时开）
adb shell atrace --async_start -c -b 32768 gfx view input sched freq idle wm
# ...复现...
adb shell atrace --async_stop -z > trace.html

# 中断是否在打风暴（himax 触摸 IRQ）
adb shell cat /proc/interrupts | grep -iE 'himax|touch|i2c|spi'
```

- 若 `getevent` 时间戳本身延迟 → 触摸/中断层（himax 驱动、`TP_RESUME_BY_LCD`、LCD resume QoS）；
- 若事件及时但画面出得慢 → 显示合成路径（`hwcomposer.unisoc` + `sprd-drm` + GPU/GSP），与卡死同源的可能性更大。

---

## 7. GSI 相关的最大嫌疑

在 stock（A13 用户态 + 原厂 system_ext）下不卡，在 GSI（A15 用户态）下卡，说明是**用户态新行为踩到了 vendor/kernel 侧的实现边界**。按嫌疑排序：

1. **显示栈**：A15 的 SurfaceFlinger + HIDL composer 2.4 + `hwcomposer.unisoc.so` / `dpu.unisoc.so` / `sprd-drm.ko` / GSP。合成/提交行为一变就可能让 `sprd-drm` 的 commit 路径死锁或帧中断丢失，进而整机 hang。
2. **GPU（mali_kbase.ko）**：A15 的 EGL/Vulkan 走 `vulkan.ums9620.so` + blobs，驱动 hang 会连带整个显示管线。
3. **camera/audio**：A15 相机服务 + HIDL provider 2.4 的组合；一旦 HAL 卡住，system_server 会先被 framework 狗咬（对应 #5）。
4. **内核 KMI 跨版本**：A15 framework 期望较新的 kernel 行为（如 `binder`、`memfd`、`sched` 扩展），本机是 5.4.210 / A12 KMI；个别压力路径下可能触发 vendor 模块的边界 bug。

**对照实验（最快缩小范围）**：换 **LOS 20（A13）GSI** 再跑同样的场景。若 A13 GSI 不卡 → A15 跨版本是主因，完整源码移植走 LOS 20 正好避开；若 A13 GSI 也卡 → 是 vendor/kernel 侧问题，完整移植同样会中招，必须先定位。

---

## 8. 请回传这 6 样东西（可以贴文件/压缩包）

1. `adb shell getprop ro.boot.bootreason`；
2. `/proc/cmdline` 输出；
3. `/sys/fs/pstore/console-ramoops-0`（整份，或 grep 后的段）＋ `logcat -L -d`；
4. `cat /proc/sprd_hang_debug/wdf_timeout` 与 `ps -A | grep -E 'watchdogd|hang_debug'`（在卡死前的运行态）；
5. `dmesg | grep -iE 'sprd_wdt|sprd_pmic_wdt|hang|panic'`；
6. 卡死时连着的那次 `dumpsys dropbox` / tombstone（若取得到）。

拿到 3 和 6，基本就能看出是「哪个 CPU 没报到、卡在哪个驱动」，而不是继续猜狗。
