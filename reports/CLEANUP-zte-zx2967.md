# 清理补丁 — 移除 ZTE ZX2967 平台组件

**性质：纯删除，不影响 se50（UMS9620）任何运行时行为。**
规模：**51 个文件删除、27 个 Kconfig/Makefile 同步、13798 行、0 新增**。
补丁：`patches/remove-zte-zx2967.patch`（396K）。

## 为什么安全

这些是**上游 ZTE ZX2967 SoC**（与展锐 UMS9620 毫无关系的另一颗 ZTE 芯片）的平台驱动。
实测配置状态：

```
# CONFIG_ARCH_ZX is not set          ← 总开关关
# CONFIG_DRM_VENDOR   (depends on ARCH_ZX)  ← 未生成
# CONFIG_ZX_TDM / ZX_SPDIF / ZX_I2S / ZX_DMA
# CONFIG_I2C_ZX2967 / IR_ZX / POWER_RESET_ZX
# CONFIG_ZX2967_THERMAL / ZX2967_WATCHDOG / MMC_DW_ZX
# CONFIG_SND_SOC_ZX_AUD96P22 / PINCTRL_ZX / SOC_ZX296702
```

**全部 off → 一个字节都没进 vmlinux**，删除后 `olddefconfig` 通过、无残留引用。

## 删除清单

### 整目录删除（纯 ZX，共 40 文件）
| 目录 | 原 Kconfig | 原 Makefile 引用 |
|---|---|---|
| `drivers/gpu/drm/vendor/` | `drivers/gpu/drm/vendor/Kconfig`（DRM_VENDOR） | `drivers/gpu/drm/Makefile` `obj-$(CONFIG_DRM_VENDOR) += vendor/`；父 Kconfig `source` 行 |
| `drivers/pinctrl/vendor/` | PINCTRL_ZX / PINCTRL_ZX296718 | `drivers/pinctrl/Makefile` `obj-$(CONFIG_PINCTRL_ZX) += vendor/`；父 Kconfig `source` |
| `drivers/soc/vendor/` | ZX2967_PM_DOMAINS | `drivers/soc/Makefile` `obj-$(CONFIG_ARCH_ZX) += vendor/`；父 Kconfig `source` |
| `sound/soc/vendor/` | ZX_SPDIF / ZX_I2S / ZX_TDM | `sound/soc/Makefile` `obj-$(CONFIG_SND_SOC) += vendor/`；父 Kconfig `source` |
| `arch/arm64/boot/dts/vendor/` | — | `arch/arm64/boot/dts/Makefile` `subdir-y += vendor` |
| `drivers/clk/vendor/` | （无 Kconfig） | `drivers/clk/Makefile` `obj-$(CONFIG_ARCH_ZX) += vendor/` |

### 单文件删除（10）
| 文件 | CONFIG | Makefile |
|---|---|---|
| `drivers/dma/zx_dma.c` | `ZX_DMA` | `drivers/dma/Makefile` |
| `drivers/i2c/busses/i2c-zx2967.c` | `I2C_ZX2967` | `drivers/i2c/busses/Makefile` |
| `drivers/media/rc/zx-irdec.c` | `IR_ZX` | `drivers/media/rc/Makefile` |
| `drivers/media/rc/keymaps/rc-zx-irdec.c` | `IR_ZX` | `drivers/media/rc/keymaps/Makefile` |
| `drivers/power/reset/zx-reboot.c` | `POWER_RESET_ZX` | `drivers/power/reset/Makefile` |
| `drivers/thermal/zx2967_thermal.c` | `ZX2967_THERMAL` | `drivers/thermal/Makefile` |
| `drivers/watchdog/zx2967_wdt.c` | `ZX2967_WATCHDOG` | `drivers/watchdog/Makefile` |
| `drivers/mmc/host/dw_mmc-zx.c` | `MMC_DW_ZX` | `drivers/mmc/host/Makefile` |
| `sound/soc/codecs/zx_aud96p22.c` | `SND_SOC_ZX_AUD96P22` | `sound/soc/codecs/Makefile`（obj + objs 两行） |
| `include/dt-bindings/soc/vendor,pm_domains.h` | — | （随 ZX DTS/pm-domain 删除） |

### 同步修改的 Kconfig/Makefile（27 个）
`arch/arm64/boot/dts/Makefile`、`drivers/clk/Makefile`、`drivers/{dma,gpu/drm,i2c/busses,media/rc,media/rc/keymaps,mmc/host,pinctrl,power/reset,soc,thermal,watchdog}/{Kconfig,Makefile}`、
`sound/soc/Kconfig`、`sound/soc/Makefile`、`sound/soc/codecs/{Kconfig,Makefile}`

> 注：debran 后目录名从 `zte` 改为 `vendor`、`CONFIG_DRM_ZTE` 改为 `CONFIG_DRM_VENDOR`，本补丁按当前名同步。

## 应用 / 回滚

```sh
cd kernel_ums9620_se50
git apply /path/to/patches/remove-zte-zx2967.patch     # 应用
git apply -R /path/to/patches/remove-zte-zx2967.patch  # 回滚
```

已应用状态：分支 `ifly-sw-fast-charge`，改动相对 `3f4b97fd5`（`git diff HEAD` 即得此补丁）。

## 进一步可选（未做，规模更大）

| 项 | 体量 | 说明 |
|---|---|---|
| `arch/arm/mach-zx/` + `arch/arm/boot/dts/zx296702-ad1.dts` | 小 | ARM32 的 ZX 支持，arm64 不编；需同步 `arch/arm/Makefile/Kconfig` |
| `arch/arm/`（含 `boot/dts` 19M） | 38M | 整个 ARM32 树，仅 arm64 构建用不到 |
| 其余 arch（powerpc/x86/mips/…） | ~84M | 同上，只留 arm64 可省 **~122M** |
| 其他展锐板级 DTS（peony/harrow/coconut/…） | ~850K | 非 se50 |

## 限制

- 本补丁只删**源码死代码**，**不减小 vmlinux/Image**（它们本就没编）。
- 要真正给内核瘦身，得做 `CONFIG_*` 分析（按 se50 的 DT compatible 反查用不到的驱动）。
- 建议重建一次确认：`./build.sh`（`olddefconfig` 已验证通过）。
