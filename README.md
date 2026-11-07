# TCA Cycle Kernel

面向 **iFlytek SE50 / S30 Pro**（展锐 **UMS9620 / QogirN6Pro**，Android 13）的第三方 Linux 5.4.210 内核，
代号 **TCA Cycle Kernel**（运行时 `uname -r` → `5.4.210-tca-cycle`）。

> 社区/个人维护的第三方内核，**与 iFlytek、ZTE、Unisoc 官方无关**。
> 刷机有风险：请先备份自己的 `boot` 分区，并确认底包是本机型的原厂固件。

---

## 特性

| 特性 | 说明 |
|---|---|
| **原厂模块兼容** | `CONFIG_MODVERSIONS=n` + 强制 vermagic `modversions` + `CONFIG_TCA_CYCLE_MODULE_SKIP_VERMAGIC_VERSION`（忽略 UTS release），**全部原厂 `.ko` 无需重编即可加载** |
| **FAS 帧感知调度** | 用 DRM vblank 间隔 + 各簇 busy×实测频率估算单帧工作量，通过 cpufreq QoS 天花板调节频率；默认关闭，旋钮在 `/sys/kernel/tca_fas/` |
| **触摸修复** | Himax HX83102J：总线读返回值不再被吞、异常恢复移出 IRQ 线程、IRQ 禁用引用严格配对、日志限流 |
| **触摸 Boost** | 可选：触摸时锁频 / uclamp 提升 / 浅休眠 QoS，默认关闭 |
| **KernelSU / ReSukiSU** | 通过 git submodule 集成 |
| **调优模块** | `tools/tca-module/`：Magisk/KernelSU 模块，带 WebUI（FAS / 触摸 / 充电 / 音频 / 内存诊断） |
| **AnyKernel3 打包** | `anykernel3/`：适配任意 `ums9620*` 设备，保留设备原有 ramdisk |

## 目录结构

```
kernel_ums9620_se50/            内核源码（展锐 BSP + 本项目修改）
  arch/arm64/boot/dts/          仅保留 SE50 (UMS9620 2H10) 设备树
anykernel3/                     AnyKernel3 打包骨架
tools/tca-module/               TCA Cycle Tuner（模块 + WebUI）
tools/tca-tune.sh               调优脚本（FAS/调度/触摸/充电/音频/内存）
tools/verify-vendor-modules.py  原厂模块静态预检（符号级）
patches/                        移植补丁
reports/                        逆向 / 移植 / 复查记录（含一次 netns panic 的完整分析）
build.sh                        构建脚本
```

## 构建

**依赖**

- Android **Clang r416183b**（LLVM 12.0.5，AOSP prebuilt，需自行下载）
- `aarch64-linux-gnu-` binutils（`as` / `ld` / `nm`）
- `make`、`bison`、`flex`、`bc`

**命令**

```bash
git clone --recursive <this-repo> tca-kernel
cd tca-kernel

CLANG_BIN=/path/to/clang-r416183b/bin ./build.sh
# 产物: build/se50-clang-out/arch/arm64/boot/Image
```

常用变量：

| 变量 | 作用 |
|---|---|
| `DO_MODULES=1` | 额外编译内核模块（默认不编，直接用原厂 `.ko`） |
| `DO_DT=1 DO_DTBO=1` | 编译 base DTB 与 SE50 overlay（默认不编，用原厂 DT） |
| `LTO=thin\|full\|keep` | LTO 模式，默认 `thin`；**不要用 `none`**（会关掉 CFI，原厂模块加载失败） |
| `CLEAN=1\|mrproper` | 清理 |
| `JOBS=N` | 并行度 |

> **必须保留 CFI**：原厂 74 个模块 import `__cfi_slowpath`，关掉 LTO/CFI 会导致模块加载失败。

## 刷机

1. 生成 AK3 zip：把编译出的 `Image` 覆盖到 `anykernel3/Image`，然后
   `cd anykernel3 && zip -r9 ../tca-kernel.zip .`
2. 用 Magisk/KernelSU 或 recovery 刷入。
   AK3 只替换内核，**保留设备原有 ramdisk / DT**，适用于任意 `ums9620*` 设备。
3. 若直刷 `boot.img`，必须用**你自己设备**的原厂 boot 作为底包重打包，不要跨机型。

## 验证

```bash
# 原厂模块静态预检（符号级，不是运行时保证）
python3 tools/verify-vendor-modules.py <原厂模块目录> \
        build/se50-clang-out/arch/arm64/boot/Image \
        --vmlinux build/se50-clang-out/vmlinux
```

## 已知限制

- 仅保留 SE50（UMS9620 2H10）设备树，其他板型/SoC 的 DTS 已删除
- FAS、触摸 Boost 默认关闭，需手动开启
- NPU（Imagination PowerVR AX3xxx）驱动位于 `vendor_dlkm` 分区，**不在本仓库**；本仓库只保证内核导出符号与其兼容
- 未在全部 `ums9620` 机型上验证

## 致谢与来源

- [KernelSU](https://github.com/tiann/KernelSU) / [BakaSU](https://github.com/Baka-SU/BakaSU)（GPL-2.0，原 ReSukiSU）
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3)（GPL-2.0）
- [Kasumi](https://github.com/hymo-repo/kasumi)（GPL-2.0，`tools/kasumi/`）

## 许可

内核遵循 **GPL-2.0**（见 `LICENSE` 与 `kernel_ums9620_se50/COPYING`）。
本仓库新增的脚本/工具同样以 GPL-2.0 发布。第三方组件见 `NOTICE`。
