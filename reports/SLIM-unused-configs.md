# 内核瘦身分析 — 未使用 CONFIG 候选（TCA Cycle Kernel / se50）

## 方法

`tools/find-unused-configs.py <src> <O/.config> <dt dts...>`

把**已启用（=y）**的驱动配置映射到源码，提取其 `of_device_id` `compatible`，与 **se50 实际 DT**
（`stock/base.dts` + `stock/se50.dts`，共 325 个 compatible）比对；**compatible 全都不在 DT**
且只构建少量文件（≤3）的即为候选。

⚠️ **这只是候选**，两类误报必须人工排除：
1. 框架类配置（`CONFIG_ARM64/PM/CRYPTO/REGULATOR/GPIOLIB/MMC`…）会把整个目录的 compatible 算进来
2. 用 `IRQCHIP_DECLARE/CLOCK_OF_DECLARE/TIMER_OF_DECLARE/OF_DECLARE` 声明的驱动不含 `of_device_id`
   （如 `arm,gic-v3` 用 `IRQCHIP_DECLARE`，实际**在用**）

## A. 高置信候选（非展锐、非 arm64 平台，se50 必然不用）

| CONFIG | 源码行数 | 来源 |
|---|---|---|
| `CONFIG_RPMSG` | 1953 | Qualcomm SMD/GLINK |
| `CONFIG_POWER_AVS` | 1675 | Rockchip |
| `CONFIG_INTERCONNECT` | 1439 | Qualcomm |
| `CONFIG_SPMI` | 1355 | Qualcomm PMIC arb |
| `CONFIG_PM_DEVFREQ_EVENT` | 1263 | Rockchip / Exynos |
| `CONFIG_ARM_SCPI_PROTOCOL` | 1040 | ARM SCPI |
| `CONFIG_ARM_SCMI_PROTOCOL` | 989 | ARM SCMI |
| `CONFIG_GNSS` | 887 | 第三方 GNSS（u-blox/sirf/mtk） |
| `CONFIG_VIRTIO` | 815 | virtio_mmio |
| `CONFIG_PCI_MSI` | 619 | ppc fsl_msi |
| `CONFIG_PCIE_KIRIN` | 543 | HiSilicon Kirin |
| `CONFIG_SND` | 542 | hda_tegra（音效里的 tegra HDA） |
| `CONFIG_UIO` | 478 | fsl elbc-gpcm |
| `CONFIG_HVC_DRIVER` | 419 | IBM OPAL |
| `CONFIG_COMMON_CLK_SCPI` | 314 | SCPI clocks |
| `CONFIG_MFD_ACT8945A` | 92 | Active-semi |

> 注：像 `CONFIG_SND`/`CONFIG_PCI_MSI` 是整个子系统的开关，**不能整关**；这里指的是把它们下面
> 的 **arch/其它厂商子项** 关掉（如 `CONFIG_SND_HDA_TEGRA`、`CONFIG_PCI_MSI` 的 ppc 部分）。

## B. 需人工确认（通用驱动，se50 DT 里没有对应 compatible）

| CONFIG | 判断依据 |
|---|---|
| `CONFIG_ARM_GIC_V3_ITS` / `_PCI` / `CONFIG_ARM_GIC_V2M` | DT 无 `arm,gic-v3-its`/`gic-v2m-frame`；若无 PCIe/MSI 设备可关 |
| `CONFIG_SERIAL_AMBA_PL011` / `CONFIG_SERIAL_OF_PLATFORM` | 用 `sprd_serial`（DT 有 `sprd,sc9836-uart`） |
| `CONFIG_PINCTRL_SINGLE` | 用展锐 pinctrl |
| `CONFIG_TOUCHSCREEN_GOODIX` | se50 用 `himax,hxcommon`（但 ck10 用 goodix，做通用内核则保留） |
| `CONFIG_HID`（i2c-hid 部分） | 若有 i2c-hid 设备才需要 |
| `CONFIG_USB_EHCI_HCD_PLATFORM` | 用 DWC3 |
| `CONFIG_OF_MDIO` | 无 MDIO 以太网 |
| `CONFIG_PM_OPP` / `CONFIG_PM_GENERIC_DOMAINS` / `CONFIG_VIDEO_DEV` | 框架，通常**必须保留**，只有子项可关 |
| `CONFIG_USB_DWC3_OF_SIMPLE` | 用 `snps,sprd-dwc3` |
| `CONFIG_REGULATOR_FIXED_VOLTAGE` | 若 DT 用 `regulator-fixed` 则保留 |

## C. 误报（别关）

`CONFIG_ARM64`、`CONFIG_ARM_GIC_V3`（`IRQCHIP_DECLARE` 在用）、`CONFIG_PM`、`CONFIG_CRYPTO`、
`CONFIG_REGULATOR`、`CONFIG_GPIOLIB`、`CONFIG_SERIAL_SPRD`、`CONFIG_SPRD_TIMER`、`CONFIG_CORESIGHT_*`

## 安全流程（务必逐个）

```sh
cd kernel_ums9620_se50
scripts/config --file /path/to/.config --disable CONFIG_RPMSG
make O=/path/to/build/se50-clang-out olddefconfig
./build.sh            # 或 make Image modules
# 刷机 → 开机 → dmesg 无 Unknown symbol / probe 失败
```

一次一个、能开机再下一个。回滚：`--enable` 反向操作。

## 预期收益

- 高置信 A 类合计约 **1.4 万行源码**，编进 vmlinux 的部分会直接缩小内核、减少编译对象。
- 真正的编译时间大头是驱动数量；关掉这些**非展锐/非本平台**驱动是安全且有效的方向。
- 若还要更狠：把只用于别的机型的 vendor 驱动也关掉（但会影响"通吃 5 机型"）。

## 工具

```sh
python3 tools/find-unused-configs.py kernel_ums9620_se50 \
        build/se50-clang-out/.config stock/base.dts stock/se50.dts
```
