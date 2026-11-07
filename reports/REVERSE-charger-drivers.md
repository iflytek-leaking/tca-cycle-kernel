# REVERSE-charger-drivers — 原厂（讯飞 se50）充电/电量计模块逆向与移植可行性

> 目标：判定原厂 \`.ko\` 能否用磁盘上的 RK 5.10 BSP 源码移植，还是要按原厂 \`.ko\` 重写。
> 结论先行：**RK 5.10 的 cw221x_battery.c / sgm41542_charger.c 都不是原厂 .ko 的源码**（同族/相近而已），不能直接拿来编；
> 但 **原厂 树已自带 7 个模块的几乎同源源码**，可直接重编；只有 cw221x、sgm415xx、was4780 三个需要"以同族源码为骨架补差"或重写。
> 全部 CRC 不兼容的根因只有一个：**讯飞改过 \`struct power_supply\`**（与驱动逻辑无关）。

---

## 0. 结论速览

| # | 原厂模块 | 原厂源码路径（.ko 内嵌字符串/Kconfig） | 磁盘可用源码 | 函数覆盖 | 判定 |
|---|----------|------------------------------------------|--------------|----------|------|
| 1 | cw221x_battery.ko | drivers/power/supply/cw221x_battery.c ✅内嵌 | **RK 5.10** cw221x_battery.c（同族精简变体） | 14 个局部函数中 9 个同名 | ❌ 不能直接用；**大改移植**（RK 为骨架 + 补 Unisoc 层） |
| 2 | sgm415xx-charger.ko | 推断 drivers/power/supply/sgm415xx-charger.c（CONFIG_CHARGER_SGM415XX） | **原厂** sgm41511-charger.c（祖先）；**RK** sgm41542_charger.c（无关驱动） | 30 个函数：RK 0 个；原厂 按名映射 16/30 | ❌ 不能直接用；**大改移植**（原厂 sgm41511 为骨架 + 补 14 函数）或重写 |
| 3 | sc8551-charger.ko | drivers/power/supply/sc8551-charger.c ✅内嵌 | **原厂** sc8551-charger.c | 18/19 | ✅ **可移植（直接重编）**，补 1 个 helper |
| 4 | bq2560x-charger.ko | drivers/power/supply/bq2560x-charger.c ✅内嵌 | **原厂** bq2560x-charger.c | 26/26 | ✅ **可移植（直接重编）** |
| 5 | bq2597x-charger.ko | drivers/power/supply/bq2597x-charger.c ✅内嵌 | **原厂** bq2597x-charger.c | 15/15 | ✅ **可移植（直接重编）** |
| 6 | sy65153_wireless_charger.ko | drivers/power/supply/sy65153_wireless_charger.c ✅内嵌 | **原厂** sy65153_wireless_charger.c | 19/19 | ✅ 可重编（但 se50.dts **无节点**，仅 1h10 板型用） |
| 7 | was4780-i2c.ko | drivers/usb/typec/mux/was4780-i2c.c ✅内嵌 | 无（原厂 的 typec/mux 只有 pi3usb30532.c；RK/OH 也没有） | 0/10 | ❌ **需重写**（或复刻原厂逻辑） |
| 8 | sprd-charger-manager.ko | charger-manager.c + sprd_fchg_extcon.c + … ✅内嵌 | **原厂** charger-manager.c + sprd_vote.c + sprd_vchg_detect.c + sprd_fchg_extcon.c（Makefile 的 sprd-charger-manager-objs） | 137/138 | ✅ **可移植（重编）**，补 \`cm_sw_fast_charge_algo\` |
| 9 | sprd_tcpm.ko | drivers/usb/typec/tcpm/sprd_tcpm.c ✅内嵌 | **原厂** sprd_tcpm.c | 53/54 | ✅ **可移植（重编）**，补 \`sprd_tcpm_debug_open\` |
| 10 | sc27xx_fuel_gauge.ko | drivers/power/supply/sc27xx_fuel_gauge.c ✅内嵌 | **原厂** sc27xx_fuel_gauge.c（+sprd_battery_info.c） | 41/42 | ✅ **可移植（重编）** |
| 附 | snd-soc-sprd-pa-aw883xx.ko | — | 原厂 sound/soc/codecs/aw883xx | — | ✅ 重编（它也因 power_supply 符号被卡） |

> "覆盖"口径：\`nm --defined-only <ko>\` 的局部/全局函数名，与候选源码里是否出现同名标识符比较；只能证明"函数集一致"，不等于逐字节同源（每个模块都差 1 个左右 helper，见 §3–§5）。

---

## 1. 方法与证据

1. **模块元数据**：\`modinfo\`、\`nm\`（.ko 未 strip，符号齐全）、\`strings\`、\`readelf -S\`。
2. **modversions**：自写脚本解析 \`__versions\`（每条 8B CRC + 56B 名字），与父 agent 的 \`se50-clang-out/Module.symvers\`（与讯飞 module_layout=\`0x2f279e7b\` 一致的那份）逐符号对比。
3. **Ghidra headless**：\`JAVA_HOME=/usr/lib/java-21 /usr/lib/ghidra/support/analyzeHeadless … -postScript DumpDecomp.java '~.*'\`，aarch64 全函数反编译（cw 54 个函数、sgm 107 个函数，含库桩）。脚本 \`tcakernel/re-ghidra/scripts/DumpDecomp.java\`。
4. **objdump/readelf**：\`.data\` 里的 profile 数组、\`.rodata\` 的 \`of_device_id\` 表、源文件路径字符串。
5. **源码比对**：\`rksdk/kernel-5.10\`、\`tcakernel/kernel_ums9620_se50\`、\`device_soc_sprd-OpenHarmony\`(仅 LK 有 sgm4154x/bq2560x，非内核驱动)。
6. **DT**：\`tcakernel/se50.dts\`（dtbo overlay 解出）中 \`fragment@37\`。

---

## 2. cw221x_battery.ko 逆向

### 2.1 基本信息
- \`modinfo\`：author \`Xu Shengfei <xsf@rock-chips.com>\`、description \`CW221X FGADC Device Driver V0.1\`、alias \`of:N*T*Ccellwise,cw221X\`、\`depends: sprd_battery_info\`。
- 依赖 = Rockchip 作者名 → 代码血统来自 Rockchip/Cellwise，但**被 Unisoc 化**（见下）。
- 定义符号 14 个：\`cw221X_probe/remove/suspend/resume\`、\`cw_init\`、\`cw_read_word\`、\`cw_battery_get/set_property\`、\`cw_bat_work\`、\`cw221x_init_sprd_bat_info\`、\`registers_show/store\`、\`debug_temp_show/store\`。

### 2.2 probe 流程（\`cw221X_probe\` @0x1012f8）
1. \`devm_kmalloc(0x110)\` 分配私有结构（含 i2c_client、dev、workqueue、delayed_work、psy、profile 指针、各测量值）。
2. \`iio_channel_get(dev, 0)\` 取一个 ADC 通道 → \`iio_read_channel_raw()\` 存 ADC 原始值（用于选 profile）；失败只打日志。
3. \`devm_iio_channel_get(dev, "charge-vol-channel")\` 取 VBUS 电压通道；失败 **致命**（-517）。
4. \`device_property_read_u8_array(dev,"cellwise,battery-profile",…)\`：
   - 读不到 → \`dev_warn "No battery-profile found, using current flash contents"\`；
   - 长度≠0x50(80) → \`-EINVAL\`；
   - 否则 devm_kmalloc(80)+读入。
5. **profile 回退**：算 \`d=|0x51c - adc_raw|\`，\`d<=199\` 用 \`gf_config_profile_info\`，否则 \`lf_config_profile_info\`（打印 \`"%s, the driver gf/lf profile:"\`）。
6. \`cw_init()\` 初始化 IC；失败重试（msleep 200，最多 2 次）后放弃。
7. 读一遍 FW_VERSION(0xAB)/CHIP_ID(0x00)/VCELL(0x02)/SOC(0x04)/TEMP(0x06)/CURRENT(0x0E)/CYCLE(0xA4)/SOH(0xA6) 存私结构。
8. \`devm_power_supply_register\`，**psy name = "sc27xx-fgu"**（不是 RK 的 "cw221X-bat"），type=battery，15 个属性。
9. \`alloc_workqueue("cwfg_gauge")\` + \`INIT_DELAYED_WORK(cw_bat_work)\`，首延时 0xd=13 jiffies。
10. 读 DT \`max-bat-cap\`（默认 10000000，**单位 µAh**）与 \`bat-rsense\`（默认 0x5dc=1500，单位 mΩ）。
11. \`cw221x_init_sprd_bat_info()\`：\`sprd_battery_get_battery_info()\` → 取 battery_info 里的 \`voltage_max_design\` 存偏移 0x100，再 \`sprd_battery_put_battery_info()\`。
12. 建 sysfs：\`registers\`（device 属性）+ \`debug_temp\`（psy 属性）；module param \`debug\`（=2 时每秒 dump 0x00..0xFF 并 250ms 轮询）。

### 2.3 DT 属性 / compatible（se50.dts 1168 行附近 \`cw2217b@64\`）
\`\`\`dts
cw2217b@64 {
    reg = <0x64>;
    compatible = "cellwise,cw221X";
    io-channels = <&pmic_adc 0x0e>;
    io-channel-names = "charge-vol-channel";
    cellwise,battery-profile = <0x5a000000 0xc600 … 0x00 0x68>;  /* 80B */
    max-bat-cap = <0x895440>;   /* 9,000,000 µAh */
    bat-rsense  = <0x636>;      /* 1590 mΩ */
    monitored-battery = <&bat>;
};
\`\`\`
原厂驱动实际读取的属性：\`cellwise,battery-profile\`、\`max-bat-cap\`、\`bat-rsense\`、\`io-channels\`/\`io-channel-names="charge-vol-channel"\`（以及隐式用 sprd_battery_info 的电池信息）。

### 2.4 寄存器操作序列（\`cw_init\` @0x101c0c，与 RK 完全一致）
- 读 **0x00** 必须 = **0xA0**（否则 \`not cw221X\`）。
- 读 **0x08 MODE_CONFIG**：
  - ==0（active）→ 读 **0x0B SOC_ALERT**，若 bit7(UPDATE_FLG)=1 则逐字节比对 **0x10..0x5F**（80B）与内存 profile；全同→无需更新。
  - 否则进入"刷 profile"：
    1. 写 **0x08=0x30**，msleep(20)；
    2. 写 **0x08=0xF0**（sleep），msleep(10)；
    3. 依次写 **0x10+i = profile[i]**，i=0..0x4F（80 字节）；
    4. 写 **0x0B=0x80**（UPDATE_FLG）、**0x0A=0x00**（关中断）；
    5. 写 **0x08=0x30**，msleep(20)；写 **0x08=0x00**（active），msleep(10)；
    6. 轮询 **0xA7 IC_STATE** 的 bit2|bit3(0x0C)，最多 50×100ms；超时再写 0x30→0xF0 回 sleep，返回 -1。
- \`cw_read_word()\`：对同一寄存器连续读 2 次（间隔 msleep(10)），不一致再读第 3 次；返回 2 字节。

### 2.5 电量算法关键常量（与原厂反编译一致）
| 量 | 寄存器 | 公式/常量 |
|---|---|---|
| 电压 | 0x02/0x03 | \`raw * 5 / 16\`（mV，0.3125mV/LSB） |
| SOC | 0x04/0x05 | \`soc_h\`（高字节）作为 ui_soc；\`>=100\` 钳到 100；分值存 soc_l |
| 温度 | 0x06 | \`raw*5 - 400\`（0.1°C，即 -40 + raw/2） |
| 电流 | 0x0E/0x0F | 补码；\`(fw&0xC0)!=0\`（CW2215/2217）→ \`raw*1600/rsense\`；\`(fw&0xC0)==0 && fw!=0\`（CW2218）→ \`raw*3815/rsense\` |
| 循环 | 0xA4/0xA5 | \`raw / 16\` |
| SOH | 0xA6 | 直读 |
| FW | 0xAB | bit7:6 = 01→CW2217 / 10→CW2215 / 00→CW2218 |
| 满充 | — | 属性 CHARGE_FULL/DESIGN = \`max-bat-cap\` 默认 10,000,000 µAh |
| rsense | — | DT \`bat-rsense\` 默认 1500 |

工作周期：正常 0x4e2=**1250ms**；\`debug==2\` 时 0xfa=**250ms** 并整表 dump。

### 2.6 profile 常量（.data）
- \`lf_config_profile_info\`（偏移 0x1c0，80B）= **与 RK \`config_profile_info\` 逐字节相同**：
  \`5a 00 00 00 00 00 00 00 a0 b2 c2 ca c2 bd 9c 5c 38 ff ff c4 86 74 60 55 4f 4d 4b 80 c0 db cd d0 ce d2 d3 d2 d0 ce c3 d5 b9 c9 c5 a3 92 8a 80 72 63 62 74 90 a6 7e 5f 48 80 00 ab 10 00 a1 fb 00 00 00 64 1e b1 04 00 00 00 00 00 00 00 00 00 4d\`
- \`gf_config_profile_info\`（偏移 0x170，80B）为另一条曲线（首字节同样 5a，第 8 字节起 \`bd a8 a7 c2 …97\`）。
- DT 里给的那条 profile 与两者都不同，说明 se50 走的是 DT profile。

### 2.7 与 RK 5.10 cw221x_battery.c 的差异清单
**相同（骨架/算法层）**：compatible \`cellwise,cw221X\`、i2c id \`cw221X\`、MODULE_AUTHOR/DESCRIPTION；上述全部寄存器序列与算法常量；\`cw_read_word\` 双读策略；lf profile。

**不同（必须补的）**：
1. RK 只用 \`cellwise,battery-profile\`；原厂额外 \`max-bat-cap\`/\`bat-rsense\`/\`io-channels\`+name，并用 **sprd_battery_info**。RK 没有依赖 sprd_battery_info。
2. RK 无 profile 时固定用内置表；原厂多一条 **ADC 选型**逻辑（iio_channel_get/iio_read_channel_raw → gf/lf）。
3. ps name：RK \`cw221X-bat\` ↔ 原厂 **\`sc27xx-fgu\`**（charger-manager 的 \`cm-fuel-gauge\` 按名找）。
4. 原厂多 VBUS 采集（\`charge-vol-channel\` + \`iio_read_channel_processed\`）与 sysfs \`registers\`/\`debug_temp\`。
5. 属性集/number：原厂 15 个，含 CAPACITY_LEVEL 阈值、STATUS 特判、\`debug_temp\` 覆盖 TEMP；RK 11 个。
6. 轮询/重试细节与 work 周期不同（原厂 1250ms，RK 5000ms）。
7. 原厂产出的 5 个 RK 没有的函数：\`cw221x_init_sprd_bat_info\`、\`registers_show/store\`、\`debug_temp_show/store\`。

> **判定**：RK cw221x_battery.c 是"同族精简版"。直接编译可编过，但**不能驱动 se50 的 DT**（丢 max-bat-cap/bat-rsense/iio/sc27xx-fgu 名称/gf-lf 选择），会让 charger-manager 拿不到 fgu。→ 以 RK 为骨架**大改移植**（补 5 函数 + 4 组 DT 属性 + 命名），寄存器/算法部分基本可原样复用。

---

## 3. sgm415xx-charger.ko 逆向

### 3.1 基本信息
- \`modinfo\`：description \`SGM41513 Charger Driver\`、\`depends: sprd-charger-manager,sprd_battery_info\`；of alias 8 个：\`sgms,sgm41513_chg / sgm41513x_chg / sgm41516_chg / sgm41516D_chg / sgm41541_chg / sgm41542_chg / sgm41543_chg / sgm41543D_chg\`。
- 30 个局部函数，全部 \`sgm41513_*\` 命名：probe/remove/suspend/resume、set_termina_cur、set_limit_current、stop_charge、start_charge、enable/disable_otg、vbus_is_enabled、usb_get/set_property、property_is_writeable、otg_work、feed_watchdog_work、work、current_work、usb_change、int_handler、set_current、check_wireless_charge、en_chg_timer、dump_register_show、register_* 等；全局导出一个 \`sgm415xx_disable_pfm\`。
- **无内嵌源文件路径**（不像其它模块），只能由讯飞 config \`CONFIG_CHARGER_SGM415XX=m\` 与字符串判断源文件约在 \`drivers/power/supply/sgm415xx-charger.c\`（推测）。

### 3.2 probe 流程（\`sgm41513_charger_probe\` @0x1033cc）
1. 校验 I2C 支持 SMBUS_BYTE_DATA，\`devm_kmalloc(0x338)\`。
2. 读 **REG 0x0B** 得 part id：\`(val>>3)&0xF\`；再从 of_match 的 match data 取"期望 part id"（表里没写则默认 **0xD**），不一致直接报 \`The vendor id is 0x%x\` 失败。
3. \`of_find_node_opts_by_path("/chosen")\` 读 bootargs；若以 \`"charger"\` 开头 → 标记（工厂/充电模式，0x66 字节置 1）。
4. \`devm_usb_get_phy_by_phandle(dev, "phys", 0)\`；失败致命。
5. \`extcon_get_edev_by_phandle(dev, 0)\`（属性 \`extcon\`）；失败致命。
6. \`power_supply_get_by_name("sc27xx-fgu")\` + \`power_supply_put\`（只探测/告警）。
7. \`device_property_present(dev,"role-slave")\`：无 → 角色=1（master），有 → 角色=2（slave）并 \`devm_gpiod_get(dev,"enable",…)\`。
8. 找 syscon：\`of_find_compatible_node("sprd,sc27xx-syscon")\` 或 \`"sprd,ump962x-syscon"\`；\`of_device_is_compatible(parent,"sprd,sc2721")\` 决定一处常量 0x8000/1；\`of_property_read_u32_index(np, <charger_detect 属性>, 1/2, …)\` 取检测寄存器与 \`charger_pd\`；\`dev_get_regmap()\`（**只有电池检测/PD 用 regmap，主充电走 i2c_smbus_byte**）。
9. \`devm_power_supply_register\`（master → \`sgm41513_charger_desc\`；slave → \`sgm41513_slave_charger_desc\`）。
10. \`sprd_battery_get_battery_info()\` 取电压/电流门限；失败只 warn 并用默认 500mA/4.2V 等。
11. **hw_init 序列**（见 3.4）。
12. 建 work/alarm（watchdog feed、current work），master 再 \`devm_regulator_register\`（vbus regulator，名字 \`sgm41513_otg_vbus\`）。
13. \`atomic_notifier_chain_register\`（phy 的 notifier）、sysfs group \`sgm41513_sysfs\`（属性 \`sgm41513_dump_reg/lookup_reg/sel_reg_id/reg_val\`）、\`irq-gpio\` → \`devm_gpio_request_one("sgm41513_int")\` → \`devm_request_threaded_irq(…,"sgm41513 interrupt")\`、\`usb_phy_get_charger_current()\` 初始 limit。
14. \`device_create_file\` 建 \`registers\`。

### 3.3 DT compatible / 属性（se50.dts \`sgm_charger@3B\`）
\`\`\`dts
sgm_charger@3B {
    compatible = "sgm,sgm41542_chg";   /* 注意是 _chg 后缀 */
    reg = <0x3b>;
    phys = <…>; extcon = <…>; monitored-battery = <&bat>;
    irq-gpio = <&gpio 0xb9 0x01>;
    sgm41513_otg_vbus { regulator-name = "sgm41513_otg_vbus"; };
};
\`\`\`
驱动读取的属性：\`phys\`、\`extcon\`、\`role-slave\`、\`enable\`、\`irq-gpio\`、\`monitored-battery\`(经 sprd_battery_info)、syscon 的 charger_detect 属性。

### 3.4 寄存器操作序列（SGM41513 0x00–0x0f）
hw_init（master，从 \`probe\` 反编译）：
1. \`0x0B |= 0x80\`（REG_RST 软复位）；
2. \`0x05 &= ~0x30\`（关看门狗 timer）；
3. \`0x01 |= 0x40\`（WD_RST 复位看门狗）；
4. \`0x00 &= 0x7F\`（退出 HIZ）；
5. \`0x04 = (x & 7) | 0xB8\`（充电电压上限，终止电压域）；
6. \`0x0A |= 0x03\`（mask Iindpm/Vindpm 中断）；
7. master: \`0x06 |= 0xC0\`（OVP），slave: \`0x06 = (x&0x3F)|0x80\`；
8. \`0x06 = (x&0xF0)|vindpm_code\`（VINDPM，3.9V 起 100mV/step）；
9. \`0x04 = (x&7)|vreg_code<<3\`（VREG，3.856V 起 32mV/step，0x18=4.6V）；
10. \`0x04 |= 0x01\`（recharge）；\`0x05 &= ~0x08\`（禁 EN_TIMER）；slave 分支另有 \`0x0D &= ~1\`（disable JEITA）。
充电电流（\`sgm41513_charger_set_current\`）：
- 非 slave：分段线性映射到 6-bit 写 **0x02**（<0x9c41=40000µA: /5000；≤1.5A 多段；≥1.5A /120000+0x30；上限 0x3D）；
- slave：\`min(x,0x39AD9F)/60000\`。
- \`sgm415xx_disable_pfm(dev, en)\`：读 **0x01**，写 \`(x&0x7F)|(en<<7)\`。
OTG：读/写 **0x01** 的 OTG_EN，boost current 在 **0x02** 的 bit7。

### 3.5 与 RK 5.10 / 原厂 sgm41511 的对比
**RK sgm41542_charger.c：完全不同的驱动**
- compatible \`"sgm,sgm41542"\`（DT 是 \`"sgm,sgm41542_chg"\` → **RK 驱动根本不匹配**）；
- 命名 \`sgm4154x_*\`，走 regulator 框架，DT 属性是 \`input-voltage-limit-microvolt\`/\`input-current-limit-microamp\`/\`regulators\` 子节点；无 extcon/phys/sprd syscon/sprd_battery_info/watchdog/OTG power_supply/sysfs。
- 函数名 0 个与 .ko 相同；寄存器编号巧合地一致（都是 SGM41513 系列 AD 器件的 0x00–0x0f）。
- → **RK 源码不可用**。

**原厂 sgm41511-charger.c：最接近的祖先**
- 同样结构：\`sgm41511_read/write/update_bits\`、\`hw_init\`、\`feed_watchdog_work\`、\`otg_work\`、\`enable/disable_otg\`、\`vbus_is_enabled\`、\`usb_get/set_property\`、\`register_vbus_regulator\`、\`sgm41511_charger_is_fgu_present\` 等 —— 与原厂 **16/30 按名一一对应**（把 \`sgm41513_\` 换成 \`sgm41511_\`）。
- 原厂多出的 14 个：\`set_termina_cur\`、\`usb_change\`、\`charger_work\`、\`current_work\`、\`int_handler\`、\`check_wireless_charge\`、\`en_chg_timer\`、\`sgm415xx_disable_pfm\`、\`dump_register_show\`、\`register_id_show/store\`、\`register_table_show\`、\`register_value_show/store\`、\`registers_show/store\`。
- compatible 也不同：原厂 \`"Sgm,sgm41511_chg"\`（大写 S）↔ 原厂 \`"sgm,sgm41542_chg"\` 等 8 个；of_match 带 match data（期望 part id）。
- → **原厂 sgm41511 可作骨架，但要补 14 个函数、多型号 of_match、DT 属性/命名差异，属于"大改移植"**；工作量明显大于 sc8551/bq。

> 网页检索未找到 Unisoc \`sgm415xx-charger.c\` 公开源码；SGM41513 datasheet 可佐证寄存器映射（0x00 输入控制/HIZ、0x01 power-on/WD_RST、0x02 ICHG、0x04 VREG、0x05 timer/term、0x06 VINDPM/OVP、0x0B part id/reset）。

---

## 4. 其余模块源码核实（原厂 / RK / 其它）

| 模块 | 证据 | 结论 |
|---|---|---|
| sc8551-charger.ko | 内嵌 \`drivers/power/supply/sc8551-charger.c\`；原厂 同名文件含 18/19 函数（仅缺 \`sc8551_deinit_adc\`，原厂 有 \`sc8551_init_adc/sc8551_get_ibat_now_mA\`、ADC 寄存器）；compatible \`sc,sc8551-standalone/master/slave\` 与 DT 一致 | **直接重编**（补 deinit helper） |
| bq2560x-charger.ko | 内嵌同名文件；原厂 26/26；原厂 该驱动本身就是 BQ/SGM 通吃（识别 sgm41513/A、sgm41511、bq25601、sy6974）；compatible \`ti,bq2560x_chg\` | **直接重编** |
| bq2597x-charger.ko | 内嵌同名文件；原厂 15/15；compatible \`ti,bq2597x-standalone/master/slave\` | **直接重编** |
| sy65153_wireless_charger.ko | 内嵌同名文件；原厂 19/19；compatible \`silergy,wl_charger_sy65153\`；**se50.dts 无该节点**，仅在 \`dtbo_extract/1h10.dts\` 与 \`cm-wireless-chargers\` | 可重编；se50 用不到 |
| was4780-i2c.ko | 内嵌 \`drivers/usb/typec/mux/was4780-i2c.c\`；compatible \`sprd,WAS4780-i2c\`；导出 \`WAS4780_switch_event/reg_notifier/unreg_notifier\`、\`was4780_tcpc_event_changed/tcpc_get_micgnd_status/tcpc_micgnd_changed\`；原厂 typec/mux 目录只有 pi3usb30532.c | **无源 → 重写**（或按 .ko 反编译复刻） |
| sprd-charger-manager.ko | 内嵌 \`charger-manager.c\`+\`sprd_fchg_extcon.c\`；原厂 Makefile \`sprd-charger-manager-objs := charger-manager.o sprd_vote.o sprd_vchg_detect.o sprd_fchg_extcon.o\`；138 函数覆盖 137（缺 \`cm_sw_fast_charge_algo\`，RK 的 rockchip_charger_manager.c 有同名实现可参考） | **重编**（补 1 函数） |
| sprd_tcpm.ko | 内嵌 \`drivers/usb/typec/tcpm/sprd_tcpm.c\`；原厂 53/54（缺 \`sprd_tcpm_debug_open\`，疑 debugfs 条件编译） | **重编** |
| sc27xx_fuel_gauge.ko | 内嵌 \`sc27xx_fuel_gauge.c\`；原厂 41/42；原厂 Makefile \`obj-$(CONFIG_FUEL_GAUGE_SC27XX) += sc27xx_fuel_gauge.o sprd_battery_info.o\` | **重编** |

Kconfig 缺口（讯飞 config 里的符号，原厂 树没有）：\`CONFIG_FUEL_GAUGE_CW221X\`、\`CONFIG_CHARGER_SGM415XX\`、\`CONFIG_TYPEC_MUX_WAS4780\`。这也独立佐证这三个驱动在 原厂 树上不存在。

---

## 5. 根因：不是驱动源码，而是 \`struct power_supply\`

用 \`se50-clang-out/Module.symvers\`（module_layout=\`0x2f279e7b\`，与讯飞一致）逐符号比对 11 个模块的 \`__versions\`，**所有 CRC 差异全部集中在 \`power_supply_*\`**：

- cw221x_battery.ko：\`power_supply_changed\`(8ab67211→dd9846f4)、\`power_supply_get_drvdata\`、\`devm_power_supply_register\`；+ \`sprd_battery_*\`（由 sprd_battery_info.ko 提供，跨模块，非内核）。
- sgm415xx-charger.ko：\`power_supply_get_property/get_drvdata/put/get_by_name/unregister/devm_power_supply_register\` + \`cm_notify_event\`、\`sprd_battery_*\`(跨模块)。
- sc8551/bq2560x/bq2597x/sy65153/was4780/sprd-charger-manager/sprd_tcpm/sc27xx_fuel_gauge：同样只差 \`power_supply_*\`（charger-manager 还差 \`sprd_tcpm_*\`，由 sprd_tcpm.ko 提供，跨模块）。
- 反例自证：\`power_supply_is_system_supplied\`（无 \`struct power_supply*\` 参数/返回）**CRC 一致**；凡参数或返回值含 \`struct power_supply *\` 的都变了 → 讯飞改了 **\`struct power_supply\` 布局**（增删/改字段），不是 config、不是编译器。

影响面（谁依赖充电簇的导出符号）：只有充电簇自身 + \`snd-soc-sprd-pa-aw883xx.ko\`（\`power_supply_get_property/get_by_name\`）。其余 150+ 个原厂 .ko 与充电簇无符号耦合，可继续复用。

**结论**：
1. 想原样复用这 11 个 .ko → 必须让内核的 \`struct power_supply\` 与讯飞一致（源码不在盘上，只能在 \`include/linux/power_supply.h\` 里按 CRC/字段偏移猜改）；跨模块符号（sprd_battery_info/tcpm）也要成对保留 → 风险高，不推荐。
2. 用源码重编 → genksyms 自动产出与**我们内核**一致的 CRC，跨模块同时一致 → 一劳永逸。

---

## 6. 移植 / 重编方案

### 阶段 0：固化现状
- 内核用 原厂 \`kernel_ums9620_se50\`；config 用 \`se50-custom.config\`；clang r416183b；\`module_layout\` 已能复现 \`0x2f279e7b\`。
- 记录基线：\`se50-clang-out/Module.symvers\`。

### 阶段 1：直接重编（覆盖 7 个模块，工作量小）
打开（\`se50-custom.config\` 已是 \`=m\`）：
\`\`\`
CONFIG_FUEL_GAUGE_SC27XX=m
CONFIG_CHARGER_MANAGER=m            # → sprd-charger-manager.ko（charger-manager.o+sprd_vote.o+sprd_vchg_detect.o+sprd_fchg_extcon.o）
CONFIG_SPRD_TYPEC_TCPM=m            # → sprd_tcpm.ko
CONFIG_CHARGER_SC8551=m
CONFIG_CHARGER_BQ2560X=m
CONFIG_CHARGER_BQ2597X=m
CONFIG_WIRELESS_CHARGER_SY65153=m   # se50 可不要
\`\`\`
- 重编后把 .ko 放回 vendor_ramdisk，替换原厂同名文件。
- 补差：
  - \`sc8551_deinit_adc\`：可从 原厂 的 \`sc8551_disable_adc\` 逻辑改/加；
  - \`cm_sw_fast_charge_algo\`：参考 \`rksdk/kernel-5.10/drivers/power/supply/rockchip_charger_manager.c:1356\`；
  - \`sprd_tcpm_debug_open\`：debugfs 桩即可（大概率非关键）。

### 阶段 2：cw221x 移植（以 RK 源码为骨架 + 补 Unisoc 层）
1. 拷 \`rksdk/kernel-5.10/drivers/power/supply/cw221x_battery.c\` 到 原厂 \`drivers/power/supply/\`。
2. 寄存器/算法/默认 profile 不动。
3. 增加：
   - \`#include <linux/iio/consumer.h>\` + \`sprd_battery_info.h\`；
   - ps name 改 **"sc27xx-fgu"**，type battery，属性补 vbus/TEMP/CAPACITY_LEVEL；
   - DT 解析：\`max-bat-cap\`(默认 10000000)、\`bat-rsense\`(默认 1500)、\`devm_iio_channel_get("charge-vol-channel")\`、\`iio_channel_get()+iio_read_channel_raw\`；
   - profile 回退 gf/lf 两表（.data 已提取，见 §2.6；也可只保留 DT profile + lf 表）；
   - \`sprd_battery_get_battery_info()\` 取 \`voltage_max_design\`；
   - sysfs \`registers\`（读 0x00..0xAB，写 \`"%x %x"\`）与 \`debug_temp\`；
   - module param \`debug\`（=2 全表 dump + 250ms）。
4. Kconfig/Makefile：新增 \`config FUEL_GAUGE_CW221X tristate "CW221X Fuel Gauge"\`，\`obj-$(CONFIG_FUEL_GAUGE_CW221X) += cw221x_battery.o\`（对应讯飞 config 名）。
5. 若嫌工作量大，也可先只做"能注册 sc27xx-fgu + 读 DT profile + 基本属性"，先让充电链路通，后续再补 debug。

### 阶段 3：sgm415xx 移植（工作量最大）
两条路：
- **A. 以 原厂 sgm41511-charger.c 为骨架改名扩写**：\`sgm41511_\`→\`sgm41513_\`；加 8 个 compatible + of_match match data（期望 part id，默认 0xD）；补 IRQ handler、\`charger_work/current_work/usb_change\`、\`check_wireless_charge\`、\`en_chg_timer\`、sysfs \`register_*\`/\`registers\`、\`sgm415xx_disable_pfm\`；对齐 DT 属性 \`phys/extcon/role-slave/enable/irq-gpio\` 与 syscon charger_detect。寄存器序列按 §3.4 核对。
- **B. 直接按 .ko 反编译重写**（§3.2/3.4 已给完整流程），更可控但要复刻 watchdog/alarm、OTG regulator、notifier、sysfs。
- 只要重编，CRC 自动匹配；对外行为（power_supply 属性、cm_notify_event）按 .ko 字符串/反编译对齐即可。
6. Kconfig/Makefile 用 \`CONFIG_CHARGER_SGM415XX\`。

### 阶段 4：was4780 重写
- \`drivers/usb/typec/mux/was4780-i2c.c\`：I2C（\`regmap\`）+ compatible \`sprd,WAS4780-i2c\`，导出 \`WAS4780_switch_event\`、\`WAS4780_reg_notifier/unreg_notifier\`、\`was4780_tcpc_event_changed\`、\`was4780_tcpc_get_micgnd_status\`、\`was4780_tcpc_micgnd_changed\`；实现 \`WAS4780_usbc_analog_setup_switches/update_settings\`、probe/remove、notifier。
- 也可先复制原厂 .ko 反编译的寄存器读写序列（字符串里有 \`USB TYPEC_MODE\`、\`setting GPIOs active\`）。
- Kconfig 名 \`CONFIG_TYPEC_MUX_WAS4780\`（原厂 Kconfig 需新增）。

### 阶段 5：验证
- \`modinfo\` 的 vermagic 必须为 \`5.4.210-android12-9-668732-gb286db7ac1f5-dirty SMP preempt mod_unload modversions aarch64\`。
- 用 \`Module.symvers\` 复核每个新 .ko 的 \`__versions\` 是否 0 差异。
- 设备上 \`insmod\` 顺序：sprd_battery_info → sprd_tcpm → sprd-charger-manager → cw221x → sgm415xx → sc8551（依赖链见各 \`depends:\`）。
- /sys/class/power_supply/sc27xx-fgu、charger-manager 的 \`cm-chargers/cm-charge-pumps/cm-fuel-gauge\` 是否就绪。

### 不推荐方案
- **关 MODVERSIONS + 改 vermagic**：struct 布局不同，\`power_supply_get_property\` 等会按错偏移取字段，可能直接崩。
- **手工伪造内核 \`__crc_\`**：可让 .ko 过校验，但 ABI 仍不一致。
- **硬套 RK 的 cw221x/sgm41542 源码**：DT 不匹配（属性名/compatible）、缺 Sprd 集成，等于没充电管理。

---

## 7. 风险与未决（标注推测）

- **推测**：\`sgm415xx-charger.c\` 的确切源文件名与目录；其根因依赖 \`CONFIG_CHARGER_SGM415XX\` 推断（.ko 无内嵌路径）。
- **推测**：\`cm_sw_fast_charge_algo\` / \`sprd_tcpm_debug_open\` / \`sc8551_deinit_adc\` 是否条件编译或已被改名；当前在盘上找不到同名定义。
- \`sgm41513_charger_of_match\` 的 match data 数值未逐项解出（relocatable .rodata 合并导致），只确认识别逻辑为 \`读0x0B>>3&0xF == of_match.data\`、默认 0xD。
- **推测**：se50 上 cw221x 与 sc27xx_fuel_gauge 都注册 \`sc27xx-fgu\`；实际用哪个取决于加载顺序/是否加载，需在设备上确认（DT \`cm-fuel-gauge="sc27xx-fgu"\`）。
- \`iio_channel_get(dev, NULL)\` 取的是哪个通道、ADC 阈值 \`0x51c±199\` 的物理含义未定（推测为电池型号检测）。
- 本报告未改动任何源码；只新增本 md。

---

## 附录 A：复现命令
\`\`\`bash
# modversions 解析（对每个 .ko）
readelf -S -W X.ko            # 取 __versions 的 offset/size
python3 crcdiff.py            # 与 se50-clang-out/Module.symvers 对比（脚本见 /tmp/revchg/crcdiff.py）

# Ghidra headless
JAVA_HOME=/usr/lib/java-21 /usr/lib/ghidra/support/analyzeHeadless \
  /tmp/ghidra_proj revcw -import X.ko -overwrite \
  -scriptPath tcakernel/re-ghidra/scripts -postScript DumpDecomp.java '~.*'

# 反编译产物
/tmp/revchg/cw.clean.txt   # cw221x 54 函数
/tmp/revchg/sgm.clean.txt  # sgm415xx 107 函数

# 源文件路径
strings -a X.ko | grep -oE 'drivers/[A-Za-z0-9_/.-]+\.c' | sort -u

# profile 数组（.data 0x170=gf / 0x1c0=lf，各 80B）
objdump -s -j .data X.ko
\`\`\`

## 附录 B：寄存器速查
- CW221X：0x00 ID(0xA0)、0x02/03 VCELL、0x04/05 SOC、0x06 TEMP、0x08 MODE(0x30/0x00/0xF0)、0x0A GPIO、0x0B SOC_ALERT(bit7 UPDATE_FLG)、0x0E/0F CURRENT、0x10..5F PROFILE(80B)、0xA4/A5 CYCLE、0xA6 SOH、0xA7 IC_STATE(0x0C ready)、0xAB FW_VER。
- SGM41513：0x00 输入/HIZ(bit7)、0x01 power-on/WD_RST(bit6)/CHG_EN/OTG_EN、0x02 ICHG、0x03 预充/终止、0x04 VREG/VRECHG、0x05 timer/EN_TERM、0x06 VINDPM/OVP、0x08 状态、0x09 fault、0x0A Iindpm/Vindpm mask、0x0B part id(bits6:3)+REG_RST(bit7)、0x0D JEITA。
