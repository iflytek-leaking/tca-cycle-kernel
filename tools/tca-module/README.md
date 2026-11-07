# TCA Cycle Tuner — WebUI 模块（Magisk / KernelSU）

把 `tca-tune.sh` 包成一个可刷入的模块，在 KernelSU / BakaSU 的模块页直接打开 WebUI 点按调优。
不改内核、不改原厂 `.ko`。常规调优写入 `/proc/sys` 与 `/sys`，部分原值保存在 `/data/adb`；并非所有设置都可由 `stock` 还原。开机配置和旧 GC 属性是持久文件，不能笼统认为“重启即失效”。

## 构建

    sh tools/tca-module/build.sh
    # -> artifacts/tca-cycle-tuner-0.5.2.zip

只维护主脚本 `tools/tca-tune.sh`；构建才复制到模块目录的 gitignored 副本。

## 安装

1. BakaSU / KernelSU / Magisk 里刷入 zip。
2. 重启。
3. 管理器的模块页 -> TCA Cycle Tuner -> 打开 WebUI。

WebUI 通过管理器的 WebView 桥（`window.ksu.exec`）以 root 执行
`sh <模块目录>/tca-tune.sh ...`。只支持 KernelSU 系（KernelSU / BakaSU / KsuWebUI
等注入 `ksu` 接口的管理器）；普通浏览器打开会提示没有桥接。

UI 采用 **Material Design 3**：tonal 色彩 token、filled/tonal/outlined 按钮、MD3 开关、分段按钮、filled 文本字段、bottom sheet 输出、snackbar、暗/亮主题（跟随系统，可切换）。

**动态取色（Material You）**：读取系统已提取的壁纸主色（`/data/system/users/<id>/wallpaper_info.xml` 的 `colorValue0`）作为种子，用 [Material Color Utilities](https://github.com/material-foundation/material-color-utilities)（Apache-2.0，已 vendor 到 `webroot/vendor/mcu.js`）生成整套 MD3 角色色。可开关、可选风格（tonal spot / vibrant / expressive / content / fidelity / neutral / rainbow / monochrome）、可一键重新取色；结果缓存到 localStorage，下次打开 pre-paint 直接应用。默认开启，取不到色时自动回退到内置 baseline。

## 功能

- 一键：日常 / 游戏 / 省电 / 还原
- 调度：balance / performance / fast / powersave（Unisoc uclamp 模型）
- 触摸：Touch Boost / uclamp / IRQ 绑核（默认关）
- 音频：低延迟 profile + 锁 CPU idle
- 充电：功率墙 / 电流墙 / 一键降温 / 限充 % / 停充 / JEITA / 还原
- 网络 / IO：BBR + fq_codel、BFQ
- 开机自动应用：把档位写进 `/data/adb/tca-webui.conf`，`service.sh` 开机读取执行

## 0.5.2 安全版本：内存与 GC

- **撤回而非修补猜测式隔离**：旧版按 cmdline/root 前缀收容可能误迁移 Android 进程，限值和设备布局也未经验证。CLI 的 `mem cap/uncap/run/adopt/root/daemon/preset`、`mem guard on` 均明确报错并返回非零；不创建组、不迁移进程、不写全局内存参数。
- **保留诊断**：`mem status` / `mem show` 读取内存、PSI、zram mm_stat/io_stat、swap 设备、回收/OOM/压缩累计计数、direct_swappiness（节点存在时）和常见旧组路径；不能证明进程归属或保证 Android 不受 OOM 影响。
- **停止旧守护**：`mem guard off` 仅移除 `/data/adb/tca-ds-guard`。旧循环会在当前扫描/休眠结束后退出，不保证立即停止；不读取旧 PID、不发送信号。安装、开机服务和卸载也移除此标志。**已有进程归组、cgroup 限额、旧全局内存设置仍保留**，不能用此命令或 `stock` 假装还原；需先审查其他启动配置，再完整重启并核对设备状态。不提供全局迁移或猜测式解除限额。
- **GC 预设撤回**：旧 `0.8` 目标利用率不意味着降低 GC 频率，效果取决于 ART、工作负载和内存压力；不承诺“GC 减负”。`gc preset smooth`、`gc apply`、`gc soft-restart` 均明确失败，不写运行属性、不重启 zygote。
- **GC 诊断与回滚**：`gc status` 仅显示属性服务读数（不代表现有 ART/zygote 堆配置）及模块 `system.prop`。`gc rollback`（兼容旧 `gc preset stock`）从实际脚本目录的 `system.prop` 原子移除四个精确键：`dalvik.vm.heapstartsize`、`dalvik.vm.heaptargetutilization`、`dalvik.vm.heapminfree`、`dalvik.vm.heapmaxfree`。保留无关行；拒绝符号链接属性文件；重复执行安全。**必须完整重启设备**，其他模块可能仍覆盖这些键，不能保证恢复原厂值。
- **升级/开机**：升级包不包含旧 `system.prop`，完整重启后须核对属性；若在旧安装上手动更新脚本，先执行回滚。开机服务拒绝旧 mem/GC 写入命令（包括 GC 回滚，避免隐式更改持久配置），仅允许诊断及 `mem guard off`。其他调优保留。配置仍是空白分隔参数，不是 shell 脚本；不执行引号语法、变量替换或通配符。WebUI 不再保存内存守护配置。

## 回归测试（无需 root / Android）

    python3 tools/tests/test_tca_tune_safety.py

临时目录和 mock 验证 GC 删除最后一个键、精确键/重复执行、失败时保留原文件、拒绝危险旧命令、argv 保留和开机过滤。不是设备端隔离/ART 效果验证。

## 文件

- `module.prop` / `customize.sh` / `service.sh` / `uninstall.sh`
- `tca-tune.sh`：构建时从 `../../tools/tca-tune.sh` 复制（已 gitignore，不提交）
- `webroot/{index.html,style.css,app.js}`

## 注意

- 脚本执行需要 root；管理器 WebUI 本身即 root。
- `charge limit <pct>` 会起一个每 20s 轮询的守护，取消用 `charge limit stop`。
- 卸载会清理 `/data/adb/tca-tune.sh`、`/data/adb/tca-*.saved`、`/data/adb/tca-webui.conf`。
