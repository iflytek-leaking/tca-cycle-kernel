# ABI 核实（二）— 模块簇：不可混用

## 结论
模块↔模块的几个簇（电池/充电/typec/USB）**原厂与自编的结构体布局不同**，因此
**必须整簇一致**（全用原厂，或全用自编），**不能把自编 provider 和原厂 consumer 混着用**。
内核侧的 `power_supply_*` 接口则已验证兼容（见 `ABI-power-supply-verify.md`），不受影响。

## 电池簇 — struct sprd_battery_info（provider = sprd_battery_info.ko）

原厂有 134 个模块，包含自己的 `sprd_battery_info.ko`、`cw221x_battery.ko`、`sgm415xx-charger.ko`、
`sc27xx_fuel_gauge.ko`、`sprd-charger-manager.ko` 等——即**整簇都能用原厂件**。

### 证据 1：函数内存访问偏移集不同
`sprd_battery_get_battery_info`（同一函数）中提取所有 `[xN, #off]` 偏移：

| | 唯一偏移数 |
|---|---|
| stock/modules/sprd_battery_info.ko | 94 |
| build/.../sprd_battery_info.ko | 90 |

diff（stock 独有 vs ours 独有）：`20/44/76/152/240/340/472` vs `48/408/436/512` —— 结构性差异，非噪声。

### 证据 2：已知字段偏移（此前精确反汇编）
| 字段 | stock | ours |
|---|---|---|
| precharge_current_ua | **0x0c** | **0x10** |
| charge_term_current_ua | **0x10** | **0x14** |
| constant_charge_voltage_max_uv | **0x18** | **0x1c** |
| factory_internal_resistance_uohm | **0x20** | **0x24** |

→ 我们的 `struct sprd_battery_info` 在 precharge 之前**多一个 int 字段**，整段后移 4 字节。

## 后果 / 规则

| 组合 | 结果 |
|---|---|
| 原厂 provider + 原厂 consumer | ✅ 自洽 |
| 自编 provider + 自编 consumer | ✅ 自洽 |
| **原厂 provider + 自编 consumer（或反之）** | ❌ 字段错位（如电量/充电电流读错，危险） |

**因此：**
- 若要「全部用原厂模块」：电池/充电/typec/USB 各簇都从 `stock/modules/` 取，**别掺自编件**。
  包括原厂自带的 `cw221x_battery`/`sgm415xx-charger`，我们的移植版就不再需要。
- 若要用我们的移植驱动（`cw221x_battery`/`sgm415xx-charger`/`was4780-i2c`）：
  必须把该簇的 provider（`sprd_battery_info.ko` / `sprd-charger-manager.ko` / `sprd_tcpm.ko` 等）
  也一起重编，保持整簇一致。

## 覆盖范围
|`provider`|符号|同文件消费者簇|
|---|---|---|
|sprd_battery_info.ko|sprd_battery_get/put_battery_info, find_ocv2cap/resistance_table|sc27xx_fuel_gauge, bq2560x, bq2597x, sgm415xx, cw221x, sy65153, sprd-charger-manager|
|sprd-charger-manager.ko|cm_notify_event|sprd_tcpm, sc8551-charger, bq2597x, was4780|
|sprd_tcpm.ko|sprd_tcpm_*|sc27xx_pd, sc8551-charger|
|musb_hdrc.ko|musb_*/sprd_musb_*|musb_sprd|
|sprd_u_ether.ko|gether_*/sprd_gether_*|sprd_usb_f_rndis|

以上 5 个簇各自内部共享头文件，整簇同源即一致。

## 工具
```sh
python3 tools/reverse-abi.py <ko> <symbol> [size]     # 反推只读实例字段偏移
aarch64-linux-gnu-objdump -d <ko> | awk '/<func>:/{p=1} p{print} p&&/^$/{exit}'   # 函数内访问偏移
```
