# 触摸/压感笔修复 —— Himax HX83102J 驱动补齐

## 症状

刷入只换 kernel 的 boot 后：能开机、Wi-Fi 正常，但**触摸与压感笔全部失效**。

## 根因

`se50.dts` 的触摸节点写得很清楚：

```dts
himax_ts@0 {
    compatible = "himax,hxcommon";
    himax,panel-coords = <0x00 0x640 0x00 0xa00>;   /* 1600 x 2560 */
    himax,irq-gpio = <... 0x0d 0x01>;
    himax,rst-gpio = <... 0x0e 0x01>;
    status = "okay";
};
```

触摸 IC 是 **Himax**（原厂内核里也满是 `[HXTP] Himax common touch panel driver init` 日志）。
而原厂内核把 Himax 驱动**内建**进 vmlinux（stock config：`TOUCHSCREEN_VENDOR_V2=y` +
`TOUCHSCREEN_HIMAX_CHIPSET_V3_3=y` + `TOUCHSCREEN_HIMAX_IC_HX83102J=y`，
`TOUCHSCREEN_BOARD_NAME="harrow"`）——我们换掉 kernel 后这个驱动就没了。

更具体地：ZTE F50 这棵发布树里 `hxchipset_v3_3/Makefile` 引用了 `himax_ic_HX83102j.o`，
`.config` 也开着 `CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102J=y`，
**但 `himax_ic_HX83102j.c` 这个文件在 F50 发布时被删掉了**（F50 不用这块屏）。
所以 `CONFIG_VENDOR_COMMON_COMPILE` 只能关着——一开就编译报缺文件。
之前的 shim（只导出 `himax_resume_by_lcd`/`himax_int_enable`）只是让 `sprd-drm.ko` 能加载，
并不能替代真正的触摸驱动。

## 驱动来源

同 SoC 的 ZTE UMS9620 兄弟机型开源仓库里带完整的 pad 版 Himax 驱动：

| 仓库 | 路径 |
|---|---|
| `dikeckaan/zte-ums9620-kernel-5.4.254` | `drivers/vendor/common/touchscreen_v2/hxchipset_pad/` |
| `Enceka/android_kernel_zte_ums9620_mifi_u30air` | 同上 |
| `Ssrml/android_kernel_zte_ums9620_p720_w205ds` | 同上 |

`hxchipset_pad` 与我们的 `hxchipset_v3_3` 是**同一代框架**（同样 `hx_s_ts` / `hx_s_core_fp` /
`struct ic_setup_collect g_hx83102j_setup`），只是 IC 换成 HX83102J，并且带了 pad（平板）面板配置。

> 注：OnePlus/三星那些 `himax_ic_HX83102.c` 属于**新一代**框架（`private_ts` +
> `himax_modular_table.c`），接口完全不同，不能直接用。

## 改动清单

| 文件 | 改动 |
|---|---|
| `drivers/vendor/common/touchscreen_v2/hxchipset_pad/` | 新增（20 个文件，来自兄弟仓库） |
| `touchscreen_v2/Kconfig` | `source hxchipset_v3_3/Kconfig` → `source hxchipset_pad/Kconfig` |
| `touchscreen_v2/Makefile` | himax 目标对象由 `hxchipset_v3_3/*` 换成 `hxchipset_pad/*`；include 路径同步 |
| `touchscreen_v2/ztp_common.h` `ztp_core.[ch]` `ztp_report_algo.c` `ztp_state_change.c` `ztp_ufp.[ch]` `lcd_state_notify.c` | 升级到兄弟仓库版本（pad 驱动需要新版 `struct ztp_device` 与 `tpd_save_last_log` 等） |
| `touchscreen_v2/lcd_state_notify.c` | 去掉 `#ifdef TOUCHSCREEN_LCD_NOTIFY` 外层守卫——ztp core 无条件调用 `tpd_notifier_call_chain()` |
| `include/vendor/common/zte_tpd.h` | 新增（新版 ztp 需要） |
| `hxchipset_pad/himax_lcd_notify.c` | 新增：补 `himax_resume_by_lcd()`（原厂/讯飞私有，pad 驱动没有），转发到 `himax_chip_common_resume()` |
| `hxchipset_pad/firmware_config/harrow/` | 新增 |
| `drivers/misc/himax_lcd_notify.c` | **删除**（原来只导出两个符号的 shim，现已被真驱动取代；保留会与 `himax_platform.c` 的 `himax_int_enable` 重复定义） |

配置（`.config`）：

```
CONFIG_VENDOR_COMMON_COMPILE=y
CONFIG_TOUCHSCREEN_VENDOR_V2=y            # 由 =m 改为内建（原厂即为内建）
CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_PAD=y
CONFIG_TOUCHSCREEN_HIMAX_COMMON=y
CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102J=y
CONFIG_TOUCHSCREEN_HIMAX_DEBUG=y
CONFIG_TOUCHSCREEN_HIMAX_INSPECT=y
CONFIG_TOUCHSCREEN_BOARD_NAME="harrow"
# CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_V3_3 is not set
```

## 验证

```
himax 符号总数 : 421      ksymtab 导出 : 10
  himax_int_enable      himax_resume_by_lcd   _hx83102j_init
  himax_common_init     g_hx83102j_setup
uname -r : 5.4.210-tca-cycle
banner   : Project TCA cycle kernel Linux version 5.4.210-tca-cycle #21 ...
Image    : 31918592 bytes  (加触摸前 31521280)
原厂模块 : 160 个 / 1753 依赖符号 / 未解析 0 → PASS
ramdisk  : md5 与原厂逐字节一致 e8e25cdef60aa27285a37fdeb5198431
```

## 已知不确定项

1. **面板参数**：pad 驱动公开仓库里只有 `papaya` 这一份平板面板配置
   （`FIX_HX_RX_NUM=48` / `FIX_HX_TX_NUM=32` / `HXTP_DIFFER_CONFIG="huaying"` / `HX_ZERO_FLASH`）。
   讯飞 se50 原厂用的是 `harrow`（未公开）。本包把 `papaya` 内容放在 `harrow/` 目录下以满足
   原厂 `CONFIG_TOUCHSCREEN_BOARD_NAME="harrow"`。若触摸能出点但坐标/边缘异常，
   需要按实际模组调整 RX/TX 或换固件配置。
2. **零闪存固件**：`papaya` 打开了 `HX_ZERO_FLASH`，驱动会尝试加载/烧写固件文件。
   若开机 touch 无反应，先看 `dmesg | grep -i hx tp`（或 `HXTP`）确认是探测失败还是固件加载失败。
3. 本次改动把 `hxchipset_v3_3`（HX83112F）从构建里摘掉了——F50 若要回退，
   把 `Kconfig`/`Makefile` 的 source 与对象换回 `hxchipset_v3_3` 即可（目录仍在）。

## 调试建议（刷完先看这些）

```bash
dmesg | grep -iE "hxtp|himax|hx83102" | head -50    # 探测/固件日志
cat /proc/bus/input/devices | grep -i -A4 himax      # 输入设备是否注册
ls /sys/bus/spi/devices/                             # SPI 触摸节点是否出现
getevent -l                                         # 直接看有没有事件
```

若 `dmesg` 里出现 `Could NOT find Himax Chipset`，说明 SPI 通了但 IC ID 不匹配；
若出现 `himax_int_enable` 相关或 probe defer，说明 GPIO/regulator 依赖没满足。
