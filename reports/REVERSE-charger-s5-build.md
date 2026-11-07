# S5 充电方案实施报告（重编 + 移植 + 打包）

> 目标：让自编内核（原厂 UMS9620 源码 + 双 DSI 补丁）能驱动原厂 se50 的充电/电量/USB 子系统。
> 结论：**已全部落地并通过 ABI 校验**；充电簇、USB 簇共 16 个模块重编/移植 + 1 个显示 shim + 1 个补丁复用，交付物见 `s5-ship/`。
> 唯一硬缺口：**HX83102J 触摸驱动源码缺失**（见 §6）。

---

## 1. 交付物

```
s5-ship/
  boot-se50-s5.img           # 仅替换 kernel 的 boot.img（38,375,936 B Image）
  modules/                   # 18 个 .ko（已 strip debug）
  modules.load / .charger / .cali
  install-vendor-modules.sh
  README.md
```

- kernel：`se50-clang-out/arch/arm64/boot/Image`，clang r416183b + `se50-stock.config`+touch 之外的同源配置，`module_layout=0x2f279e7b`。
- boot.img：magiskboot 重打包，ramdisk/DTB 保持原厂。

---

## 2. 阶段 1：直接重编（充电簇）

在 `se50-clang-out` 用 clang 全量 `make modules` 后，以下模块与 `Module.symvers` **0 差异**（`__versions` 逐符号比对）：

```
sprd_battery_info  sprd_tcpm  sprd-charger-manager  sc27xx_fuel_gauge
bq2560x-charger    bq2597x-charger  sc8551-charger  sy65153_wireless_charger
sc27xx_pd
```

> 关键点：全量 `make modules` 让 `modpost` 能看到跨目录模块导出（`sprd_tcpm_*`、`sprd_battery_*`、`cm_notify_event`），单独的 `M=drivers/power/supply` 会因 `sprd_tcpm_*` 未定义而失败。

## 3. 阶段 2~4：移植 3 个缺失驱动

| 模块 | 来源 | 结果 |
|---|---|---|
| `cw221x_battery.ko` | RK 5.10 `cw221x_battery.c` 骨架 + Unisoc 层（IIO、`sprd_battery_info`、psy 名 `sc27xx-fgu`、`max-bat-cap`/`bat-rsense`、gf/lf profile、1250ms work） | 编译通过；注册名/属性/DT 对齐 |
| `sgm415xx-charger.ko` | 原厂 `sgm41511-charger.c` 改名 `sgm41513_*` + 8 compatible + 补 14 函数（IRQ/watchdog/OTG/sysfs/current_work…） | 编译通过 |
| `was4780-i2c.ko` | Ghidra 反编译复刻（regmap、update_settings、notifier、6 个导出） | 编译通过；导出 CRC 与 stock 对齐 |

**was4780 原型修正**：stock 消费者 `snd-soc-sprd-codec-ump9620.ko` 期望
`was4780_tcpc_get_micgnd_status` CRC = `0xd6e68204`。用 genksyms 穷举确认原厂原型是
**`bool was4780_tcpc_get_micgnd_status(void)`**（不是 `int`），已修正并重编 → CRC 命中。

CW221X 注意：原厂 的 `struct sprd_battery_info` 无 `voltage_max_design`，移植用语义等价的
`constant_charge_voltage_max_uv`（原厂 .ko 读的偏移 0x18 对应的就是它）。

## 4. 阶段 4.5：连带替换（此前未识别的 ABI 耦合）

用“全量 symvers（含模块导出）”重扫 134 个 stock 模块，除充电簇外还有 5 个模块因 provider 变了而必须成对替换：

| stock 模块 | 依赖的我们重编的 provider |
|---|---|
| `sc27xx_pd.ko` | `sprd_tcpm` |
| `musb_sprd.ko` | `musb_hdrc` |
| `musb_hdrc.ko` | （自身 provider，需替换） |
| `sprd_usb_f_rndis.ko` | `sprd_u_ether` |
| `sprd_u_ether.ko` | （自身 provider，需替换） |

其余 stock 模块报出的 261 个“符号问题”绝大多数是 **ABSENT**（provider 是保留的 stock 模块，provider+consumer 都是原厂，彼此 CRC 自洽），不影响加载。

扬声器 `snd-soc-sprd-pa-aw883xx.ko`：本地无对应源码且模块名不同，按 S4 方案 B
**只补丁 `__versions` 的 2 个 power_supply CRC**（`47d247fa→57e44dfc`、`6cb7ea88→c0aff7cb`），已验证 0 差异。

## 5. 重要修正：vermagic 的 `+` 无需处理

本内核 `CONFIG_LOCALVERSION` 已含 `-dirty`，因 git 树无 tag，`setlocalversion` 会再追加一个 `+`，于是内核 vermagic = `...-dirty+`，而 stock 模块 = `...-dirty`。

**但 `kernel/module.c:same_magic()` 在模块带 `__versions`（`CONFIG_MODVERSIONS=y`）时会跳过首个空格之前的部分**，只比较 `" SMP preempt mod_unload modversions aarch64"`。因此：
- stock 模块（`-dirty`）能加载到我们的 `-dirty+` 内核；
- 我们重编的模块也用同一逻辑匹配。

→ 不必为 vermagic 重新链接 vmlinux（`LOCALVERSION=` 可复现 stock 串，但非必需）。

## 6. 硬缺口：HX83102J 触摸驱动

- 原厂内核把 Himax 触摸**编进 vmlinux**（stock 模块集里没有 himax .ko），并导出 `himax_resume_by_lcd`/`himax_int_enable` 供 stock `sprd-drm.ko` 调用。
- 原厂 源码是**另一个 Himax 版本**：只有 `himax_ic_HX83112f.c`，**没有 `himax_ic_HX83102j.c`**；而 `himax_common.c` 会调用 `_hx83102j_init()` → 模块链接报 undefined。全盘检索（rksdk/t508n/其它设备目录）均无该源文件。
- 因此 原厂 触摸驱动无法编译，**触摸在本内核上不可用**。
- 为保显示，提供 `himax_lcd_notify.ko` shim，导出两个 stock 需要的符号，CRC 精确命中：`himax_resume_by_lcd=0xd79063d2`、`himax_int_enable=0x01633f89`。把它放在 `sprd-drm.ko` 之前加载即可。
- 恢复触摸需要移植 HX83102J IC 驱动（约 20KB 级、寄存器表/固件配置），属独立 RE 任务。

## 7. 验证

- 18 个交付模块对合并 symvers（vmlinux + 各 `M=` 目录）**0 mismatch**。
- stock 消费者 `snd-soc-sprd-codec-ump9620` 对 `was4780_tcpc_get_micgnd_status` 现已命中。
- `sprd-drm.ko` 需要的 2 个 himax 符号由 shim 命中。
- 未做真机 insmod（无设备）。

## 8. 复现要点

```bash
# 内核树
cd kernel_ums9620_se50
# 全量模块（解析跨目录依赖）
make O=../se50-clang-out ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu- \
     -j4 KCFLAGS=-Wno-error modules
# 移植模块（配置 CONFIG_FUEL_GAUGE_CW221X/CHARGER_SGM415XX/TYPEC_MUX_WAS4780=m）
make O=../se50-clang-out ... M=drivers/power/supply modules
make O=../se50-clang-out ... M=drivers/usb/typec/mux modules
# 显示 shim（树外）
make -C kernel_ums9620_se50 O=../se50-clang-out ... M=../himax_shim modules
# boot 重打包
cd s5-ship/bootwork && magiskboot unpack ../../boot/unpack/boot.img
cp ../../se50-clang-out/arch/arm64/boot/Image kernel
magiskboot repack boot.img ../boot-se50-s5.img
```

## 9. 后续建议

1. 真机验证：先只刷 boot + 覆盖模块，`dmesg` 看 `sprd-drm`/`sprd_battery_info`/`sc27xx_fuel_gauge` 是否 probe。
2. 触摸：从其它 UMS9620/同 Himax 内核找 `himax_ic_HX83102j.c`，或按 HX83112f 骨架 + 原厂固件配置移植。
3. 若不想替换 USB 簇，可评估把 `musb_hdrc`/`sprd_u_ether` 保持 stock、只重编 `musb_sprd`/`sprd_usb_f_rndis` 并补丁 CRC（未做）。
