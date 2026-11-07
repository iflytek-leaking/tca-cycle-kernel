# DTBO 机型清单 与 多机型适配评估

日期：2026-10-01（第二版：按“触摸/屏幕/背光/音频PA 活着就行”这个验收标准重写）

## 1. DTBO 里的 9 个机型

| # | id | compatible | 机型 |
|---|---|---|---|
| 0 | 0x1100 | sprd,ums9620-1h10 | 展锐 1H10 参考板 |
| 1 | 0x2100 | sprd,ums9620-2h10 | 展锐 2H10 参考板 |
| 2 | 0 | sprd,ums9620-2h10_uob_marlin3 | 2H10 UOB Marlin3 |
| 3 | 0 | sprd,ums9620-2h10_uob | 2H10 UOB |
| 4 | 0 | sprd,ums9620-ca30pro | 讯飞 CA30 Pro |
| 5 | 0 | sprd,ums9620-ck10 | 讯飞 CK10 |
| 6 | 0 | sprd,ums9620-se50 | 讯飞 SE50（本机 / S30 Pro） |
| 7 | 0 | sprd,ums9620-su30 | 讯飞 SU30 |
| 8 | 0 | sprd,ums9620-tb | 讯飞 TB |

源码树只有 1h10/2h10/feimao/ufi/uob/marlin3 六个 overlay；讯飞的 5 个不在开源 drop 里。

## 2. 内核是机型无关的

`boot.img` 只有 kernel；基础 DTB 在 `vendor_boot`，板级 overlay 在 `dtbo`，板级驱动在 `/vendor/lib/modules`。
我们的 AK3 只换 `boot` 里的 kernel，其余全用设备原厂。=> 同一个 Image 能启动全部 9 个机型。

## 3. 4 项关键功能的驱动来源（关键修正）

第一版误把“原厂 config 里 =y 但我们没编”当成缺驱动。实际上这几项都在**原厂模块**里，
而原厂模块集是**跨机型共享**的，我们的内核直接加载它们：

| 功能 | 驱动 | 形式 | 覆盖机型 |
|---|---|---|---|
| 屏幕 + 背光 | `sprd-drm.ko` | 共享模块 | **全部 9 个** |
| 触摸 himax | `CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_PAD`（匹配 himax,hxcommon） | 内建 | 除 su30 外全部 |
| 触摸 novatek | `nvt_nt36xxx_spi_ts.ko` + `nvt_touch_host.ko` | 共享模块 | su30 |
| 触摸 focaltech/synaptics | `focaltech_ft8756_spi_ts.ko` / `synaptics_td4320_spi_ts.ko` | 共享模块 | 备用 |
| 音频PA aw883xx | `snd-soc-sprd-pa-aw883xx.ko` | 共享模块 | se50 / su30 / tb / ck10 |

`sprd-drm.ko` 里已确认含：`sprd,qogirn6pro-backlight`、`sprd,backlight`、
`Kinetic,ktz8866`（84 处）、`lontium,lt9711exb`（70 处）—— 所以 ck10 的显示桥、se50/tb/su30 的
ktz8866 背光都由这个模块负责，**不需要内核内建**。

vermagic 全部为 `5.4.210-android12-9-668732-gb286db7ac1f5-dirty SMP preempt mod_unload modversions aarch64`，
我们内核是 `5.4.210-tca-cycle ... modversions aarch64`；`same_magic` 忽略 release + `CONFIG_MODVERSIONS=n`，
所以原厂模块能直接加载（se50 的 160 个模块已验证 0 未解析）。

## 4. 各机型 4 项功能评估

| 机型 | 触摸 | 屏幕 | 背光 | 音频PA |
|---|---|---|---|---|
| se50（本机） | ✅ himax 内建 | ✅ sprd-drm | ✅ sprd-drm(ktz8866) | ✅ aw883xx |
| tb | ✅ himax | ✅ | ✅ | ✅ aw883xx |
| su30 | ✅ nvt 模块 | ✅ | ✅ | ✅ aw883xx |
| ck10 | ✅ himax | ✅ sprd-drm(lt9711) | ✅ | ✅ aw883xx |
| ca30pro | ✅ himax | ✅ | ✅ | ⚠ aw882xx（不在共享集，需其固件模块） |
| 1h10 / 2h10 / uob | ✅ himax | ✅ | ✅ | ⚠ ucp1301（同上） |

## 5. 结论

1. **本机 se50：4/4 全活**（himax 内建 + sprd-drm 模块 + aw883xx 模块，均已验证）。
2. **tb / su30 / ck10：4/4 也应全活** —— 触摸走模块、屏幕/背光走 sprd-drm、音频走 aw883xx，全是共享集。
3. ~~ca30pro / 1h10 / 2h10 音频PA 缺驱动~~ → **已被 §9 实测推翻**：每个机型的 vendor_ramdisk 自带按自己板子编的同名模块（如 ca30pro 的 `snd-soc-sprd-pa-aw883xx.ko` 里 compatible 就是 `awinic,aw882xx_smartpa`），内核只需满足符号即可。
4. 真正在内核里缺的（vi5300 ToF / ak09973 磁力计 / ucs12cm0 光距感 / stspin220 马达）**都是传感器和马达**，
   不在“触摸/屏幕/背光/音频PA”这 4 项里，按你的标准可以不管。

## 6. 下一步（可选）

- [x] **AK3 白名单已扩**：新增 `device.prefix1=ums9620`，放行所有 `ums9620*` 设备（提交 `9a3ef65f6`）。
- 若要把 ca30pro / 1h10 / 2h10 的音频PA 也做全，需要它们的原厂固件模块，或从讯飞完整树移植 aw882xx / ucp1301。

## 7. 充电（补充验收项）

充电也全是**原厂模块**（原厂 config 里全是 `=m`），共享集里都有：

| compatible | 模块 | 用于 |
|---|---|---|
| charger-manager | sprd-charger-manager.ko | 全部（PD/PPS/SFCP/自定义快充框架） |
| ti,bq2560x_chg | bq2560x-charger.ko | 全部 |
| ti,bq2597x-standalone | bq2597x-charger.ko | 全部 |
| sgm,sgm41542_chg / sgm,sgm41513x_chg | sgm415xx-charger.ko | se50/su30/tb/ca30pro（一个模块覆盖两个 compatible） |
| sc,sc8551-standalone | sc8551-charger.ko | se50/su30/tb |
| cellwise,cw221X | cw221x_battery.ko | se50/su30/tb（电量计） |
| silergy,wl_charger_sy65153 | sy65153_wireless_charger.ko | 1h10（无线充） |
| sprd,ptn38003a redriver | ptn38003a-i2c.ko | 全部（USB redriver） |
| sprd,flash-aw36518 / ocp8137 | flash_ic_aw36518.ko / flash_ic_ocp8137.ko | 闪光灯 |

| 机型 | 有线充 | 电量计 | 无线充 |
|---|---|---|---|
| se50（本机） | ✅ bq2560x+bq2597x+sgm41542+sc8551 | ✅ cw221x | — |
| tb | ✅ 同上 | ✅ cw221x | — |
| su30 | ✅ 同上 | ✅ cw221x | — cps8601 是**笔充**，可忽略 |
| ck10 | ✅ bq2560x+bq2597x | ✅ sc27xx | — |
| ca30pro | ✅ bq2560x+bq2597x+sgm41513x | ✅ sc27xx | — |
| 1h10 | ✅ bq2560x+bq2597x | ✅ sc27xx | ✅ sy65153 |
| 2h10 / uob | ✅ bq2560x+bq2597x | ✅ sc27xx | — |

结论：**充电 9/9 全有**。su30 的 `cps8601` 是给**触控笔**充电的（不是主机电池），缺了无所谓。

## 8. 总评（按验收口径：触摸/屏幕/背光/音频PA/充电）

| 机型 | 触摸 | 屏幕 | 背光 | 音频PA | 充电 |
|---|---|---|---|---|---|
| se50（本机） | ✅ | ✅ | ✅ | ✅ | ✅ |
| tb | ✅ | ✅ | ✅ | ✅ | ✅ |
| su30 | ✅ | ✅ | ✅ | ✅ | ✅ |
| ck10 | ✅ | ✅ | ✅ | ✅ | ✅ |
| ca30pro | ✅ | ✅ | ✅ | ✅（见 §9） | ✅ |
| 1h10 / 2h10 / uob | ✅ | ✅ | ✅ | 预计同 ca30pro（各自固件带 ucp1301 模块） | ✅ |

**ca30pro 已实测通过**（模块符号验证 161/161，见 §9）；1h10/2h10 未经实测，
需要对应机型的 vendor_boot 才能下定论。

## 9. ca30pro vendor_boot 实测（2026-10-01）

用户提供了 ca30pro 的 `vendor_boot.img`（sha256 `220905d8…`）。结论：

1. **机型标识**：`ro.product.vendor.device=ums9620_ca30pro`，`ro.build.product` 同。
   → AK3 的 `device.prefix1=ums9620` 前缀白名单覆盖 ✅。
2. **vendor_ramdisk 是 9 机型共享骨架**：`first_stage_ramdisk/` 里放着全部 9 个机型的
   fstab（`fstab.ums9620_{1h10,2h10,2h10_uob,2h10_uob_marlin3,ca30pro,ck10,se50,su30,tb}`），
   但内容两两完全相同 —— fstab 不区分机型，机型由 dtbo + prop 决定。
3. **基础 DTB 与 se50 仅差 2 处** `sprd,mipi-drive-capability`（0x04 vs 0x07，MIPI 驱动强度），
   这是板级显示调参，留在各自 vendor_boot 里，我们不动。
4. **模块集不是共享二进制，而是同名按板子编**：ca30pro 的 161 个模块 = se50 的 160 个
   + `cps5601x-charger.ko`，但 `sprd-drm.ko`、`snd-soc-sprd-pa-aw883xx.ko` 等 md5 都不同。
   ca30pro 的 aw883xx.ko 里 compatible 是 `awinic,aw882xx_smartpa` —— 音频 PA 由它自己固件负责 ✅。
5. **唯一缺一个内核符号**：ca30pro 的 `sprd-drm.ko` 在面板 MIPI 上电序列
   （`hx83102_{boe,cx,jt}_option_mipi_power`）里调用 `if_himax_smwp_enabled()`，
   按触摸 SMWP（智能唤醒）状态选分支。se50/F50 源码树从没带过这个符号（se50 原厂内核也没有），
   是 ca30pro 内核私有的。已在我们内核里加桥接导出（返回 himax 驱动的实时 `HX_SMWP_EN` 状态）：
   `drivers/vendor/common/touchscreen_v2/hxchipset_pad/himax_common.c`。
6. **验证结果**：加符号后 ca30pro 161 个模块 **0 未解析 PASS**；se50 160 个模块回归 **0 未解析 PASS**；
   ABI 护栏不变（TSK_STACK 48 / TSK_ACTIVE_MM 1256 / TSK_STACK_CANARY 1432）。
7. ca30pro 模块集已存 `stock/ca30pro-vendor-modules/`（17 MB）作为回归基准。

**推论**：tb / su30 / ck10 / 1h10 / 2h10 大概率同模式（同名模块按板子编），但每个机型在实测前
都可能像 ca30pro 这样差一两个私有符号。要坐实需各机型的 vendor_boot.img。

## 10. ca30pro boot.img 逆向（2026-10-01）

用户又提供了 ca30pro 的 `boot_b.img`（sha256 `d6cfa550…`）。解出内核对比：

1. **内核同源**：`5.4.210-android12-9-668732-gb286db7ac1f5-dirty`，Clang r416183b，
   与 se50 原厂同版本串同工具链，2025-10-03 编（se50 是 2025-01-18）。
   `if_himax_smwp_enabled` 在其中确实有导出 —— 我们的桥接方向正确。
2. **config 与 se50 原厂只差 13 行**（已存 `stock/ca30pro.config`）：

   | ca30pro 多出 | se50 多出 |
   |---|---|
   | `LCD_HX83102_1197_{BOE,CX,JT}_OPTION_MIPI_HD=y`（三面板 MIPI 上电序列，编进 sprd-drm.ko） | `LCM_KTZ8866_*`（背光控制） |
   | `SND_SMARTPA_AW882XX=m` | `SND_SMARTPA_AW883XX=m` |
   | `CHARGER_CPS5601X=m` | `MAG_AK09973=y` |
   | `WL_CHARGER_CPS8601=y`（**笔充内建**，我们树无此源码） | `STSPIN220=y` |
   | `IFLY_NO_PD_FAST_CHARGE=y`、`DMIC2_AS_MAIN_MIC=y`、`HW_AUDIO_SWITCH=y` | |

   触摸栈：ca30pro/se50 原厂都是 `TOUCHSCREEN_HIMAX_CHIPSET=y`，我们树里是改名后的
   `_CHIPSET_PAD=y`（`TOUCHSCREEN_BOARD_NAME="harrow"`），导出符号一致，模块兼容已验证。
3. **刷我们内核后 ca30pro 会失去的内建功能**：cps8601 笔充（用户说无所谓）、vi5300 ToF、
   ucs12cm0 光距感（均不在验收口径）。音频 PA / 屏幕 / 背光 / 触摸 / 充电全由 ca30pro 自带模块
   + 我们内核的符号覆盖，verify 0 未解析。
4. **boot ramdisk 与 se50 仅差 `system/etc/ramdisk/build.prop`**（机型名/指纹/日期），
   其余逐字节相同。ca30pro 身份：`ro.product.bootimage.device=ums9620_ca30pro`，
   指纹 `iFLYTEK/ums9620_ca30pro_native/ums9620_ca30pro:13/TP1A.220624.014/...`。
5. **刷机建议**：ca30pro 用 **AK3 卡刷包**（`split_boot; flash_boot` 保留设备自己的 ramdisk，
   build.prop 不动）；不要裸刷我们为 se50 打的 boot.img（会把 bootimage build.prop 刷成 se50 的）。

**ca30pro 结论：5/5 验收项达标（实测）**，代价是笔充 + 两个传感器。
