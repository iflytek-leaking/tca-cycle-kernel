# se50 复用原厂 .ko 兼容性报告

## 方法
- 源码：`kernel_ums9620_se50`
- 工具链：clang r416183b（原厂同款）
- config：`se50-stock.config`（从原厂内核 IKCONFIG 提取）+ `LOCALVERSION` 对齐
- 编译产物：`Image`(38MB) / `vmlinux`
- 校验：把每个原厂 `.ko` 的 `__versions` CRC 与本内核 `Module.symvers` 逐符号比对

## 关键结果

| 指标 | 值 |
|---|---|
| **`module_layout` CRC** | 本内核 `0x2f279e7b` == 原厂 `0x2f279e7b` ✅ |
| 检查模块数 | 194（vendor_boot ramdisk 160 + 用户提供 134，去重） |
| **完全 CRC 兼容** | **183** |
| 有 CRC 不匹配 | **11**（合计 45 个符号，全是 `power_supply_*`） |

## 不兼容的 11 个模块（全是充电/电量计）

| 模块 | 不匹配/总数 |
|---|---|
| sprd-charger-manager.ko | 8/105 |
| sc8551-charger.ko | 7/49 |
| sgm415xx-charger.ko | 6/76 |
| bq2560x-charger.ko | 5/66 |
| sy65153_wireless_charger.ko | 3/49 |
| sprd_tcpm.ko | 3/87 |
| sc27xx_fuel_gauge.ko | 3/80 |
| cw221x_battery.ko | 3/40 |
| bq2597x-charger.ko | 3/52 |
| was4780-i2c.ko | 2/27 |
| snd-soc-sprd-pa-aw883xx.ko | 2/80 |

不匹配符号示例：`power_supply_changed`、`power_supply_get_property`、`power_supply_get_drvdata`、`devm_power_supply_register`、`power_supply_unregister`、`power_supply_get_by_name`。
→ 说明 **iFlytek 的内核改过 `power_supply`（`struct power_supply`/`power_supply_core`）**，与 原厂 源码不一致。config 里 `CONFIG_THERMAL/LEDS_TRIGGERS/POWER_SUPPLY` 两边完全相同，排除 config 因素。

## 结论
- **显示（双通道 DSI）**：`sprd-drm.ko` 318/337 匹配、**0 CRC 不匹配** ✅；缺的 `trusty_*/tipc_*` 由 vendor_ramdisk 里的 `trusty.ko`/`trusty-ipc.ko` 提供 ✅
- **触摸**：Himax 内建，开 config 即可 ✅
- **传感器/存储/网络/USB 等**：183 个模块兼容 ✅
- **充电/电量计**：11 个模块**加载不了** ❌ —— 需要 iFlytek 的 `power_supply` 源码差异

## 产物
- `se50-clang-out/arch/arm64/boot/Image`（可加载原厂 .ko 的测试 Image）
- `se50-custom.config`（含 Himax 触摸修正）

## 待办
1. 用 `se50-custom.config` 重编正式 Image
2. 充电：获取 iFlytek 的 `power_supply.h`/`power_supply_core.c`，或接受暂时无充电管理
