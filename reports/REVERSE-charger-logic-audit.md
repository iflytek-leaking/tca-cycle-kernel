# S5 充电逻辑审计（ABI 之外的运行时校验）

> 背景：`REVERSE-charger-s5-build.md` 证明了 **ABI 可加载**（CRC 全对齐）。本报告进一步核对 **运行时逻辑是否与原厂等价**：DT 绑定名、power_supply 属性集、以及"重编模块与原厂 .ko 的函数差集"。
>
> 结论：**接线与属性集全部对齐；但重编的 charger-manager 丢了 1 个 iFlytek 私有活函数 `cm_sw_fast_charge_algo` 及其 DT 开关 `cm-ifly-charge-control`。** 其余为次要/调试差异。

---

## 1. 接线校验（全部通过）

| 检查项 | DT 要求 | 我们的驱动 | 结果 |
|---|---|---|---|
| 主充电器名 | `cm-chargers = "sgm41513_charger"` | `sgm41513_charger_desc.name = "sgm41513_charger"` | ✅ |
| 电荷泵名 | `cm-charge-pumps = "sc8551-standalone"` | `sc8551 psy_desc.name = "sc8551-standalone"`（role table） | ✅ |
| 电量计名 | `cm-fuel-gauge = "sc27xx-fgu"` | cw221x 与 sc27xx_fuel_gauge 均注册该名（见 §3） | ⚠️ 同名冲突（原厂亦如此） |
| 电池在位判定 | `cm-battery-stat = 2` → `CM_FUEL_GAUGE` | 问 fuel gauge 的 `PRESENT`，cw221x/sc27xx 均支持 | ✅ |

### power_supply 属性集（charger-manager ↔ 驱动）
charger-manager 对 **主充** set：`STATUS / CALIBRATE(启停充) / INPUT_CURRENT_LIMIT / CONSTANT_CHARGE_CURRENT`，get：`STATUS / ONLINE / INPUT_CURRENT_LIMIT / CONSTANT_CHARGE_CURRENT / CALIBRATE`。
- `sgm41513_charger_desc.properties = {STATUS, CONSTANT_CHARGE_CURRENT, INPUT_CURRENT_LIMIT, ONLINE, HEALTH, USB_TYPE, CONSTANT_CHARGE_VOLTAGE_MAX, CALIBRATE}`，set/get 分支覆盖上述全部 ✅。
- charger-manager 对 **电荷泵** set：`CALIBRATE / PRESENT / CONSTANT_CHARGE_VOLTAGE_MAX`，get：`CURRENT_NOW / VOLTAGE_NOW / CONSTANT_CHARGE_VOLTAGE / CONSTANT_CHARGE_CURRENT / ...`。
- `sc8551` set 分支：`CALIBRATE / CONSTANT_CHARGE_VOLTAGE_MAX / PRESENT`，get 分支覆盖 `CURRENT_NOW/VOLTAGE_NOW/CONSTANT_CHARGE_VOLTAGE/...` ✅。
- `PRESENT` 只在 set 分支（无 get）——与原始 原厂 `sgm41511-charger.c` 一致；且 `cm-battery-stat=2` 不查主充 PRESENT，故无影响 ✅。

### DT 属性覆盖
se50 charger-manager 节点的标准属性（`cm-chargers/charge-pumps/fuel-gauge/charge-voltage-max/fast-charge-*/flash-charge-*/cp-*-threshold/cp-taper-current/cap-remap-table/thermal-zone/...`）原厂 `charger-manager.c` 全部解析；`sprd,support-fchg / support-pd-pps / pd-fixed-max-microwatt / pd-extcon-enable` 由同模块的 `sprd_fchg_extcon.c / sprd_vchg_detect.c` 解析 ✅。
**唯一 DT 中给了但 原厂 不解析的私有属性：`cm-ifly-charge-control`（见 §2）。**

---

## 2. 【P0 功能差异】缺少 iFlytek 软件快充算法

用"stock .ko 的函数符号是否出现在 原厂 源码标识符集合"精确比对（排除 LTO 内联/CFI/工具链符号），整个充电/USB 簇只有一处实质缺失：

```
sprd-charger-manager.ko : cm_sw_fast_charge_algo   (0x8018, 0x2bc = 700B)
```

**它是活代码，不是死代码：**
- 调用点：`readelf -r` 在 `.text+0x7b10` 有 `R_AARCH64_CALL26 .text + 8018`，位于 `cm_cp_tune_algo`（电荷泵调谐循环）内。
  （objdump 把它误标注成 `__cfi_check`，所以按符号名 grep 会漏。）
- 入口第一个判断：`ldrb w8,[desc,#636]; cbnz w8, ...` —— **只有 `desc->ifly_charge_control` 非 0 才会执行**。
- 该标志由 `charger_manager_probe` 中 `of_find_property(np, "cm-ifly-charge-control")` 设置（`.text+0x8f48` 引用该字符串）。se50 的 DT 有这个属性 → 原厂标志=1 → 算法运行。

**日志佐证**：stock 有 `>>>>>>>fast_charge_algo:step: %d`、`fast_charge_handler`、`Set ifly_charge_control`、`ifly_charge_control, will decrease input current for aging...`。

**影响**：这是叠加在 原厂 自带 CP 调谐（`cm_cp_vbat/ibat/vbus/ibus_step_algo`、`cm_cp_tune_algo`）上的一层软件闭环快充。缺它 → CP 快充的电流/电压爬升曲线与讯飞不同（可能更快/更慢或达不到峰值），**基础充电（5V/9V fixed fchg、PPS 握手、热保护）仍走 原厂 原生逻辑，不受影响**。

**为什么不能直接搬 stock 模块**：ABI 上 stock charger-manager 还依赖 `struct power_supply` 的讯飞布局（这正是 S4 里 CRC 不同的根因），CRC-patch 后二进制会按错误偏移访问结构体，风险高（S4 已判定"不推荐"）。

**修复选项：**
1. **反编译移植**（推荐）：还原 `cm_sw_fast_charge_algo`（700B，直接读写 `struct charger_desc` 的约 15 个字段偏移）+ 在 `struct charger_desc` 加 `ifly_charge_control` 字段 + probe 解析该 DT 属性 + 在 `cm_cp_tune_algo` 中调用。需要先用 原厂 的 `struct charger_desc` 布局把偏移映射回去。工作区搜索无源码（仅 Rockchip 同名概念，不可用）。
2. **先真机验证**：只刷 boot+模块，实测 CP 快充表现；若可接受则暂不移植。
3. **不推荐**：CRC-patch 复用 stock charger-manager（结构体布局风险）。

---

## 3. 【P2】同名电量计冲突（与原厂同构）

- base DTB（`vendor_boot_dtb.bin` carve 后）`/soc/aon/spi@400000/pmic@0/fgu@800` **无 status → enabled**；overlay `fragment@13` 只加校准/capacity-track，未 disable。
- 因此 stock 系统中 `sc27xx_fuel_gauge`（compatible `sprd,ump9620-fgu`）与 `cw221x_battery`（`cellwise,cw221X`）都会 probe 并注册 `"sc27xx-fgu"`。
- `power_supply_register` 同 class 同名设备在 `device_add` 会 `-EEXIST`，只有一个成功。stock 的 `modules.load` 顺序与我们的包**一致**（`sc27xx_fuel_gauge` 在前），因此胜负判定应与原厂相同。
- **待真机确认**：`dmesg | grep -i "sc27xx-fgu|cw221" ` 看哪个注册成功。若原厂实际用 cw221x（外置 CW2217B）而 PMIC FGU 因硬件原因 probe 失败，两边也应一致。

---

## 4. 【P2】其它次要差异（不影响充电）

| 模块 | stock 有、原厂 无 | 评估 |
|---|---|---|
| sc8551-charger | `sc8551_init_adc`/`sc8551_deinit_adc`（读 IBUS/VBUS/IBAT/VBAT 后开关 ADC 并打日志） | 原厂 已有低层 `sc8551_enable_adc/get_adc_data/set_adc_scanrate`；属诊断封装，功能可替代 |
| sprd_tcpm | `sprd_tcpm_debug_open` | debugfs，无功能影响 |
| musb_hdrc | `musb_linknode_clear`/`musb_regdump_open` | USB debugfs |
| sprd_usb_f_rndis | `rndis_opts_*`（configfs 属性） | RNDIS 配置项，非充电 |

---

## 5. 结论与建议

- **ABI 可加载 + 接线/属性集对齐** → 基础充电链路成立。
- **唯一需要决策的功能缺口**：`cm_sw_fast_charge_algo` + `cm-ifly-charge-control`（CP 快充调谐）。
- 建议：先真机实测；若 CP 快充不达标，再做反编译移植（我可以按 `struct charger_desc` 偏移逐字段还原）。

复现命令：
```bash
# 函数差集（真正 iFlytek-only）
grep -rhoE '[a-zA-Z_][a-zA-Z0-9_]{3,}' drivers/power/supply drivers/usb/typec drivers/usb/musb drivers/usb/gadget/function | sort -u > /tmp/ids
nm modules/sprd-charger-manager.ko | awk '$2~/[tT]/{print $3}' | grep -vE '\.|^__' | sort -u > /tmp/sf
comm -23 /tmp/sf /tmp/ids
# 调用点（objdump 会误标 __cfi_check）
readelf -r modules/sprd-charger-manager.ko | grep -E '\+ (0x)?8018'
# DT 标志引用
readelf -r modules/sprd-charger-manager.ko | grep -E '\+ (0x)?30dc'
```
