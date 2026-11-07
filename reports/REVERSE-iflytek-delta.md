# 讯飞 iFLYTEK「se50 内核」相对 原厂 F50 源码的改动清单（REVERSE）

> 目标：超越 CRC 符号签名，用**模块符号指纹 + 模块独有字符串 + 内核导出符号表 + 配置**找出讯飞相对 `kernel_ums9620_se50` 的**行为级/结构级**改动。
> 结论分级：**确定** = 有字节/符号/源码/配置的直接证据；**推测** = 证据指向但未逐字节确认。
> 只读分析，本文件为唯一写入产物。不编造。

---

## 0. 分析对象与方法

| 对象 | 路径 | 说明 |
|---|---|---|
| 原厂 参考源码 | `kernel_ums9620_se50/`（git `1e79a67b7b55e65cf78867827096a8e7cee21ce8`） | F50 MiFi，同 BSP 5.4.210 |
| 讯飞原厂内核镜像 | `boot/unpack/kernel`（38 840 832 B，sha256 `feaf3b86744cb193…`） | 从 `tcakernel/boot/boot.img` 用 magiskboot 解出，raw aarch64 Image |
| 讯飞原厂模块 | `vendor_rd/lib/modules/`（160）、`modules/`（134） | 去重后 **194** 个 .ko；**未 strip**，含完整 .symtab |
| 本地 原厂 构建 | `se50-clang-out/`（同 clang r416183b；`Module.symvers` 12199 条） | 用于导出符号对照 |
| 原厂 config | `boot/unpack/kernel` 内嵌 IKCONFIG（`scripts/extract-ikconfig` 提取，6394 行） | 与 `se50-clang-out/.config` 对比 |

### 方法要点（重要，供复核）
1. **模块符号指纹**：用 `nm` 取每个 .ko 的 **已定义函数符号**（T/t/W/w），剥离 `.isra/.constprop/.part` 后缀；与「全 原厂 源码标识符全集」（对 `drivers sound net fs kernel mm block` 的 *.c/*.h/*.S 取 3 452 713 个唯一标识符，`/tmp/vendor_ids.txt`）求差。差集 = 该模块内**多出的函数**（原厂 全树都找不到该名字）。
2. **内核导出符号 diff**：直接从 raw Image 解析 `__ksymtab`（24 字节 `{value, name, ns}`，绝对指针）。**关键坑：内核 `CONFIG_CFI_CLANG=y`，ksymtab 的 value 指向 CFI 跳转蹦床（4 字节 `b real_func`），不是函数本体**；real 地址 = 蹦床地址 + 符号扩展(imm26<<2)。vaddr↔文件偏移用 stock 自身 `__ksymtab_strings` 校准（`file_off = vaddr - 0xffffffc010080000`，用 1.2 万个符号名交叉验证）。
3. **配置 diff**：stock IKCONFIG vs 原厂 `se50-clang-out/.config`。
4. 字符串/兼容性字符串：`strings` + 对 原厂 全源码/DTS 语料反查。

> 产物校验：stock 导出符号 12214 条、原厂 `Module.symvers` 12199 条，规模一致，diff 可信。

---

## 1. 结论速览

| # | 改动 | 分级 | 关键证据 |
|---|---|---|---|
| D1 | **显示子系统大改**：`sprd-drm.ko` 新增 KTZ8866 背光、LT9711EXB 桥、3 组面板驱动、双 DSI、DPU 色彩/FPS 控制 | **确定** | 模块内 62 个 原厂 全树不存在的函数 + `iflytek`/`jwchen14` 字符串 + 新 config |
| D2 | **触摸新增 `himax_resume_by_lcd`（导出，void 无参）**，LCD 复位后由 sprd-drm 的 kthread 调用 | **确定** | 内核 ksymtab 导出 + 反汇编还原 + 原厂 全树无此名 |
| D3 | **触摸新增 `if_himax_int_enabled`（导出）** | **确定** | stock ksymtab 有、原厂 源码无 |
| D4 | **触摸驱动为另一版本**：讯飞用 `CONFIG_TOUCHSCREEN_HIMAX_CHIPSET`（HX83102J），原厂 源码是 `TOUCHSCREEN_HIMAX_CHIPSET_V3_3` | **确定** | 两边 config 符号名不同，原厂 Kconfig 无前者 |
| D5 | **新增 `CONFIG_TP_RESUME_BY_LCD`** 特性（即 D2 的开关），原厂 Kconfig 无 | **确定** | stock config `=y`，原厂 全树无此 Kconfig |
| D6 | **充电/电量计改动**（已知 11 模块 CRC 不匹配是结果，根因是 `power_supply` 结构/接口改过） | **确定（CRC）**/**推测（具体实现）** | 见既有 `REUSE-STOCK-KO-REPORT.md`；另 stock config 新增 `CHARGER_SGM415XX=m`、`FUEL_GAUGE_CW221X=m`、`TYPEC_MUX_WAS4780=m` |
| D7 | **大量 se50 专有硬件/驱动**（充电、面板、传感器、Camera/NPU/GPU/Vdsp/音频），原厂 F50 树根本没有源码 | **确定** | 58 个模块在 原厂 树无对应 Makefile；新 Kconfig；兼容性字符串 `cellwise,cw221X`、`sgm,sgm41513_chg`、`lontium,lt9711exb`、`Kinetic,ktz8866` |
| D8 | **给多个已存在驱动加 debugfs/sysfs 调试接口** | **确定** | 见 §3；dwc3/musb/pcie/tcpm/regulator/seth/ufs/wcn_bsp 等 |
| D9 | stock 不导出 `usb_stor_*`（原厂 导出），但两边 `CONFIG_USB_STORAGE=y` | **推测** | `__ksymtab` 无 `usb_stor_*`，但内核内有其代码字符串；疑 iFLYTEK 去掉了 `EXPORT_SYMBOL_GPL` 或源码有分歧 |
| D10 | 内核启用了 `CONFIG_COMPAT_VDSO` / `GENERIC_COMPAT_VDSO`、`LOCALVERSION_AUTO` | **确定** | config diff |

**未改动（好消息）**：`sprd-gsp.ko`、`sdhci-sprd.ko`、`clk-sprd.ko`、`sipa-core.ko`、`dwc3-sprd.ko` 的**所有**函数名均能在 原厂 全树找到（差集 = 0）——显示 GSP / 存储 SDHCI / 时钟 / SIPA / dwc3-sprd 主体没有行为级新增。

---

## 2. 模块清单 delta

- 设备模块去重 **194** 个（`vendor_rd` 160 + `modules` 134，交集 100）；其中 **136** 个能在 原厂 树找到对应 Makefile/源码目录，**58** 个找不到（下表）。
- 原厂 树的 Makefile 映射失败的 58 个 = 「原厂 F50 树不产出/无源码」的候选。其中一部分是「一个 .ko 由多目录多 .o 合成」导致按名映射失败（如 `snd-soc-*`, `sprd-*`），但经源码目录核对，下列类别确实 **原厂 树无源码**：

**确定 原厂 树无源码的整类驱动（F50 MiFi 不需要）**
- Camera/影像：`sprd_camera.ko`, `sprd_cpp.ko`, `sprd_camsys_pw_domain.ko`, `jpg.ko`
- NPU/VPU/Vdsp/GPU：`npu_img_vha.ko`, `vpu.ko`, `sprd-vpu-pw-domain.ko`, `sprd_vdsp.ko`, `mali_kbase.ko`, `mmdvfs.ko`
- 传感器/指纹/马达：`sprd_sensor.ko`, `microarray_fp.ko`, `mcdt_hw_r2p0.ko`, `stspin220`(config)
- 触摸（非 Himax）：`focaltech_ft8756_spi_ts.ko`, `nvt_nt36xxx_spi_ts.ko`, `nvt_touch_host.ko`, `synaptics_td4320_spi_ts.ko`
- 充电/背光/杂项：`cw221x_battery.ko`, `sgm415xx-charger.ko`, `was4780-i2c.ko`, `flash_ic_aw36518.ko`, `flash_ic_ocp8137.ko`
- 音频新链路：`snd-soc-sprd-card.ko`, `snd-soc-sprd-codec-ump9620*.ko`, `snd-soc-sprd-pdm-r2p0.ko`, `snd-soc-sprd-tdm.ko`, `snd-soc-sprd-vbc*.ko`, `snd-soc-sprd-pa-aw883xx.ko`, `sprd_vdsp` 等
- 其它：`wcn_bsp.ko`, `sprdbt_tty.ko`, `sprd_fm.ko`, `voice_trigger_irq.ko`, `zsmalloc.ko`(配置差异)

**配置新增的 se50 专有能力（stock 有，原厂 树 Kconfig 完全没有该符号）** —— 确定：
`TP_RESUME_BY_LCD`, `LCM_KTZ8866_BACKLIGHT_CURRENT_CONTROL`, `LCM_KTZ8866_SUSPEND_CONTROL`, `LCM_DPU_CRTC_STATE_CONTROL`, `LCM_PQCM_CAUSE_NO_DISPLAY`, `LCM_REDUCE_DPU_UNDERFLOW_CONTROL`, `LCM_MP`, `CHARGER_SGM415XX`, `FUEL_GAUGE_CW221X`, `DTOF_VI5300`, `MAG_AK09973`, `SENSORS_UCS12CM0`, `STSPIN220`, `HW_AUDIO_SWITCH`, `USB_SPRD_LINKFIFO`, `TYPEC_MUX_WAS4780`, `TOUCHSCREEN_HIMAX_CHIPSET`。

---

## 3. 符号指纹：模块内多出的函数（`nm` 定义符号 − 原厂 全树标识符）

对 194 个模块逐一求差。下表列出**原厂 树有该驱动源码、但模块里多出函数**的模块（=真正的源码级改动；整类无源码的见 §2）。

| 模块 | 多出函数数 | 多出的函数名 |
|---|---|---|
| `sprd-drm.ko` | **62** | 见下 |
| `sprd_usb_f_rndis.ko` | 14 | `rndis_opts_{class,dev_addr,host_addr,ifname,protocol,qmult,subclass}_{show,store}`, `rndis_attr_release`（新增 RNDIS 参数 sysfs） |
| `dwc3-sprd-core.ko` | 10 | `dwc3_{descriptor_fetch_queue,ep_info_register,event_queue,rx_fifo_size,rx_info_queue,rx_request_queue,transfer_type,trb_ring,tx_fifo_size,tx_request_queue}_open`（新增 dwc3 debugfs） |
| `wcn_bsp.ko` | 9 | `edma_debug_info_show/_save_for_mbuf`, `edma_pending_irq_check`, `pcie_debug_point_show`, `pcie_runtime_put`, `sprd_pcie_debug_point_show`, `sprd_pcie_fw_push_cancel`, `wcn_firmware_ready_close`, `wcn_set_powerdown_flag` |
| `musb_hdrc.ko` | 2 | `musb_linknode_clear`, `musb_regdump_open`（新增 musb 寄存器 dump） |
| `ump9620-regulator.ko` | 2 | `fops_enable_open`, `fops_ldo_open`（新增 debugfs） |
| `ump9621-regulator.ko` | 2 | 同上 |
| `pcie-sprd.ko` | 1 | `sprd_pcie_link_up` |
| `sc8551-charger.ko` | 1 | `sc8551_deinit_adc` |
| `seth.ko` | 1 | `fops_gro_enable_open` |
| `sipc-core.ko` | 1 | `sbuf_copy_from_user`（原厂 仅在 `include/linux/sipc.h` 声明） |
| `spool.ko` | 1 | `sbuf_copy_from_user` |
| `sprd-charger-manager.ko` | 1 | `cm_sw_fast_charge_algo` |
| `sprd_tcpm.ko` | 1 | `sprd_tcpm_debug_open` |
| `ufs-sprd-qogirn6pro.ko` | 1 | `ufs_sprd_setup_clocks` |

> 说明：差集为 0 的模块（含 `sprd-gsp`、`sdhci-sprd`、`clk-sprd`、`sipa-core`、`dwc3-sprd`、`trusty-ipc`、`sprd-cpufreq-*`、`sc27xx_*` 主体等）**没有新增函数名**，但注意：**函数名相同 ≠ 实现相同**，本方法只能抓「新增/删除符号」，抓不到「同名函数内部被改」。因此下文对高风险子系统给出更强证据（字符串/配置/反汇编）。

### `sprd-drm.ko` 多出的 62 个函数（显示子系统，重点）
- **KTZ8866 背光**（Lontium/Kinetic KTZ8866，两路）：`ktz8866_1/2_{init,exit,probe,remove,config_node_show,resume_reg_config,suspend_reg_config}`；导出 `ktz8866_1/2_{resume,suspend}_reg_config`；字符串 `Kinetic,ktz8866_1/2`、`alias=of:N*T*CKinetic,ktz8866_*`、**`iflytek ktz8866_1_probe success end` / `iflytek Lontium KTZ8866_* driver exited`**。
- **LT9711EXB 桥**（Lontium）：`lt9711exb_{init,exit,probe,remove}`、`lt9711_{block_erase,firmware_upgrade,firmware_write,flash_read_addr_set,flash_write_config,fw_read_back,fw_read_crc,get_version,node_show,screen_node_show}`；字符串 `lontium,lt9711exb`、`jwchen14 Lontium LT9711EXB driver installing....`。
- **面板专用**（每面板 mipi_init/power/uninit/power_init/get_backlight_delay_time）：`hx83102_boe_option_*`（屏 HX83102 BOE）、`kd143n00_nh1_option_*`（KD143N00 NH1）、`nv160wum_nh0_option_*`（NV160WUM NH0）、`dummy_mipi_*`/`dummy_power_init`。
- **DPU/双 DSI/色彩/FPS**：`sprd_dual_dsi_parse_dt`、`dpu_cm_set`、`dpu_get_gsp_base`、`dpu_r6p0_glb_enable`、`dpu_resume_reset`、`dpu_suspend_reset`、`parse_ctm`、`colorMatrix_multi`、`sprd_dpu_atomic_get_property`、`sprd_find_crtc_from_index`、`sprd_check_crtc_active_state`、`sprd_panel_backlight_work_func`、`panel_fps_show`、`actual_fps_store`。
- **触摸-显示联动**：`kthread_lcd_resume_tp_func` —— 内核线程，调用 `himax_resume_by_lcd`（见 §5）。

> 与之呼应的 stock config：`LCM_KTZ8866_BACKLIGHT_CURRENT_CONTROL=y`、`LCM_KTZ8866_SUSPEND_CONTROL=y`、`LCM_DPU_CRTC_STATE_CONTROL=y`、`LCM_PQCM_CAUSE_NO_DISPLAY=y`、`LCM_REDUCE_DPU_UNDERFLOW_CONTROL=y`、`LCD_HX83102_1197_BOE_OPTION_MIPI_HD`、`LCD_MIPI_ROTATION_180`。
> **对照：`sprd-gsp.ko` 393 个函数差集为 0** —— GSP（GPU/显示合成）没动。

---

## 4. 高风险子系统抽查结论

| 子系统 | 模块 | 结论 | 依据 |
|---|---|---|---|
| display DRM/DPU | `sprd-drm.ko` | **大改** | 62 新函数 + iflytek/lt9711/ktz8866/面板字符串 + 7 个新 LCM config |
| display GSP | `sprd-gsp.ko` | 未改 | 差集 0 |
| storage MMC | `sdhci-sprd.ko` | 未改 | 差集 0（71 函数全在 原厂 树） |
| storage UFS | `ufs-sprd-qogirn6pro.ko` | 小改 | +`ufs_sprd_setup_clocks` |
| clock | `clk-sprd.ko`, `ums9620-clk.ko` | 未改 | 差集 0 |
| PMIC/regulator | `ump9620/9621-regulator.ko` | 小改（debugfs） | +`fops_enable_open`/`fops_ldo_open` |
| fuel gauge/charger | 11 个模块 | **CRC 不匹配**（已知），源码新增 cw221x/sgm415xx/was4780 | `REUSE-STOCK-KO-REPORT.md` + 新 config + 新模块 |
| touch | Himax | **新增 2 个导出 + 另一版本驱动** | 见 §5 |
| USB gadget | `sprd_usb_f_rndis.ko` | 中改 | +14 个 rndis sysfs 属性 |
| USB dwc3 | `dwc3-sprd-core.ko` | 中改（debugfs） | +10 个 queue debugfs；`dwc3-sprd.ko` 主体未改 |
| USB musb | `musb_hdrc.ko` | 小改 | +2 debugfs |
| USB typec | `sprd_tcpm.ko` | 小改 | +`sprd_tcpm_debug_open`；`sprd_typec_displayport.ko` 差集小 |
| modem/sipa | `sipa-core.ko` | 未改 | 差集 0（553 函数） |
| modem/sipc | `sipc-core.ko` | 小改 | +`sbuf_copy_from_user`；`sipa-dele/sipa-sys/sipa_usb` 差集小 |
| WCN/无线 | `wcn_bsp.ko` | 小改 | +9 debug/pcie 函数 |
| wlan combo | `sprd_wlan_combo.ko` | 差集 72（原厂 树无源码，整类差异） | §2 |

---

## 5. 【重点】himax_resume_by_lcd 完整还原（确定）

### 5.1 事实
- 原厂 `sprd-drm.ko` 有未定义符号：`U himax_int_enable`、**`U himax_resume_by_lcd`**。
- 该调用点：`sprd-drm.ko` 的 `kthread_lcd_resume_tp_func`@file offset `0x13c4c`（`bl` 到 `himax_resume_by_lcd`，调用前 **未设置 x0**）。
- 原厂 源码全树 grep `himax_resume_by_lcd` = **无**。stock 内核把它作为**导出符号**（`__ksymtab` 命中，定位见 §5.2）。
- 原厂 已有的相关符号：`himax_int_enable(int)`（`himax_platform.c`，已 `EXPORT_SYMBOL`）、`himax_chip_common_resume(struct himax_ts_data*)`（`himax_common.c:3485`，声明 `himax_common.h:612`）、全局 `hx_s_ts`（`himax_common.c:109`，`EXPORT_SYMBOL`）、`himax_resume_proc(bool)`（`himax_ic_core.c:1853`）。

### 5.2 定位（绕开 vmlinux-to-elf 的真实做法）
stock Image 的 `__ksymtab` 项（24 B `{value,name,ns}`）：
- `himax_resume_by_lcd`：value=`0xffffffc01172afb0`（**CFI 蹦床**），name=`0xffffffc012181009` → file `0x2101009`。
- 蹦床反汇编：`0xffffffc01172afb0: 17dbf186 b 0xffffffc010e275c8` → **真函数 vaddr=`0xffffffc010e275c8`，file offset=`0xe275c8`**。
- 旁证 `himax_int_enable`：value=`0xffffffc01172205c` → 蹦床 `b 0xffffffc010e2d934` → 真函数 file `0xe2d934`。

### 5.3 反汇编还原（`0xe275c8`，节选）
```asm
ffffffc010e275c8: str  x30, [x18], #8        ; SCS
ffffffc010e275cc: stp  x29, x30, [sp, #-16]!
ffffffc010e275d0: mov  x29, sp
ffffffc010e275d4: adrp x0, 0xffffffc011d23000
ffffffc010e275d8: add  x0, x0, #0x333        ; = "himax tp resume after lcd rst high\n"
ffffffc010e275dc: bl   0xffffffc01025337c    ; printk / I()
ffffffc010e275e0: adrp x8, 0xffffffc01283a000
ffffffc010e275e4: ldr  x0, [x8, #3336]        ; = hx_s_ts
ffffffc010e275e8: cbz  x0, 0xffffffc010e275fc ; if (hx_s_ts == NULL)
ffffffc010e275ec: bl   0xffffffc010e27614    ; himax_chip_common_resume(hx_s_ts)
ffffffc010e275f0: ldp  x29, x30, [sp], #16
ffffffc010e275f4: ldr  x30, [x18, #-8]!
ffffffc010e275f8: ret
ffffffc010e275fc: adrp x0, 0xffffffc011dca000
ffffffc010e27600: adrp x1, 0xffffffc011daf000
ffffffc010e27604: add  x0, x0, #0x1ae        ; = "[HXTP] %s: Entering! hx_s_ts is NULL\n"
ffffffc010e27608: add  x1, x1, #0xf06        ; = "himax_resume_by_lcd"
ffffffc010e2760c: bl   0xffffffc01025337c
ffffffc010e27610: b    0xffffffc010e275f0
```
子函数 `0xffffffc010e27614` 内日志串确认为 `__func__ = "himax_chip_common_resume"`（另有 inlined `"if_himax_int_enabled"`、`"Already resumed, skip..."`、`"enter"`），与 原厂 `himax_chip_common_resume` 源码流程一致。

### 5.4 等价 C 还原
```c
/* 新增：LCD 复位拉高后由 sprd-drm 的 kthread 调用 */
void himax_resume_by_lcd(void)
{
    I("himax tp resume after lcd rst high\n");
    if (hx_s_ts == NULL) {
        E("%s: Entering! hx_s_ts is NULL\n", __func__);
        return;
    }
    himax_chip_common_resume(hx_s_ts);
}
EXPORT_SYMBOL(himax_resume_by_lcd);
```

### 5.5 给 原厂 源码的最小补丁（可直接加）
1. 在 `drivers/vendor/common/touchscreen_v2/hxchipset_v3_3/himax_common.c`（`himax_chip_common_resume` 之后）加入上面的函数。`hx_s_ts`、`himax_chip_common_resume`、`I()/E()` 宏在该文件均已有。
2. 在 `himax_common.h`（或 `himax_modular.h`）加：
   ```c
   extern void himax_resume_by_lcd(void);
   ```
3. 编进内核（导出到 `Module.symvers`），即可让原厂 `sprd-drm.ko` 解析该符号。
4. **注意**：stock 侧该函数由 `CONFIG_TP_RESUME_BY_LCD=y` 与 `sprd-drm` 的 `kthread_lcd_resume_tp_func` 配套；若还要复刻完整「LCD 复位后唤醒 TP」行为，需在 sprd-drm 侧补 `kthread_lcd_resume_tp_func`（原厂 树无）。若只是让原厂 sprd-drm.ko 能加载，只补导出函数即可。
5. **第二个 kernel 导出缺口**：`if_himax_int_enabled` 也是 stock 新增导出（原厂 源码 grep 不到）。它被 `himax_chip_common_resume` inline 调用；原厂 的 v3_3 驱动里有等价查询逻辑但**函数名不同**，故同样需要新增/导出，否则引用它的模块（若有）也解析不到。

---

## 6. 内核导出符号 diff（stock vs 本地 原厂 构建）

- stock `__ksymtab` 解析出 **12214** 个导出；原厂 `Module.symvers` **12199** 个。
- **stock 多出 42 个**（全部集中在触摸，逐一对 原厂 源码核验后）：
  - **原厂 源码确实没有**（确定新增）：**`himax_resume_by_lcd`**、**`if_himax_int_enabled`**。
  - 原厂 源码已有、但 iFLYTEK 编成内建并导出：`g_cfg_table_hw_opt_crc`、`g_zf_opt_crc`、`g_fw_boot_upgrade_name`、`g_fw_mp_upgrade_name`、`g_mmi_refcnt`、`g_target_report_data`、`g_ts_dbg`、`g_update_cfg_buf`、`g_flag_eb/ec/ed_event`、`Is_2T2R`、`IC_CHECKSUM`、`HX_EXCP_RESET_ACTIVATE`、`HX_PROC_SEND_FLAG`、`CFG_*/CID_*/FW_VER_*_FLASH_ADDR`、`himax_{bus_read,bus_write,debug_init,debug_remove,input_register,int_enable,parse_dt,report_data_init,rst_gpio_set}`、`hx_{parse_assign_cmd,s_core_fp,s_debug_data,s_ic_data,s_ic_setup,s_touch_data,s_ts,touch_proc_dir}`。
- **stock 少 27 个**：全部 `usb_stor_*`（USB 大容量存储）。但两边 `CONFIG_USB_STORAGE=y` 相同、stock 内核中仍有这些代码字符串 ⇒ **推测** iFLYTEK 去掉了 `drivers/usb/storage` 的 `EXPORT_SYMBOL_GPL`（安全收敛），或该文件有源码级差异。**未逐字节确认**。

### 配置级差异（stock 启用、原厂 无该符号）
见 §2；其中与「运行时行为」直接相关的：
- `CONFIG_TP_RESUME_BY_LCD=y`（触摸-显示联动）
- `CONFIG_LCM_KTZ8866_{BACKLIGHT_CURRENT_CONTROL,SUSPEND_CONTROL}=y`（背光）
- `CONFIG_LCM_{DPU_CRTC_STATE_CONTROL,PQCM_CAUSE_NO_DISPLAY,REDUCE_DPU_UNDERFLOW_CONTROL}=y`（DPU）
- `CONFIG_CHARGER_SGM415XX=m`、`CONFIG_FUEL_GAUGE_CW221X=m`、`CONFIG_TYPEC_MUX_WAS4780=m`、`CONFIG_SND_SMARTPA_AW883XX=m`
- `CONFIG_DTOF_VI5300=y`、`CONFIG_MAG_AK09973=y`、`CONFIG_SENSORS_UCS12CM0=y`、`CONFIG_STSPIN220=y`、`CONFIG_HW_AUDIO_SWITCH=y`、`CONFIG_USB_SPRD_LINKFIFO=y`
- `CONFIG_COMPAT_VDSO=y`、`CONFIG_GENERIC_COMPAT_VDSO=y`、`CONFIG_LOCALVERSION_AUTO=y`（后者解释 vermagic 的 `-dirty`）

---

## 7. 对移植的建议动作

1. **必做（解锁原厂模块加载）**：给 原厂 源码加 `himax_resume_by_lcd` 与 `if_himax_int_enabled` 两个导出（§5.5），并启用 Himax。注意 原厂 的 config 符号是 `CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_V3_3`（stock 是 `TOUCHSCREEN_HIMAX_CHIPSET`），需要改名/改 Kconfig 才能被 stock config 命中。
2. **充电**：原厂 11 个充电模块 CRC 不匹配的根因仍是 `power_supply` 结构/接口差异（既有报告）；本报告补充：还新增了 `cw221x_battery`、`sgm415xx-charger`、`was4780-i2c`、`ptn38003a-i2c` 等源码与 Kconfig。
3. **显示**：若沿用原厂 `sprd-drm.ko`，需保证内核导出 `himax_int_enable`/`himax_resume_by_lcd`；若用 原厂 源码重编 `sprd-drm`，必须补 KTZ8866/LT9711/面板/双 DSI/DPU 色彩等 62 个函数与 7 个 LCM config，否则无背光/无显示。
4. **不必动**：`sprd-gsp`、`sdhci-sprd`、`clk-sprd`、`sipa-core`、`dwc3-sprd` 等可放心沿用/重编。

---

## 8. 证据、可复现命令与限制

### 关键证据
- `himax_resume_by_lcd` stock 导出：`__ksymtab` @ file `0x2095c78`，value `0xffffffc01172afb0` → 蹦床 `b 0xffffffc010e275c8`。
- 日志字符串：`himax tp resume after lcd rst high\n`（file `0x1d23333`）、`[HXTP] %s: Entering! hx_s_ts is NULL\n` + `himax_resume_by_lcd`。
- `sprd-drm.ko`：`nm` 62 个函数名 原厂 全树不存在；`strings` 含 `iflytek ktz8866_*_probe success end`、`jwchen14 Lontium LT9711EXB driver installing....`。
- 配置：stock 启用 `TP_RESUME_BY_LCD` 等 17 个 原厂 Kconfig 不存在的符号。

### 复现要点
- 模块差集：`nm <ko> | awk '$2~/^[TtWw]$/{print $3}'` 去后缀 → 与 `/tmp/vendor_ids.txt`（全树标识符）`comm -23`。
- 内核导出：解析 raw Image `__ksymtab`（24 B/项），注意 CFI 蹦床二次跳转。
- config：`scripts/extract-ikconfig boot/unpack/kernel`。

### 限制
1. 符号指纹只能发现**新增/删除名**，**同名函数内部被改不会报**；display/clock/storage 的「差集 0」只说明没有新增函数，不等于二进制相同。
2. 原厂 树本身缺很多 se50 驱动源码（camera/NPU/GPU/音频等），这些「多出函数」是 **BSP 代差**，未必是讯飞自己写的。
3. `__ksymtab` 解析只覆盖**导出**符号；非导出静态函数改动需 kallsyms/逐函数反汇编（未做）。
4. `usb_stor_*`、`sbuf_copy_from_user` 等差异未逐字节确认，标为推测。
5. 未做 stock 内核与本地构建的**全量同名函数反汇编对比**（CFI/LTO 导致重定位噪声大），故「实现级差异」可能仍有遗漏。

---
*生成：REVERSE 分析会话。所有结论均可由上述路径与命令复核。*
