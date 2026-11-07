# Plan A — 全原厂模块（TCA Cycle Kernel）

**目标：160 个原厂模块一个不少、一个不加载失败。原厂模块文件一个都不改。**

## 一、需要做的内核侧改动（都已在树里）

| 项 | 文件 | 说明 |
|---|---|---|
| 关 CRC 校验 | `.config` / `f50_defconfig` | `CONFIG_MODVERSIONS=n`（`check_version()` 直接返回 1） |
| 保 vermagic | `include/linux/vermagic.h` | `MODVERSIONS=n` 时仍输出 `"modversions "`，匹配原厂模块 vermagic |
| 忽略 UTS release | `kernel/module.c` `same_magic()` | 由 `CONFIG_TCA_CYCLE_MODULE_SKIP_VERMAGIC_VERSION` 控制，解决我们 `-dirty+` vs 原厂 `-dirty` |
| 补 2 个符号 | `drivers/vendor/common/himax_lcd_notify.c`（built-in） | `himax_resume_by_lcd` / `himax_int_enable`：原厂把 Himax 驱动编进 vmlinux，我们缺 IC 文件，用内建 shim 顶 |

## 二、模块符号缺口分析（全 160 个原厂模块）

```
vendor 模块总数 : 160
modules.load    : 61（引导加载；其余 99 个按需/热插拔）
被依赖符号      : 1753      由 vendor 模块互相提供: 290
未解析符号      : 2 → himax_int_enable / himax_resume_by_lcd（sprd-drm.ko 需要，已由内建 shim 提供）
```

补 shim 前 2 个缺失，**重建内核后应为 0**。校验命令：
```sh
python3 tools/verify-vendor-modules.py stock/vendor_rd/lib/modules \
        build/se50-clang-out/arch/arm64/boot/Image
# 期望 RESULT: PASS
```

## 三、其它加载前提（均已确认）

| 检查项 | 结果 |
|---|---|
| `module_layout` CRC | 与原厂一致（`0x2f279e7b`）；且 `MODVERSIONS=n` 下根本不校验 |
| 模块 license | 160 个全是 GPL/GPL v2/Dual → GPL-only 符号可用 |
| `CONFIG_MODULE_SIG` | 关（不验签） |
| `CONFIG_MODULE_FORCE_LOAD` | 关（不需要 `--force`） |
| `import_ns`（VFS 命名空间） | 仅 `nvt_touch_host`/`aw883xx` 声明；我们内核不导出带命名空间的符号，检查返回 0，无碍 |
| vermagic 全串 | 我们 `...-dirty+ SMP preempt mod_unload modversions aarch64`，与模块 flags 段一致 |
| 依赖顺序 | 用原厂 `modules.load`（61 条）+ `modules.dep`（160 条） |

## 四、交付与操作

1. **重编内核**（`./build.sh`）→ 得到新的 `Image`（含内建 shim；`MODVERSIONS=n`）。
2. **boot 分区**：只替换 boot.img 里的 kernel，DTB/DTBO/vendor_boot 保持原厂。
3. **vendor 分区**：**160 个 .ko 与 `modules.load`/`modules.dep`/`modules.alias`/`modules.softdep` 全部保持原厂不动**。
   （我们自编的模块/移植驱动一律**不装**，它们与本方案无关。）
4. 重启。

> 本方案下：**不修改任何原厂 .ko，不额外依赖自编模块**，唯一内核源码新增是 4 行内建 shim。

## 五、边界说明

- `himax_lcd_notify.c` shim 是内建（`obj-y`），不是模块；不违反"全原厂模块"。
- se50 实际用 **Novatek** 触摸（`nvt_touch_host.ko`），Himax 只是原厂编进内核的历史包袱，shim 为 no-op 不影响触摸。
- 原厂模块间的结构体（`sprd_battery_info` 等）由原厂 pair 自洽，本方案不掺自编件，无错位风险。
