# 原厂 Himax 触摸驱动逆向 —— 寄存器/时序/面板参数实测

对象：`stock/boot/unpack/kernel`（原厂 5.4.210 Image，38,840,832 B）

## 方法

1. **地址换算**：arm64 `Image` 的 `file_offset = vaddr - _text`。
   用我们的 vmlinux 验证：`linux_banner` 的 `vaddr - _text` = `0x193562c`，
   与 Image 里该字符串的实际文件偏移逐字节吻合，且 `_text` 就在文件偏移 0。
2. **字符串扫描**：`strings` 直接暴露驱动版本与固定文件名。
3. **精确交叉引用**：只有字符串地址不足以定位——用 objdump 反汇编原厂 `.text`
   （`0xffffffc010081000`–`0xffffffc0110a0000`，4.2M 条），再解析
   `adrp xN, page` + `add xN, xN, #off` 求出目标地址，从而定位引用点。
4. **逐字段读数**：对 `hx_s_ic_data` / `hx_s_ts` 的偏移直接读立即数。

## 1. 驱动版本

```
3.0.0.19_#5635_Iflytek_SE50_01      <- 原厂（讯飞 SE50 定制）
3.0.0.22_#5831_P720P01_03           <- 我们移植的 ZTE pad 版
3.0.0.3_Iris7_01                    <- 仓库自带 v3_3 版
```

原厂比 v3_3 新、比 ZTE pad 版旧，是同一代码族的定制分支。

## 2. 寄存器映射 —— 逐字节一致

我们编出的 `g_hx83102j_setup`（`vaddr 0xffffffc011e43d18`，424 B）的字节序列，
在原厂 Image 文件偏移 **`0x240f6a8`** 唯一命中。逐字对齐结果：

```
原厂 104 words  <->  我们 106 words，精确对齐，只有 2 处差异：
  OURS EXTRA  ours+0x10  = 0x10007fe8   (_func_headphone)
  OURS EXTRA  ours+0x18  = 0x10007fe8   (_func_headset)
```

其余 **全部 104 个寄存器地址/数据字完全相同**，包括 IC 专有的
`_addr_rawout_sel = 0x100072ec`。结论：**寄存器配置无需任何改动**。
（这 2 个 `_func_*` 是 ZTE pad 版新增的字段，只影响结构体自身布局，
因为驱动按字段名访问，各自的布局内部自洽。）

## 3. IC 识别与芯片参数 —— 一致

反汇编原厂 `hx83102j_chip_detect`（`0xffffffc010e1e900` 起）：

```asm
ldrb w19, [sp, #7] ; ldrb w20, [sp, #6] ; ldrb w21, [sp, #5]   // 读 0x900000d0 的 4 字节
cmp  w19, #0x83
cmp  w20, #0x10
cmp  w21, #0x29        <- 与我们的 HX83102J 判定完全相同
```

> 注意：`strings` 里只搜到 `HX83102D`，**搜不到 `HX83102J`**——因为原厂是把
> 8 字节芯片名当**立即数**存进去的：
> `mov x9,#0x5848 / movk #0x3338 / movk #0x3031 / movk #0x4a32`
> → 小端 `48 58 38 33 31 30 32 4a` = **"HX83102J"**。
> 所以 `HX83102D` 只是它 rodata 里另一处无关残留，IC 依然是 J。

随后写入 `hx_s_ic_data` 的立即数：

| 字段 | 原厂 | 我们的 `himax_ic_HX83102j.h` |
|---|---|---|
| `ic_adc_num` | `mov w9,#0x30` = 48 | `HX83102J_DATA_ADC_NUM 48` ✓ |
| `dsram_sz` | `mov w11,#0x2000; movk #0x1` = 73728 | `HX83102J_DSRAM_SZ 73728` ✓ |
| `isram_sz` | `+0x16<<12` = 163840 | `HX83102J_ISRAM_SZ 163840` ✓ |
| flash size | `mov w12,#0x3fc00` = 261120 | `HX83102J_FLASH_SIZE 261120` ✓ |

## 4. 面板参数 —— 发现 3 处不同（关键）

反汇编原厂 `himax_mcu_touch_information`（`0xffffffc010e23c00` 起），
按 `hx_s_ic_data` 偏移逐字段读：

| 偏移 | 字段 | 原厂立即数 | 原厂值 | 我们 papaya |
|---|---|---|---|---|
| +64 | `rx_num` | `#0x30` | 48 | 48 ✓ |
| +68 | `tx_num` | `#0x20` | 32 | 32 ✓ |
| +72 | `bt_num` | `wzr` | 0 | 0 ✓ |
| +84 | `max_pt` | `#0xa` | 10 | 10 ✓ |
| +88 | `int_is_edge` | `strb wzr` | **0** | 1 ✗ |
| +89 | `stylus_func` | `mov #0x1` | **1** | 0 ✗ |
| +90 | `stylus_id_v2` | `strb wzr` | 0 | 0 ✓ |
| +91 | `stylus_ratio` | `mov #0x1` | **1** | 10 ✗ |

字段归属由原厂日志串确认：
`@0xffffffc011d3dbd0` = `"[HXTP] %s:int_is_edge =%d,stylus_func = %d"`（打印 +88/+89），
`@0xffffffc011d725de` = `"[HXTP] %s:HX_STYLUS_ID_V2 =%d,HX_STYLUS_RATIO = %d"`（打印 +90/+91）。

**`stylus_func = 1` 就是压感笔的关键**：上游 papaya 配置为 0（关闭笔支持），
原厂为 1。我们之前照抄 papaya，等于把笔关掉了。

另外：原厂代码里**没有 `himax_differ_config` 分支**，rodata 里也没有
`huaying`/`lectron` 等模组串 → 原厂**未定义 `HXTP_DIFFER_CONFIG`**。

## 5. 固件/判据文件名 —— 原厂用固定名

| 原厂 rodata 字符串 | 上游 pad 驱动生成 |
|---|---|
| `Himax_firmware.bin` (0x1d5c068) | `Himax_firmware_<vendor>.bin` |
| `Himax_mpfw.bin` (0x1cbdccd) | `Himax_mp_firmware_<vendor>.bin` |
| `hx_criteria.csv` (0x1d413dd) | `hx_criteria_<vendor>.csv` |

原厂里 `%s_%s.bin` 模式串**不存在**，说明讯飞把名字写死了（单面板机型）。
设备 `/vendor/firmware` 里只有固定名文件，按 vendor 拼名会 `request_firmware` 失败。

## 6. 据此做的改动

| 文件 | 改动 |
|---|---|
| `hxchipset_pad/firmware_config/harrow/himax_firmware_config.h` | `int_is_edge` 1→0、`stylus_func` 0→1、`stylus_ratio` 10→1、移除 `HXTP_DIFFER_CONFIG` 与 `HIMAX_DEFAULT_FIRMWARE` |
| `hxchipset_pad/himax_common_interface.c` | 固件名改为固定 `Himax_firmware.bin` / `Himax_mpfw.bin` / `hx_criteria.csv` |

寄存器映射、IC 判定、IC 参数、RX/TX/`max_pt` 均**未改**（本来就与原厂一致）。

## 7. 自验证：编译产物与原厂逐指令比对

反汇编我们自己内核的 `himax_mcu_touch_information`：

```asm
mov w9, #0xa      // max_pt      = 10
mov w2, #0x30     // rx_num      = 48
mov w3, #0x20     // tx_num      = 32
strb wzr, [x8, #88]   // int_is_edge  = 0
mov w9, #0x1  -> [x8,#89]  // stylus_func  = 1
strb wzr, [x8, #90]   // stylus_id_v2 = 0
mov w9, #0x1  -> [x8, #91] // stylus_ratio = 1
```

与原厂指令序列**完全一致**。

## 8. 本次构建结果

```
Image : 31918592 bytes
uname -r : 5.4.210-tca-cycle
himax 符号 419 / ksymtab 10，含 himax_int_enable / himax_resume_by_lcd
固定固件名字符串 Himax_firmware.bin / Himax_mpfw.bin / hx_criteria.csv 均在镜像内
原厂模块 160 个 / 1753 依赖符号 / 未解析 0 → PASS
ramdisk md5 与原厂逐字节一致 e8e25cdef60aa27285a37fdeb5198431
```

## 9. 仍未确定的

- 原厂 `struct ic_setup_collect` 与我们的差 2 个 `_func_*` 字段（结构体布局差异），
  因为驱动按名字访问、各自自洽，**不影响寄存器行为**；但若将来要混用两边的
  `himax_common.c`/`himax_ic_core.c` 与 IC 文件，必须成对。
- 原厂/上游的 `HXTS_VENDOR_*` 模组名未在原厂 rodata 找到（`HXTP_DIFFER_CONFIG` 未定义时用途有限），
  现保留 papaya 的取值，仅影响 procfs 里显示的模组名。
- 触摸/笔是否完全可用仍需上机验证；若仍不行，`dmesg | grep -i hxtp` 里现在能看到
  固件名的实际请求结果，可直接判断是探测失败还是固件加载失败。
