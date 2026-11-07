# iFLYTEK ums9620_se50 平板 —— 移植笔记

## 设备
- 机型：iFLYTEK `ums9620_se50_s30pro`，board `ums9620_se50`，Android 13 (`TP1A.220624.014`)
- 内核：`Linux 5.4.210-android12-9-668732-gb286db7ac1f5-dirty`，clang 12.0.5 (r416183b)
- boot：Android boot **v4**，kernel 为 raw Image，GKI 通用 ramdisk，带 VBMETA；**boot.img 内无 DTB**
- 源码底座：`kernel_ums9620_se50`（原厂 F50 同 BSP，5.4.210，同一 clang）

## 工具链（本机已有）
AOSP clang r416183b：
```
/home/misakapardola/leaking-iflytek/recovery_twrp/prebuilts/clang/host/linux-x86/clang-r416183b/bin
```
（recovery_shrp、rksdk 下也有同版本副本）

## 展锐 DTB 处理（重要）
紫光的 DTB 镜像外面套了一层专有头，**必须先去掉头**才能得到裸 DTB。
- 头 magic：`SPRD`（4 字节，0x44525053）
- 去头/解包：`sprd_dt <sprd_dt_image>`  → 吐出 `<a>_<b>_<c>.dtb`
- 反向打包（加头）：`sprd_dtbtool -o <out> <dtb目录>`，选项 `-p` dtc 路径、`-s` page size
- 工具位置：`/home/misakapardola/leaking-iflytek/sprddt/`（已 chmod +x）
- 之后：`dtc -I dtb -O dts <裸dtb>` 反编译看 model/compatible

## DTB 去哪找
不在 boot.img，也不在现有文件夹里。在设备分区，按可能性：
1. `vendor_boot`（GKI v4 的 DTB 在这里）
2. `dtbo`（overlay）
3. `dtb`（部分机型单列）
4. `uboot_a/b`（少数拼接）
dump：`spd_dump`（`/home/misakapardola/leaking-iflytek/spddump/spreadtrum_flash/spd_dump`），或设备上 `dd if=/dev/block/by-name/<part>`。

## 模块（.ko）
- 位置：`tcakernel/modules/`，共 **134** 个，**未签名**
- vermagic（全部一致）：
  ```
  5.4.210-android12-9-668732-gb286db7ac1f5-dirty SMP preempt mod_unload modversions aarch64
  ```
- 原厂 config：`CONFIG_MODVERSIONS=y`，`CONFIG_MODULE_SIG` 未开，`CONFIG_MODULE_FORCE_LOAD` 未开
- 原厂 `module_layout` CRC = `0x2f279e7b`（复用模块的关键指纹）
- 含各 se50 专属驱动：`cw221x_battery.ko` `sgm415xx-charger.ko` `snd-soc-sprd-pa-aw883xx.ko` `sprd_sensor.ko` 等

## 已做实验
| 构建 | module_layout CRC | 结论 |
|---|---|---|
| GCC 15 + se50 原厂 config（无 LTO/CFI） | `0x79fa48f8` | 与原厂不符 → 原厂 .ko 不能直接加载 |
| clang r416183b + se50 原厂 config（LTO/CFI） | 构建中 | 待确认 |

vermagic 已能完全复现正确。

## 复用原厂 .ko 的判定
内核 `CONFIG_MODVERSIONS=y` 时加载模块会逐符号比对 `__versions` CRC，`module_layout` 是闸门。
- CRC 一致 → 直接复用
- CRC 不一致 → 要么同源码重编模块，要么关 `MODVERSIONS` + 改 .ko 的 vermagic（去掉 `modversions `），要么拿 se50 原厂源码

## DTB 提取结果（全部到手）
- \`dtb_a.img\`：全 0（空分区）
- \`dtbo_a.img\`：DTBO 镜像，9 个板型 overlay；**entry6 = \`sprd,ums9620-se50\`**（本平板）
  - 其他：1h10 / 2h10 / 2h10_uob_marlin3 / 2h10_uob / ca30pro / ck10 / su30 / tb
- \`vendor_boot_a.img\`：VNDRBOOT v4，page 4096
  - cmdline: \`console=ttyS1,115200n8 buildvariant=user\`
  - vendor_ramdisk 29094608 B
  - DTB 段 = DTBO（1 条）= **base DTB**，161076 B，\`compatible = "sprd,ums9620"\`
- 产物：\`base.dtb\`/\`base.dts\`（base）+ \`se50.dtb\`/\`se50.dts\`（board overlay）
- 注：这两处都是标准 DTBO（magic \`d7b7ab1e\`），**没有** SPRD 专有头；\`sprddt\` 那套用于带 SPRD 头的 dt 镜像

## base.dts vs 仓库 DTS 结构对比（子 agent，已核实）
- **真正的 SoC base 是 `qogirn6pro.dtsi`(94KB)**，`ums9620.dtsi`(26KB) 只是包装（CPU/GIC/timer/pmu/gsp/pll/coresight）。
- 结论：**同一套 UMS9620/QogirN6Pro**，匹配度极高
  - 节点：base 954 / 源码 939；带 compatible：403 / 399
  - compatible+reg 冲突 **0**，仅源码节点 **0**
  - /soc 顶层 97 个子节点**顺序逐项一致** → probe 顺序无差异
- 设备 base 独有（4 处 compatible）：**DSI1 host+phy(0x31400000)**、**sensorhub**、**ise_med 保留内存**
  - DSI1/sensorhub 在 se50 overlay 里**未被引用** → 平板大概率单屏、基础 bring-up 不需要
  - `&ise_med_reserved` 仅出现在 harrow/coconut/unicorn/tulip/staple 子目录 overlay 副本，且无 label 定义；这些不参与顶层 dtbs 构建 → 潜在隐患，非当前阻塞
- 属性级小差异：serial 的 source-base、dsi/dphy 的 dual-channel、dptx status、hsphy status、LDO_VDDLDO2 always-on、PMIC efuse calib 偏移、vbc 3 个 pin group

## DSI1（第二路 DSI/双通道屏）移植 —— 已补机械部分
已改文件（git diff，+66 行）：
- `arch/arm64/boot/dts/sprd/qogirn6pro.dtsi`：新增 `dsi1@31400000`(`sprd,qogirn6pro-dsi1-host`) 与 `dphy1`(`sprd,qogirn6pro-dsi1-phy`)，并给 dsi/dphy 加 `sprd,dual-channel`
- `arch/arm64/boot/dts/sprd/ums9620.dtsi`：`&dsi1 { clocks = <&dpu_vsp_gate CLK_DSI1_EB>; }`
- `drivers/gpu/drm/sprd/sprd_dsi.h/.c`：新增 `qogirn6pro_dsi_s`（用已有的 `qogirn6pro_dsi_s_glb_ops`）+ `sprd,qogirn6pro-dsi1-host` 匹配
- `drivers/gpu/drm/sprd/sprd_dphy.c`：新增 `sprd,qogirn6pro-dsi1-phy` 匹配
验证：`make dtbs` 通过，生成的 `ums9620.dtb` 含 dsi1/dphy1；`sprd-drm.o` 编译 0 error。

### 重要限制（未解决）
1. **se50 原厂 config 里 `CONFIG_DRM_SPRD is not set`**，但设备实际加载 `sprd-drm.ko`（模块）。→ 显示驱动是模块，要生效需以 `CONFIG_DRM_SPRD=m`（vendor config）重编该模块。
2. **整个仓库没有任何代码读 `sprd,dual-channel`** → master 侧双通道逻辑缺失。DSI1 节点会 probe，但 master 不会驱动 slave 通道，双通道屏可能仍不亮。需要 iFlytek 的驱动逻辑。
3. 若 se50 实际用**单通道 FHD 屏**，以上都不需要。

## 复用原厂 .ko 的缺口（全量扫描）
- 194 模块中 183 个完全 CRC 兼容；module_layout = 原厂 0x2f279e7b
- 唯一不兼容：power_supply_*（11 个充电类）→ 讯飞改过 power_supply
- 内核必须额外提供的符号只有 2 个：
  1. himax_int_enable（原厂 有，开 CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_V3_3 即导出）
  2. himax_resume_by_lcd（原厂 完全没有，必须新增+EXPORT_SYMBOL，否则 sprd-drm.ko 加载失败）
- 调用点：sprd-drm.ko kthread_lcd_resume_tp_func@0x13c4c -> himax_resume_by_lcd()（无参）

## 触摸（Himax）可行性 —— 已验证可编
- se50 用 Himax HX83102J（SPI，himax,hxcommon），原厂 树有驱动 hxchipset_v3_3
- 原厂 触摸目录由 CONFIG_TOUCHSCREEN_VENDOR_V2 门控，原厂/ f50 config 都没开 → 必须显式打开
- 直接开会导致一堆 原厂 源码陈旧 bug（因为 F50 从没编过触摸）：
  1. himax_common.c input_mt_init_slots 少第 3 个参数 → 已修
  2. himax_common_interface.c 用旧名 g_core_fp（应为 hx_s_core_fp）→ 已全局改名
  3. firmware_config/default 启用 HX_USB_DETECT_GLOBAL 等，但 struct himax_ts_data 没有 charger_wq/charger_work/charger_notifier/edge_limit_level，struct himax_core_fp 没有 fp_mrotation_set/fp_edge_limit_level_set → 用 BOARD_NAME="harrow"（不启用 USB detect）即可编过
- 结果：touchscreen_v2 全部对象编过；himax_resume_by_lcd（S3 精确还原）+ himax_int_enable 均已定义并 EXPORT_SYMBOL
- himax_resume_by_lcd 精确实现（S3 从原厂 Image 用 __ksymtab + CFI 蹦床定位）：
  void himax_resume_by_lcd(void){ I("himax tp resume after lcd rst high
"); if(!hx_s_ts){E(...);return;} himax_chip_common_resume(hx_s_ts);} EXPORT_SYMBOL(...)
