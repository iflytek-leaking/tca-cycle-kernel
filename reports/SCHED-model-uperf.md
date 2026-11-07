# 调度模型改造（参考 Uperf Game Turbo）

## 1. Uperf 是什么（结论）

用户态性能控制器,不是内核 scheduler。bin/uperf(闭源)靠:

- 读 /dev/input + SurfaceFlinger 帧分析识别场景 (touch/gesture/trigger/switch)
- sched_setaffinity / sched_setscheduler 调每线程亲和性/优先级
- /dev/cpuset/*/tasks 分区, cpufreq scaling_min/max_freq
- 展锐上靠 /proc/ppm/policy/hard_userlimit_* —— 我们内核没有 PPM
- 按 ro.board.platform -> ro.product.board 选 config/*.json, 没有 ums9620 -> unsupported

所以 Uperf 不能直接跑; 但它的模型可以映射到我们内核已有的原语。

## 2. 我们内核已有的调度模型原语

    CONFIG_UCLAMP_TASK=y / CONFIG_UCLAMP_TASK_GROUP=y
    CONFIG_ENERGY_MODEL=y / CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y
    /dev/cpuctl (cgroup v1 cpu) + /dev/cpuset

Unisoc 还加了自己的 boost 变换 (kernel/sched/fair.c:3721 / sched.h:2429):

    if (sysctl_sched_uclamp_min_to_boost)   /* /proc/sys/kernel/sched_clamp_min2boost, 默认 1 */
            clamp_util = uclamp_transform_boost(util, min_util, max_util);
    else
            clamp_util = clamp(util, min_util, max_util);

即 cgroup/task 的 uclamp.min 会被放大成 util 增量 -> EAS 选大核 + schedutil 升频。
这就是 Uperf 的 affinity/prio 那套在内核里的等价物。

cpuctl 文件是 v1 命名(不是 v2 的 cpu.uclamp.min):

    /dev/cpuctl/<group>/uclamp.min      # 百分比, 1..100 或 max
    /dev/cpuctl/<group>/uclamp.max

## 3. A: 用户态 uclamp 档位(脚本)

tca-tune.sh 新增 sched 档位(照 Uperf 的 balance/powersave/performance/fast):

| 档位 | top-app.min | foreground.min | background.max | restricted.max |
|---|---|---|---|---|
| balance | 20 | 5 | 60 | 40 |
| performance | 60 | 40 | max | 80 |
| fast | max | 60 | max | max |
| powersave | 10 | 0 | 30 | 20 |

    su -c 'sh /data/adb/tca-tune.sh sched balance'      # 日常
    su -c 'sh /data/adb/tca-tune.sh sched performance'  # 游戏
    su -c 'sh /data/adb/tca-tune.sh sched fast'
    su -c 'sh /data/adb/tca-tune.sh sched powersave'
    su -c 'sh /data/adb/tca-tune.sh sched'              # 看当前

同时确保 /proc/sys/kernel/sched_clamp_min2boost=1。原始 uclamp 值已纳入 save/stock 可还原。

## 4. B: 内核态 触摸场景 -> uclamp boost

tca_touch_boost.c 扩展: 触摸时临时把目标 cgroup 的 uclamp.min 提高,
持续 touch_boost_ms, 然后恢复原值 —— 对应 Uperf 的 touch/trigger 场景。

| sysfs | 默认 | 说明 |
|---|---|---|
| touch_uclamp_enable | 0 | 开关 |
| touch_uclamp_pct | 50 | 触摸期间写入的 uclamp.min 百分比 |
| touch_uclamp_path | /dev/cpuctl/top-app/uclamp.min | 目标 cgroup 文件 |

实现: 用 filp_open()/kernel_read()/kernel_write() (都是 EXPORT_SYMBOL)读写 cgroup 文件,
只在每次触摸爆发的起点写一次 + 结束时恢复一次 (不是每帧写)。
不用 cgroup_get_from_fd() —— 它要求 cgroup_on_dfl(), 而本机 cpuctl 是 v1 层级。

## 5. 怎么用

    su -c 'sh /data/adb/tca-tune.sh touch all'     # freq boost + uclamp boost + IRQ 绑核 全开
    su -c 'sh /data/adb/tca-tune.sh touch off'     # 全关
    su -c 'sh /data/adb/tca-tune.sh touch uclamp 1'
    echo 70 > /sys/touchscreen/touch_uclamp_pct
    echo /dev/cpuctl/foreground/uclamp.min > /sys/touchscreen/touch_uclamp_path

## 6. 风险 / ABI

- 不碰任何结构体布局; 验证仍是 asm-offsets.h diff 为空 + verify-vendor-modules.py 160/0
- 全部默认关闭, 不开启时行为与之前完全一致
- A 是纯 sysfs 写, 随时 stock 还原
- B 从触摸 threaded IRQ 上下文调用 filp_open/kernel_write (进程上下文, 可睡眠), 路径不存在时静默跳过

## 7. 没做的

- C (给 Uperf 写 ums9620.json 让它跑): 闭源二进制 + 它依赖的 /proc/ppm 我们没有,
  而且会和内核 boost 打架。不建议。
- 每线程 affinity/nice 规则 (RenderThread/UnityMain -> 大核): 那是 Uperf 用户态 daemon 干的活,
  内核不做进程名匹配; 我们用 cgroup uclamp 达到同等效果。
