# REVERSE-power_supply（最终结论）

## 结论
- 磁盘上**没有 iFlytek 的 5.4 内核源码**；只有 原厂 树本身 + `rksdk/kernel-5.10`（Rockchip 5.10）。
- 11 个充电/电量计模块与本内核的不兼容**只出现在 `power_supply_*` 符号** → iFlytek 改了 `struct power_supply` / `power_supply_core` 的 ABI（genksyms CRC 不同）。config 已排除（`THERMAL`/`LEDS_TRIGGERS`/`POWER_SUPPLY` 两边完全一致）。
- 不匹配符号：`power_supply_changed` / `get_property` / `get_drvdata` / `put` / `get_by_name` / `unregister` / `devm_power_supply_register`。

## 可用的充电驱动源码（5.10，可作移植基础）
- `rksdk/kernel-5.10/drivers/power/supply/cw221x_battery.c`（电量计，原厂 树缺）
- `rksdk/kernel-5.10/drivers/power/supply/sgm41542_charger.c`（充电，原厂 树缺）
- 原厂 树已有：`sc8551`、`aw883xx`、`sgm41511-charger.c` 等

## 可行路线（按成本排序）
1. **重编这 11 个模块**（原厂 已有 sc8551/aw883xx + 从 rksdk 5.10 移植 cw221x/sgm41542）→ 与我们的内核 CRC 自动匹配，最稳。
2. 逆向 iFlytek 的 `struct power_supply` 字段增删并改 原厂 头，逐个试 CRC → 工作量大、易错。
3. 关 MODVERSIONS + 改模块 vermagic → 不推荐（struct 布局可能不同，运行有风险）。

## 说明
原 S1 子 agent 多次未交付，已停；本结论由主 agent 有界搜索 + 既有 CRC 数据得出。