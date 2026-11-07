# ABI 核实 — power_supply 簇（TCA Cycle Kernel）

**结论：布局与原厂完全一致，CRC 差异来自字段名而非内存布局。绕过 CRC 不会造成结构体错位。**

## 方法
只比对 **vmlinux（内核）实现、且原厂模块要 import** 的符号对应的类型——这是唯一可能真出错的地方。
先用 provider 分析把 44 个 CRC 不同符号分成两类：

| 类别 | 数量 | 说明 |
|---|---|---|
| vmlinux 实现 | 7 | `devm_power_supply_register` + `power_supply_{get_property,changed,get_by_name,get_drvdata,put,unregister}` |
| 模块实现 | 37 | 来自 `musb_hdrc` / `sprd-charger-manager` / `sprd_battery_info` / `sprd_tcpm` / `sprd_u_ether` |

工具：`tools/reverse-abi.py`（反推只读实例字段偏移）+ `objdump`（栈上 cfg / val 访问）。

## 1. struct power_supply_desc —— 一致
以原厂 `stock/modules/sgm415xx-charger.ko` 的 `sgm41513_charger_desc` 为例（符号大小实测 0x68）：

| 偏移 | 原厂 | 我们 | 判定 |
|---|---|---|---|
| +0x00 | name → .rodata+619 | name | ✓ |
| +0x08 | type=0 | type=0 | ✓ |
| +0x10 | usb_types → .data+138 | usb_types（未填=0） | ✓ 同字段 |
| +0x18 | num_usb_types=8 | 0 | ✓ 同字段 |
| +0x20 | properties → .data+158 | properties → .data+138 | ✓ 同字段 |
| +0x28 | num_properties=8 | 8 | ✓ |
| +0x30/+0x38/+0x40 | get_property/set_property/property_is_writeable | 同 | ✓ |
| size | 0x68 | 0x68 | ✓ |

**关键证据**：原厂 desc+0x20 指向的数组 = `[0,29,37,4,2,64,32,68]`，
与我们源码 `sgm41513_usb_props` 编译出的值**逐项相同** → properties 在同偏移（+0x20），
且 `enum power_supply_property` 数值没变。原厂 +0x10 的数组 `[0,1,2,3,5,6,7,9]` 是 USB type
值（SDP/DCP/CDP/C/PD/PD_DRP/PD_PPS/APPLE），是原厂驱动额外填了 `usb_types`，我们的没填而已。

## 2. enum power_supply_property —— 一致
75 个成员；`sgm41513_usb_props` 的编译值与原厂完全相同（见上）。

## 3. struct power_supply_config —— 一致
反汇编原厂 `sgm41513_charger_probe`：
```
840: str x19, [sp, #24]    ; cfg+0x10 = drv_data (info)
850: str x9,  [sp, #8]     ; cfg+0x00 = of_node
874: add x2, sp, #0x8      ; cfg 起始 = sp+0x8
87c: bl  devm_power_supply_register
```
即 `of_node@0x00、drv_data@0x10`，与我们的 `struct power_supply_config` 一致。

## 4. union power_supply_propval —— 一致
原厂 `sgm41513_charger_usb_get_property` 用 `str w8, [x20]` / `str wzr, [x20]` 写 offset 0、4 字节，
与我们 `{ int intval; const char *strval; }` 一致；原厂只写 intval，不会越界。

## 5. 其余类型
- `struct power_supply`：对模块**不透明**（模块只持有指针，通过 `power_supply_get_drvdata()` 取 drv_data）。
  内核自己分配/使用，布局由内核一侧决定，模块不受影响。
- 37 个模块实现符号对应的类型（`sprd_battery_info`、`charger_manager`、musb/gether/tcpm 等）：
  由**提供者模块和消费者模块共享**，只要该簇**整套用原厂模块**即自洽，内核不参与。

## 结论
- power_supply 簇（唯一 vmlinux 侧接口）4 个类型布局全对齐 → CRC 绕过**安全**。
- CRC 差异来自 genksyms 把**成员名**也算进哈希，以及各簇内部定义差异——不是内存布局。
- 无需修改内核源码去"对齐"。若坚持 100% 保守，可对这 7 个已核实的接口之外再做遍历，但预期无收益。

## 必读工具
```sh
python3 tools/reverse-abi.py <ko> <symbol> [size]   # 反推只读实例字段偏移
```
