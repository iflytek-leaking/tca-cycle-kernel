# 原厂（讯飞）充电类 .ko 的 power_supply ABI 逆向报告

> 目标：搞清讯飞原厂 11 个充电/电量计 .ko 的 `power_supply` ABI 与本机 原厂 内核的差异，
> 判断能否只改 `include/linux/power_supply.h`（必要时含 `power_supply_core.c`）让 genksyms CRC 匹配原厂值。
> 本文所有结论均可复现；推测项已显式标注“推测”。

---

## 0. 结论摘要（TL;DR）

1. 11 个模块与本机内核不兼容的符号**只有 9 个 `power_supply_*`**（CRC 见表 3.1）。其余符号（含 `module_layout`）全部一致。
2. 本机 原厂 的 `include/linux/power_supply.h` **就是 stock ACK `android12-5.4`**，只比原版多了一个 Unisoc 原型
   `power_supply_set_input_current_limit_from_supplier()`（§2）。所以“原厂改了 power_supply 类型”这个既有结论成立，
   但**改的不是 原厂 这棵树**。
3. 从 11 个 .ko 反推出的、这些模块实际使用的 ABI **全部与 原厂 一致**：
   `struct power_supply_desc` 布局/大小（0x68）、`get/set_property` 指针在 0x30/0x38、
   `struct power_supply_config` 的 `drv_data@0x10`、`to_power_supply` 用的 `offsetof(dev)=0x38`、
   `enum power_supply_property` 的数值（模块属性数组逐项映射到 原厂 名字）全部吻合（§5）。
4. genksyms 的 CRC 覆盖**整个可达类型的 token 流，且包含成员名**（§4）。既然模块用到的偏移/枚举值都对得上，
   差异只能落在**某个成员名**或**模块不访问的尾部字段/未用类型**上——这类信息在**剥离调试信息的 .ko 里不可唯一恢复**。
5. 因此：**无法从现有二进制/源码唯一地推导出“最小 header patch”**。已穷举测试 1.6 万余种组合（§7），均无法复现原厂 CRC。
   要给出精确 patch，必须拿到讯飞那份 `power_supply.h` 原文。
6. 替代方案（均可落地）：
   - **A（推荐）重编这 11 个模块**：原厂 树已含其中 9 个的源码，2 个可从 rksdk 5.10 移植（§8）。
   - **B 直接改 .ko 的 `__versions` CRC**：已在本机全部跑通，11 个模块改完与 `Module.symvers` 零不匹配（§9）。
     因“只有这 11 个模块引用这些符号”且所有用到的偏移都一致，运行时风险低（但仍需真机 insmod 验证）。
   - **C 改内核 Image 的 `__kcrctab`**：把内核侧这 9 个符号的 CRC 改成原厂值，未改动的 .ko 即可加载（§10）。

---

## 1. 方法与工具

| 项 | 值 |
|---|---|
| 原厂模块目录 | `vendor_rd/lib/modules/`、`modules/`（两处内容相同） |
| 本机导出表 | `se50-clang-out/Module.symvers` |
| 本机内核（含 DWARF） | `se50-clang-out/vmlinux` |
| 编译器 | `clang r416183b`（AOSP，路径见 PORTING-NOTES），LTO + CFI_CLANG |
| Ghidra | `/usr/lib/ghidra/support/analyzeHeadless`（需 `JAVA_HOME=/usr/lib/java-21`） |
| Ghidra 脚本 | `/tmp/ghidra_scripts/DumpDecomp1.java`（本机 Ghidra 未启用 PyGhidra/Jython，必须用 **Java** 脚本） |
| 反编译产物 | `/tmp/ghidra_out/*.ko.txt`（11 个模块，符号表 + 全部函数伪 C） |
| CRC 复现环境 | 预编译的 `se50-clang-out/scripts/genksyms/genksyms` + 本机实测 `c_flags` |
| 快速 CRC oracle | `/tmp/pp.i`（`power_supply_core.c` 经 `-D__GENKSYMS__` 预处理），`genksyms` 单次约 42ms |

**重要观察**：原厂 .ko 是 LTO+CFI 产物，但**局部符号未剥离**（`cw_battery_get_property`、`bq2560x_charger_desc` 等都在），
因此字段偏移可以从“构造函数代码”和“静态 desc 初始化重定位”直接读出，无需依赖 CFI 蹦床。
（任务里提到的 CFI 蹦床定位法用于**函数地址**；本任务主要需要字段偏移，未用到。）

---

## 2. 本机 原厂 头 = stock ACK android12-5.4

把 `include/linux/power_supply.h` 与 AOSP ACK `android12-5.4` 原文逐行 diff，唯一差异：

```diff
-int power_supply_get_property_from_supplier(struct power_supply *psy,
-                                            enum power_supply_property psp,
-                                            union power_supply_propval *val);
+extern int power_supply_set_input_current_limit_from_supplier(
+                                         struct power_supply *psy);
```

即 原厂 只是把 ACK 的 `p_get_property_from_supplier` 换成了 Unisoc 的 `set_input_current_limit_from_supplier`。
另外核对 `android12-5.4 / android12-5.10 / android13-5.10`：`struct power_supply` 定义完全一致。
**结论：原厂 这棵树没有在 power_supply 上做私有改动；差异来自讯飞侧。**

---

## 3. CRC 对照表

### 3.1 9 个不匹配符号（原厂值在 11 个模块里完全一致）

| 符号 | 原厂 stock CRC | 本机 原厂 CRC |
|---|---|---|
| `power_supply_changed` | `0x8ab67211` | `0xdd9846f4` |
| `power_supply_get_property` | `0x47d247fa` | `0x57e44dfc` |
| `power_supply_set_property` | `0xcbd756e4` | `0x5bf6f089` |
| `power_supply_get_drvdata` | `0x1ad8b42f` | `0x65bde01a` |
| `power_supply_get_by_name` | `0x6cb7ea88` | `0xc0aff7cb` |
| `power_supply_put` | `0x389729e9` | `0xdbe24633` |
| `power_supply_unregister` | `0x6977207a` | `0x0ef25448` |
| `power_supply_register` | `0x26e40249` | `0xa0f28c21` |
| `devm_power_supply_register` | `0x41bfb453` | `0x9009a9d8` |

### 3.2 匹配的 power_supply 符号（反证：不涉及 struct power_supply 的类型不变）

| 符号 | 双方 CRC | 说明 |
|---|---|---|
| `power_supply_is_system_supplied(void)` | `0x2c208607` | 只有 int，不触及结构体 |
| `power_supply_reg_notifier(struct notifier_block *)` | `0x136209db` | |
| `power_supply_unreg_notifier(struct notifier_block *)` | `0x6b81c38b` | |

“凡参数/返回值含 `struct power_supply *` 的导出符号全部变，纯标量的全不变”——与“power_supply 相关类型被改过”完全自洽。

### 3.3 每个模块的不匹配数量

| 模块 | 不匹配/引用 | 模块 | 不匹配/引用 |
|---|---|---|---|
| sprd-charger-manager.ko | 8 / 9 | sc8551-charger.ko | 7 / 7 |
| sgm415xx-charger.ko | 6 / 6 | bq2560x-charger.ko | 5 / 5 |
| cw221x_battery.ko | 3 / 4 | bq2597x-charger.ko | 3 / 3 |
| sy65153_wireless_charger.ko | 3 / 3 | sprd_tcpm.ko | 3 / 3 |
| sc27xx_fuel_gauge.ko | 3 / 3 | was4780-i2c.ko | 2 / 3 |
| snd-soc-sprd-pa-aw883xx.ko | 2 / 2 | | |

**只有这 11 个模块引用 `power_supply_*`**（全量 194 个模块扫描确认）；其余 183 个模块完全不碰这些符号。
这一点对方案 B/C 很关键。

---

## 4. genksyms 语义（决定了能不能“改结构体凑 CRC”）

用本机 `genksyms` 实测（输入含 `__GENKSYMS_EXPORT_SYMBOL(foo)`）：

| 实验 | 结果 |
|---|---|
| 改 struct 成员名 aaa 到 bbb | CRC 变，**成员名参与哈希** |
| 改函数形参名 | CRC 不变 |
| 改返回类型 | CRC 变 |
| 改 struct 成员类型/顺序/增删 | CRC 变 |
| 加/减 const 限定符 | **CRC 变**（早期一次 overlay 未生效导致误判，已用正确路径复测） |
| 前向声明后再定义 | 定义仍被完整展开，CRC 变 |

进一步实测：只改 `struct power_supply` 的一个成员名，9 个符号的 CRC **全部改变**；
只改 `struct power_supply_desc` 的一个成员名，同样 9 个全变。
原因：函数原型里的 `struct power_supply *` 会被 genksyms 递归展开整棵可达类型图
（`power_supply -> power_supply_desc -> enum/union/函数指针 -> ...`）。
`struct power_supply_config` **不可达**于 `struct power_supply`，所以改它只影响 `*_register` 两个符号，不影响 `power_supply_changed`。

**推论**：要复现原厂 CRC，必须得到**整棵可达类型图逐 token 一致**的定义，成员名一个都不能差。

---

## 5. 从原厂 .ko 反推出的 ABI（重点）

### 5.1 struct power_supply_desc（原厂，实测）

来源：`cw221X_probe` 里对 `devm_kmalloc(0x68)` 出来的 desc 逐字段赋值；以及
`bq2560x_charger_desc / bq2560x_slave_charger_desc / sy65153_wl_charger_desc` 三个静态对象（size=104、重定位偏移）。

| 偏移 | 原厂（.ko 实测） | 原厂（DWARF） | 一致？ |
|---|---|---|---|
| 0x00 | name（指针） | `const char *name` | 是 |
| 0x08 | type（int，模块写 0） | `enum power_supply_type type` | 是 |
| 0x10 | usb_types（模块未用） | `enum power_supply_usb_type *usb_types` | 一致（未触发） |
| 0x18 | num_usb_types（未用） | `size_t num_usb_types` | 一致（未触发） |
| 0x20 | properties（.data 属性数组） | `enum power_supply_property *properties` | 是 |
| 0x28 | num_properties（stp x10,x9,[x0,#32]，8 字节） | `size_t num_properties` | 是 |
| 0x30 | 函数指针（get_property） | `get_property` | 是 |
| 0x38 | 函数指针（set_property） | `set_property` | 是 |
| 0x40 | 函数指针（property_is_writeable） | `property_is_writeable` | 是 |
| 0x48/0x50 | 未设置（0） | `external_power_changed/set_charged` | 一致 |
| 0x58/0x5c | 未设置 | `no_thermal/use_for_apm` | 一致 |
| size | **0x68 (104)** | 0x68 | 是 |

### 5.2 struct power_supply_config（原厂，实测）

`cw221X_probe` 中：`str x8,[sp,#8]`（of_node@0）、`str x19,[sp,#24]`（drv_data@0x10）。

| 偏移 | 原厂 | 原厂 |
|---|---|---|
| 0x00 | of_node | of_node，是 |
| 0x10 | drv_data | drv_data，是 |

### 5.3 struct power_supply（原厂，实测）

Ghidra 反编译出 `power_supply_get_drvdata(param_1 + -0x38)`，即原厂模块内部 `to_power_supply(dev)` 用的
`offsetof(struct power_supply, dev) = 0x38` —— 与 原厂 完全一致（原厂 DWARF：dev@0x38）。
所有 11 个模块对 `struct power_supply` 都**只通过 accessor 函数交互**，没有直接读写 `psy->desc/use_cnt/...`
（`power_supply_changed(psy)`、`power_supply_get_drvdata(psy)`、`power_supply_get/set_property(psy,...)`、
`power_supply_put(psy)` 都是函数调用）。因此二进制里能看到的字段约束只有 dev@0x38 和 desc/config。

**原厂 struct power_supply 完整布局**（`sizeof=1296=0x510`，供对照）：

```text
0x000 desc                0x3e0 changed_work           0x498 changed_lock
0x008 supplied_to         0x410 deferred_register_work 0x49c changed
0x010 num_supplicants     0x4a0 use_cnt                0x49d initialized
0x018 supplied_from                                  0x49e removing
0x020 num_supplies        #ifdef CONFIG_THERMAL       #ifdef CONFIG_LEDS_TRIGGERS
0x028 of_node             0x4a8 tzd                    0x4b8..0x500 十个 LED trigger 字段
0x030 drv_data            0x4b0 tcd                    0x508 android_kabi_reserved1
0x038 dev (size 0x3a8)                                 （size = 0x510）
```

### 5.4 enum power_supply_property（原厂，实测）

从 11 个模块里读出全部属性数组并映射到 原厂 枚举名，**全部落在合理属性上、且都 <= 74**：

| 模块 | 属性数组（十进制） |
|---|---|
| cw221x_battery | 7,46,50,2,3,24,12,17,6,0,51,13,14,18,40 |
| sgm415xx | 0,29,37,4,2,64,32,68 |
| sc8551 / bq2597x | 68,0,3,12,17,51,31,32,2,37,29 |
| bq2560x | 0,29,37,2,68,63 |
| sy65153 | 63,3,4,17,31,32,37,68 |
| sprd-charger-manager | 0,2,3,12,13,46,50,4,24,29,37,17,18,28,33,22,61,6,64,1,51,56,7,10 |
| sprd_tcpm | 64,4,9,8,12,16,17 |
| sc27xx_fuel_gauge | 2,3,51,6,46,12,14,13,17,18,31,40,68,7,10 |

例如 46=CAPACITY、50=CAPACITY_LEVEL、51=TEMP、68=CALIBRATE、64=USB_TYPE……
`sprd-charger-manager` 的 get_property 调用还直接内联了属性号 3/4/0x33/0x3f/0x44（=PRESENT/ONLINE/TEMP/TYPE/CALIBRATE），也全部对得上。
**因此：原厂 enum power_supply_property 与 原厂 在前 75 项上数值一致（“同名同值”）。**

---

## 6. 差异到底在哪（定位结论）

因为 struct power_supply / desc / config 的**已用偏移**、enum 的**已用数值**、以及
`to_power_supply` 的 dev@0x38 全部与 原厂 一致，而 genksyms 的 CRC 又对**成员名**敏感，
所以原厂与 原厂 的差异只可能是下面之一（按可能性排序，均**无法从剥离二进制唯一确定**）：

1. **某个成员的名字不同**（类型/顺序/大小完全相同，只是拼写不同）；
2. **在不改变被模块使用字段偏移的位置增/删了一个字段**（如 struct power_supply 尾部、desc 的 usb_types 一带）；
3. 某个 **enum 常量名**被改名/增删（值不变）；
4. **推测**：某个只被 struct power_supply 可达、而不被其它模块使用的叶子类型（最可能是 `struct led_trigger`；
   `thermal_zone_device`/`device_node` 已被别的符号 CRC 证明相同）发生变化。

> 注：`struct power_supply_battery_info` 不在上述 9 个符号的可达图中，**与本次 CRC 差异无关**。

---

## 7. 已穷举尝试的“最小 patch”假设（全部失败）

用快速 CRC oracle（`/tmp/pp.i` + genksyms）实测，**没有任何一种能同时命中 9 个原厂 CRC**：

- struct power_supply/desc/config 的 `ANDROID_KABI_RESERVE` 个数 0/1/2 的全部 18 种组合；
- `CONFIG_THERMAL` / `CONFIG_LEDS_TRIGGERS` 字段块的开/关组合；
- rksdk 5.10 的 33 个 `POWER_SUPPLY_PROP_CP_*` + `CHARGE_TYPE_LONGLIFE` + `TYPE_CHARGE_PUMP`（含显式/隐式起始值）；
- 把 原厂 平行头 `vendor_power_supply.h` 里 18 个 `POWER_SUPPLY_PROP_*` 追加/前插到主 enum；
- `union power_supply_propval` 增加 ival/bval/uval/strval2；
- desc 的 usb_types/properties 加 const（单独/组合）、成员交换、去掉 no_thermal/use_for_apm、type 改 int；
- 与 mainline v5.4/v5.10/v6.1、ACK android11-5.4/android12-5.4/android12-5.10/android13-5.10 的
  property/type/usb_type enum + desc + propval 做**全组合**替换：**7776 种，0 命中**；
- 对 struct power_supply/desc/config 的每个成员做**改名**、并在结构体尾部**加常见字段**：
  **8254 种，0 命中**。

**结论：无法从现有材料唯一定出最小 patch。** 要得到它，只能拿到讯飞那份 `power_supply.h`，
然后用 §11 的 genksyms 工具链直接验证。

---

## 8. 替代方案 A（推荐）：重编这 11 个模块

用**同一个内核源码树 + 同一套 clang r416183b + 同一份 .config** 重编，CRC 自动匹配，最稳。

源码可得性（本机工作区实查）：

| 模块 | 原厂 树 | rksdk 5.10 | 备注 |
|---|---|---|---|
| sc8551-charger.ko | 有 `drivers/power/supply/sc8551-charger.c` | 有 | 直接编 |
| bq2560x-charger.ko | 有 | - | 直接编 |
| bq2597x-charger.ko | 有 | - | 直接编 |
| sy65153_wireless_charger.ko | 有 | - | 直接编 |
| sc27xx_fuel_gauge.ko | 有 | 有 | 直接编 |
| sgm415xx-charger.ko | 仅有 sgm41511-charger.c | - | 需确认 sgm415xx 与 sgm41511 差异 |
| sprd_tcpm.ko | 有 `drivers/usb/typec/tcpm/sprd_tcpm.c` | - | 直接编 |
| snd-soc-sprd-pa-aw883xx.ko | 有 `sound/soc/codecs/aw883xx/` | 有 | 直接编 |
| sprd-charger-manager.ko | 有 `charger-manager.c`(+sqc) | 有 | 名字/胶水层需核对 |
| cw221x_battery.ko | 无 | 有 `cw221x_battery.c` | 从 rksdk 移植（模块本身作者就是 rock-chips） |
| was4780-i2c.ko | 无 | 无 | **无源码**，需另找源码或保留方案 B |

做法：把对应 `obj-m` 挂进 原厂 树，`make M=drivers/... modules`（或整树），产物天然与 `Module.symvers` 一致。
风险：sgm415xx/was4780/sprd-charger-manager 需先补齐源码。

---

## 9. 替代方案 B（已验证）：patch .ko 的 __versions CRC

**原理**：模块加载时 `check_version()` 拿模块 `__versions` 里的 (name, crc) 与本机内核 `__kcrctab` 的 CRC 比。
只要把这 9 个符号在模块里的记录改成本机值即可通过。`module_layout` 本来就一致，其它符号本来就一致。

**安全性依据**：
- 只有这 11 个模块引用 `power_supply_*`（全量扫描）；
- 模块对这些符号全部通过函数调用使用，直接字段约束只有 dev@0x38（一致）、desc/config 布局（一致）；
- 因此“CRC 差异来自成员名/未用字段”这一判断下，运行时结构体布局兼容。

**脚本**：`/tmp/patch_versions.py`（`struct modversion_info { u64 crc; char name[56]; }`，按 `__versions` 节逐条替换）。
本机已对 11 个模块跑通，改后逐个与 `Module.symvers` 比对 **mismatches = 0**：

```text
cw221x_battery.ko            3 项
sgm415xx-charger.ko          6 项
sc8551-charger.ko            7 项
bq2560x-charger.ko           5 项
bq2597x-charger.ko           3 项
sy65153_wireless_charger.ko  3 项
was4780-i2c.ko               2 项
sprd-charger-manager.ko      8 项
sprd_tcpm.ko                 3 项
sc27xx_fuel_gauge.ko         3 项
snd-soc-sprd-pa-aw883xx.ko   2 项
```

**注意**：
- 这只是**加载期绕过**；若原厂结构体真有模块会用到的字段差异，仍可能出问题（当前反汇编未发现此类访问，故风险低）。**必须真机 insmod + dmesg 验证**。
- 设备 vendor 分区可能受 AVB/dm-verity 保护，patch 后的 .ko 需重新打包 vendor_boot/vendor 并处理校验。
- 模块未签名（`CONFIG_MODULE_SIG` 未开），无签名失效问题。

---

## 10. 替代方案 C：改内核侧 __kcrctab

`kernel/module.c` 里 `check_version` 用的是内核的 `__kcrctab_<sym>`（`s32 __start___kcrctab[]`）。
把这 9 个符号在内核 `__kcrctab` 里的值改成原厂值，**原厂 .ko 一个字节都不用动**。
因为只有这 11 个模块引用它们，不会误伤其它模块。
缺点：要定位并重写 Image（vmlinux/Image）里的 `__kcrctab` 条目，比方案 B 更重；
若以后又编了依赖这些符号的模块，会反过来不兼容。**不如方案 B 直观。**

---

## 11. 如何实测 CRC（精确复现方法）

本机已把 `power_supply_core.o` 的真实编译命令从
`se50-clang-out/drivers/power/supply/.power_supply_core.o.cmd` 取出，套用内核自身的 genksyms 调用：

```bash
cd /home/misakapardola/leaking-iflytek/tcakernel/se50-clang-out
CMD=$(sed -n 's|^cmd_drivers/power/supply/power_supply_core.o := ||p' \
        drivers/power/supply/.power_supply_core.o.cmd)
NEWCMD=$(echo "$CMD" | sed -E 's# -c -o [^ ]+ # -E -D__GENKSYMS__ #')
export PATH=/home/misakapardola/leaking-iflytek/recovery_twrp/prebuilts/clang/host/linux-x86/clang-r416183b/bin:$PATH
eval "$NEWCMD" | scripts/genksyms/genksyms -r /dev/null | grep __crc_power_supply
```

要点：
- `-D__GENKSYMS__` 让 `EXPORT_SYMBOL` 变成 genksyms 认识的 `__GENKSYMS_EXPORT_SYMBOL`；
- 结果与 `power_supply_core.o.symversions` 和 `Module.symvers` 完全一致（本机 baseline = `0xdd9846f4`），证明工具链可信。
- **改 header 试 patch 时，用 include overlay 快速迭代**：把候选 `power_supply.h` 放
  `/tmp/ov/include/linux/power_supply.h`，在上述命令最前面插 `-I/tmp/ov/include`
  （注意是 `-I/tmp/ov/include`，不是 `-I/tmp/ov`，否则 `<linux/...>` 会解析到 `/tmp/ov/linux/...`），
  单次约 42ms（可先把 `-E` 结果存成 `/tmp/pp.i` 再反复跑 genksyms）。
- 验证模块侧：解析 `__versions`（`struct modversion_info` 64 字节一条）与 `Module.symvers` 逐符号比对，
  脚本见 `/tmp/allmismatch.py`、`/tmp/verify_patched.py`。

拿到讯飞 `power_supply.h` 后的判定流程：
1. 用上面命令算出 9 个符号 CRC；
2. 全部等于表 3.1 原厂值，则 patch 成立；
3. 只改 header 即可，无需动 `power_supply_core.c`（CRC 只由声明决定，不看实现）。

---

## 12. 产出文件

| 文件 | 内容 |
|---|---|
| `REVERSE-charger-abi.md` | 本报告 |
| `/tmp/ghidra_scripts/DumpDecomp1.java` | Ghidra headless Java 反编译脚本 |
| `/tmp/ghidra_out/*.ko.txt` | 11 个模块的反编译结果（符号表 + 伪 C） |
| `/tmp/patch_versions.py` | 方案 B：patch .ko `__versions` CRC |
| `/tmp/allmismatch.py` | 全量 `__versions` vs `Module.symvers` 比对 |
| `/tmp/verify_patched.py` | 验证 patch 后模块零不匹配 |
| `/tmp/pp.i` | 预处理后的 genksyms 输入（快速 CRC oracle） |

---

## 13. 不确定性说明（诚实标注）

- **确定**：9 个 CRC 值；原厂 头 = stock ACK；desc/config/dev 偏移与枚举数值一致；genksyms 含成员名；方案 B 可让 `__versions` 零不匹配。
- **推测**：差异落在“某个成员名/未用尾部字段/未用叶子类型”。未能定出具体字段。
- **未做**：真机 insmod 验证（本环境无设备）；was4780-i2c 与 sgm415xx 的源码获取。
- **若任务方有讯飞源码/更完整镜像**：把它的 `include/linux/power_supply.h` 丢过来，用 §11 可立刻确认最小 patch。
