# 熄屏/亮屏后触摸失效 — 根因与修复

**内核**：TCA Cycle Kernel 5.4.210 (ums9620_se50)
**现象**：开机触摸正常；熄屏再点亮后触摸完全无响应（不崩溃，纯静默失效）。
**结论**：`himax_chip_common_resume()` 少了一道 `irq_state` 判断，导致亮屏时提前返回，
既不重下 TDDI 固件也不重新使能中断。

---

## 1. 面板 ↔ 触摸的契约（来自原厂 `sprd-drm.ko` 反汇编）

全项目（160 个原厂 `.ko` + 全内核）**只有 `sprd-drm.ko` 引用 himax 符号**：

```
U himax_int_enable
U himax_resume_by_lcd
```

`sprd-drm.ko` 的 `hx83102_boe_option_mipi_power(w1)`（面板电源回调，`0x13948`）：

```
if (w1 == 0) return 0;                       // cbnz w1, ...
himax_int_enable(0);                         // 0x1399c —— 面板复位期间先关触摸中断
... 面板 SPI 初始化、大量 __const_udelay ...
gpiod_direction_output(rst, 0); ... gpiod_direction_output(rst, 1);
kthread_create_on_node(kthread_worker_fn, ..., "kthread-tp-resume", ...);
wake_up_process(...);                        // 0x13bcc
```

`kthread_lcd_resume_tp_func`（`0x13c40`，kthread 名 `kthread-tp-resume`）只做一件事：

```asm
13c4c: bl himax_resume_by_lcd
```

所以每次**面板上电**的完整序列是：

```
himax_int_enable(0)  →  面板复位  →  kthread-tp-resume: himax_resume_by_lcd()
```

**这是本机唯一的触摸 suspend/resume 通道。** `ztp_state_change.c` 里虽有一套
`tpd_lcd_notifier_callback`（`LCD_CMD_OFF`→suspend / `LCD_RESET/CMD_ON`→resume），但它整段在
`#ifdef CONFIG_TOUCHSCREEN_LCD_NOTIFY` 内，而本内核
`CONFIG_VENDOR_LCD_COMMON_FUNCTION is not set` → 该回调连同 `lcd_notify_register()` **都没编译**；
且全项目没有任何模块/代码调用 `lcd_notifier_call_chain()`。这条链在 SE50 上是死路。

---

## 2. 原厂 `himax_resume_by_lcd` / `himax_chip_common_resume` 的真实逻辑

原厂 Image 无 ELF 符号，通过 `__ksymtab`（本内核用**绝对 64 位指针**，24 字节一项：
`{value, name, namespace}`）反查得到：

| 符号 | ksymtab entry | cfi_jt | 函数地址 |
|---|---|---|---|
| `himax_int_enable` | `0x2095c30` | `0x16a205c` | `0xffffffc010e2d934` |
| `himax_resume_by_lcd` | `0x2095c78` | `0x16aafb0` | `0xffffffc010e275c8` |

（`BASE = 0xffffffc010080000`，与 `file_offset = vaddr - BASE` 一致）

原厂 `himax_resume_by_lcd` @ `0xda75c8`：

```asm
da75d4: adrp/add → "himax tp resume after lcd rst high\n"
da75dc: bl printk
da75e0: adrp x8, .bss ; ldr x0, [x8, #3336]    ; x0 = hx_s_ts
da75e8: cbz  x0, L_null                          ; if (!hx_s_ts) 打印 NULL 错
da75ec: bl   0xda7614                            ; himax_chip_common_resume(hx_s_ts)
```

**与我们 shim 的结构逐条一致** —— shim 本身没问题。

原厂 `himax_chip_common_resume` @ `0xda7614`：

```asm
da7628: ldrb w8, [x0, #2]        ; ts->suspended
da7630: cbnz w8, L_set_false     ; suspended != 0 -> ts->suspended = 0
        ; --- suspended == 0 分支 ---
da7634: adrp x8, .bss
da7638: ldr  x8, [x8, #3336]     ; x8 = hx_s_ts
da7648: ldr  w20, [x8, #496]     ; atomic_read(&ts->irq_state)   <<<< 关键 >>>>
da7654: bl   printk              ; "if_himax_int_enabled: irq_state = %d\n"
da7658: cmp  w20, #1
da765c: b.ne L_enter             ; irq_state != 1 -> 继续做完整 resume
da7670: bl   printk              ; "himax_chip_common_resume: Already resumed, skip...\n"
da7674: b    L_end
L_set_false:
da7678: strb wzr, [x19, #2]      ; ts->suspended = false
L_enter:
da767c: bl   printk              ; "himax_chip_common_resume: enter\n"
        ... 完整 resume（重下 FW / himax_int_enable(1)）...
```

即原厂语义为：

```c
if (ts->suspended == false) {
        if (atomic_read(&ts->irq_state) == 1) {   /* 中断本来就是开的 */
                I("%s: Already resumed, skip...", __func__);
                goto END;
        }
        /* irq_state == 0：中断已被 LCD 关掉，必须继续做完整 resume */
} else {
        ts->suspended = false;
}
```

**字符串取证**（`grep -a`）：

| 字符串 | 原厂 Image | 我们（修复前） |
|---|---|---|
| `if_himax_int_enabled` | ✅ | ❌ |
| `irq_state = %d` | ✅ | ❌ |
| `Already resumed, skip` | ✅ | ✅ |

字段偏移也吻合：`himax_int_enable()` 读写 `irq_state` 用的正是 `ts + 496`。

---

## 3. 根因

我们（从 F50 移植的 `hxchipset_pad`）版本的 `himax_chip_common_resume()` 是：

```c
if (ts->suspended == false) {
        I("%s: Already resumed, skip...\n", __func__);
        goto END;                          /* <-- 无条件放弃 */
} else {
        ts->suspended = false;
}
```

而面板路径**从不**调用 `himax_chip_common_suspend()`，`ts->suspended` 恒为 `false`。于是：

1. 熄屏（或面板上电）→ `sprd-drm.ko` 调 `himax_int_enable(0)` → `irq_state = 0`，`disable_irq()`；
2. 亮屏 → `kthread-tp-resume` → `himax_resume_by_lcd()` → `himax_chip_common_resume()`；
3. 进入即命中 `suspended == false` → **直接 goto END**：
   - 不执行 `hx_s_core_fp._resume_proc()` —— TDDI 复位后应重下的触摸固件没有下发；
   - 不执行 `himax_int_enable(1)` —— **中断永远不会重新打开**；
4. 结果：触摸彻底失效，且没有任何报错日志。

---

## 4. 修复

`drivers/vendor/common/touchscreen_v2/hxchipset_pad/himax_common.c` —
`himax_chip_common_resume()` 恢复原厂那道 `irq_state` 判断：

```diff
 	if (ts->suspended == false) {
+		if (atomic_read(&ts->irq_state) == 1) {
 			I("%s: Already resumed, skip...\n", __func__);
 			goto END;
+		}
 	} else {
 		ts->suspended = false;
 	}
```

这样：

* 面板路径（`suspended == false` 且 `irq_state == 0`）→ 走完整 resume：重下 TDDI 固件 +
  `himax_int_enable(1)`，触摸恢复；
* 真正无事可做的场景（中途被重复调用、中断本来就是开的）→ 仍然 skip，保持原行为；
* 内核 PM suspend 场景（`suspended == true`）→ 行为不变。

`himax_resume_by_lcd()` shim **不需要改动** —— 它与原厂逐条一致。

---

## 5. 顺带确认：ReSukiSU 已经完全正常

同一次 bugreport（`KernelSU_bugreport_2026-09-26_16_24.tar.gz`）证明 KSU 问题已解决：

```
Manager:    (v4.2.0-rc3, 35179)
KernelSU:   35181
ReSukiSU:   v4.2.0-rc3-20756bfb@ReSukiSU
KernelSU: ksu fd installed: 4, name: [ksu_driver], for pid 10475
KernelSU: allow root for: 10155
```

面板/触摸初始化本身也完全正常（同一日志）：

```
hx83102j_chip_detect:Read driver IC ID = 83, 10, 29
hx83102j_chip_detect:IC name = HX83102J
himax_mcu_touch_information:rx_num =48,tx_num =32
himax_mcu_touch_information:max_pt=10
Cusomer ID = SE50_BOE125
Project ID = 5635_IFLYTEK
i_update_FW: TP upgrade OK
S1@89, 1126 / E1@52, 1117          <- 真实触点，开机时可用
```

---

## 6. 附带发现

* `drivers/misc/himax_lcd_notify.o` 是早期 shim 的**陈旧对象**（`drivers/misc/Makefile`
  已无对应条目、源文件已删），未被链入；真正的 shim 在
  `touchscreen_v2/Makefile` 的 `vendor_tpd-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_PAD)` 列表里。
  本次已顺手从构建目录删除。
* 开机日志有一条残留告警：
  `himax_mcu_tp_info_check: STYLUS_RATIO, Set = 1 ; FW = 0` →
  `FIX_TOUCH_INFO does NOT match to FW information`。
  只影响压感笔 ratio，与本次熄屏问题无关，暂不动。
* 本机 SELinux 处于 **Permissive**。
