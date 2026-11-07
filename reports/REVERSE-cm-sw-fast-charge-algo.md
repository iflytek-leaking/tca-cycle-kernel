# 逆向报告：cm_sw_fast_charge_algo（iFlytek SW 快充算法）

目标：\`modules/sprd-charger-manager.ko\`（ARM64 可重定位，LTO+KCFI，局部符号保留）
- \`cm_sw_fast_charge_algo\` \`.text\` vaddr **0x8018**，size **0x2bc**（0x8018–0x82d4）
- 调用点：\`cm_cp_tune_algo\` \`.text\` **0x7b10**，\`R_AARCH64_CALL26 addend = .text+0x8018\`
- 参照源码：\`kernel_ums9620_se50/\`（下称 原厂）

> 说明：objdump 把很多 \`bl 0 <__cfi_check>\` 显示成 CFI 桩；真实目标一律用 \`readelf -r\` 的 addend 解析。
> 所有 \`.text\` 重定位在节 \`.rela.text.__cfi_check_fail\`（sh_info=12=\.text），偏移即 \`.text\` 内 vaddr。
> Ghidra 反编译用 \`/usr/lib/ghidra/support/analyzeHeadless\`（\`/usr/bin/ghidra\` 只是 GUI 启动器；Jython 不可用，改用 Java postScript）。

---

## 1. 伪 C 还原

**签名**：\`static void cm_sw_fast_charge_algo(struct charger_manager *cm)\`
- 参数：x0 = \`cm\`（\`struct charger_manager *\`）；\`x24 = [x0+0x18] = cm->desc\`（0x8034）。
- 返回类型 **void**：函数体内从未设置 w0，所有出口（0x8040–0x8058 的公共 epilogue、0x8274、0x82a8 等）直接恢复寄存器并 \`ret\`；调用方 \`cm_cp_tune_algo\` 也不使用 w0（0x7b14 立即覆盖 x8）。Ghidra 亦判为 \`void\`。
- 序言/尾声使用 Shadow Call Stack：\`str x30,[x18],#8\`（0x8018），\`ldr x30,[x18,#-8]!\`（0x8054/0x82 …）。

设：
\`\`\`
d = cm->desc
d+0x25c(604) cp_target_ibat      d+0x260(608) cp_target_vbat
d+0x278(632) sw_charge_current   d+0x27c(636) sw_charge_status (bool)
d+0x27d(637) ovp_flag (bool)     d+0x288(648) cp.vbat_uV
d+0x28c(652) cp.ibat_uA          d+0x290(656) cp.ibus_uA
d+0x2a0(672) jeita_vbat(jeita term voltage)
d+0x144(324) switch_charge_max_ibat
d+0x3dc(988) pps_with_dcdc       d+0x3e0(992) pps_reach_drop
d+0x3e4(996) charge_term_current (来自 battery_info+0x10)
d+0x38 (56)  psy_charger_stat
cm+0x10(16) dev                  cm+0x428(1064) cm_charge_vote (struct sprd_vote*)
\`\`\`

\`\`\`c
/* static void cm_sw_fast_charge_algo(struct charger_manager *cm) */
void cm_sw_fast_charge_algo(struct charger_manager *cm)
{
    struct charger_desc *d = cm->desc;
    int step_ocp, step_ovp, step, base, val;

    if (!d->sw_charge_status)              /* ldrb [d+636]; cbnz -> body; 0 -> ret */
        return;                            /* 0x8038-0x803c */

    /* --- 0x805c-0x8098 : 计算 step_ocp (w20) --- */
    if (d->cp.ibat_uA > d->cp_target_ibat && d->cp.ibus_uA <= 550000)
        step_ocp = -1;                     /* 0x8084 */
    else
        step_ocp = (d->sw_charge_current < d->switch_charge_max_ibat);  /* 0x808c-0x8098, cset lt */

    /* --- 0x809c-0x8120 : 计算 step_ovp (w21)，处理 ovp_flag --- */
    if (!d->ovp_flag) {                    /* 0x809c-0x80a4 */
        if (d->cp.vbat_uV >= d->jeita_vbat) {   /* 0x80a8-0x80b4 */
            d->ovp_flag = 1;                    /* 0x80d0-0x80d4 */
        } else {
            step_ovp = (d->sw_charge_current < d->switch_charge_max_ibat); /* 0x80b8-0x80c4 */
            goto have_steps;                    /* 0x80cc -> 0x8124 */
        }
    }
    dev_info(cm->dev, "%s, have switch ovp_flag, jeita_vbat %d\n",
             __func__, d->jeita_vbat);       /* 0x80dc-0x80f4, fmt .rodata+0x44e2, __func__ .rodata+0x2cb2 */

    if (d->cp.vbat_uV < d->jeita_vbat || d->cp.ibus_uA > 550000)
        step_ovp = 0;                       /* 0x8108-0x8120 */
    else
        step_ovp = -1;                      /* 0x8118 */

have_steps:
    /* --- 0x8124-0x814c : pps_reach_drop 保护 --- */
    if (d->pps_reach_drop != 0 &&
        d->cp.ibus_uA <= 550000 &&
        d->cp.vbat_uV >= d->cp_target_vbat - d->pps_reach_drop - 20000)
        step_ovp = -1;                      /* 0x827c */

    /* --- 0x8150-0x8194 : 计算候选电流 --- */
    base = min(d->sw_charge_current, d->switch_charge_max_ibat);   /* 0x8150-0x8180 */
    cm_vote_property(cm, d->cp_target_vbat + 200000,
                     d->psy_charger_stat,
                     POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX /*0x20*/); /* 0x8184 */

    step = min(step_ocp, step_ovp);         /* 0x818c-0x8190 */
    val  = base + step * 100000;            /* 0x8194 madd, w25=100000 */
    dev_info(cm->dev, ">>>>>>charge_current: %d\n", val);  /* 0x8198-0x81a4 */

    /* --- 0x81a8-0x824c : 提交或回退 --- */
    if (d->sw_charge_current > 99999 &&    /* 0x81a8-0x81b4 */
        val <= d->switch_charge_max_ibat &&/* 0x81b8-0x81c4 */
        val >= d->charge_term_current + 100000) {  /* 0x81c8-0x81d4 */
        d->sw_charge_current = val;        /* 0x81f4 */
        if (step == 1)
            cm_primary_charger_enable(cm, true);       /* 0x8204 */
        cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
                SPRD_VOTE_TYPE_IBAT /*1*/, 5 /*新的 iFlytek vote id*/,
                SPRD_VOTE_CMD_MIN /*1*/, d->sw_charge_current, cm);  /* 0x8238 blr */
        dev_info(cm->dev, ">>>>>>final sw_charge_current: %d\n",
                 d->sw_charge_current);     /* 0x823c-0x824c */
    } else {
        if (val < 100000)
            cm_primary_charger_enable(cm, false);      /* 0x81e8 */
    }

    dev_info(cm->dev,
      "\n>>>>>>>fast_charge_algo:step: %d\n"
      ">>>>>> step_otp_charge: %d\n"
      ">>>>>>>step_ocp_charge: %d\n"
      ">>>>>>>step_ovp_charge: %d\n"
      ">>>>>>>sw_charge_status: %d\n",
      step, 0 /*otp 硬编码 0*/, step_ocp, step_ovp, d->sw_charge_status); /* 0x8250-0x8274 */
}
\`\`\`

**分支/块与地址**
| 语义 | 地址 |
|---|---|
| entry, desc=cm+0x18 | 0x8034 |
| gate \`sw_charge_status\` | 0x8038–0x803c |
| step_ocp | 0x805c–0x8098 |
| ovp_flag / step_ovp | 0x809c–0x8120 |
| ovp 日志 | 0x80dc–0x80f4 |
| pps_reach_drop 保护 | 0x8124–0x814c；置 −1 在 0x8278–0x8280 |
| base=min + cm_vote_property | 0x8150–0x8184 |
| step=min + val=base+step*100000 | 0x8188–0x8194 |
| charge_current 日志 | 0x8198–0x81a4 |
| 提交判定 | 0x81a8–0x81d4 |
| 回退 \`cm_primary_charger_enable(false)\` | 0x81d8–0x81ec |
| 提交：写 d+632 / enable(true) / vote / final 日志 | 0x81f0–0x824c |
| fast_charge_algo 汇总日志 | 0x8250–0x8274 |
| CFI slowpath（间接调用） | 0x8284–0x82a8 |
| **死代码** dev_dbg 块（无任何分支指向 0x82ac） | 0x82ac–0x82d0 |

**关于 0x82ac 的死块**：它是 \`dev_dbg(cm->dev, "%s, vbat is %d, try decrease\n", __func__, vbat)\` 的动态调试 out-of-line 块（fmt=.rodata+0x1d5c，__func__=.rodata+0x2cb2，desc 仍用 x19=cm），随后 \`b 0x827c\`。全 \`.text\` 中没有任何分支目标是 0x82ac（已 grep 全量反汇编），因此静态不可达；它落在符号 size 0x2bc 之内，属于 LTO/冷块布局残留。

---

## 2. 字段映射

偏移为 stock .ko 实测；语义名给出 原厂 头文件对应或 stock 新增字段（stock \`charger_desc\` 布局与 原厂 不同，偏移全部由本 .ko 内有名函数的带标签日志交叉推出，未假设 原厂 偏移）。

| 访问 | 偏移 | 推得语义名 | 证据（原始指令地址） | 置信度 |
|---|---|---|---|---|
| \`ldrb w8,[x24,#636]\` | desc+0x27c | \`sw_charge_status\`（bool，算法总开关） | 自身最终日志把 d+636 打在 \`sw_charge_status\`（fmt .rodata+0x1b49，0x8254）；cm_cp_state_check_vbus 日志把 d+636 打在 \`sw_charge_status\`（0x6974/0x6978）；由 cm_primary_charger_enable 写（0x4e40） | **高** |
| \`ldrb w9,[x24,#637]\` | desc+0x27d | \`ovp_flag\`（bool） | 自身日志 \`have switch ovp_flag\`（0x80e0/0x80e4）；cm_update_current_jeita_status 清 0（0x2a1c/0x2a68） | **高** |
| \`ldr w8,[x24,#652]\` | desc+0x28c | \`cp.ibat_uA\` | cm_cp_tune_algo 日志 \`cp = [vbus/ibus/vbat/ibat]\`：ibat=d+652（0x7f2c/0x7f3c+.rodata+0xcd4） | **高** |
| \`ldr w9,[x24,#604]\` | desc+0x25c | \`cp.cp_target_ibat\` | 同上 \`cp_target\` 日志（0x7f68 .rodata+0x12b2）与 \`cp_max_ibat/jeita_ibat/target_ibat\` 日志（0x68e8–0x6904 .rodata+0x1d23） | **高** |
| \`ldr w8,[x24,#656]\` | desc+0x290 | \`cp.ibus_uA\` | cm_cp_tune_algo \`cp\` 日志：ibus=d+656（0x7f24/0x7f3c） | **高** |
| \`ldr w8,[x24,#632]\` | desc+0x278 | \`sw_charge_current\`（算法算出的、要 vote 出去的开关充电电流） | iFlytek 日志 \`charge_current\`/\`final sw_charge_current\`（0x81a0/0x8240）；由 cm_cp_state_check_vbus 写（0x696c）、cm_cp_tune_algo 写（0x7a90/0x7ad4/…） | **中**（语义高，确切 stock 成员名未知，为新增字段） |
| \`ldr w9,[x24,#324]\` | desc+0x144 | \`desc->switch_charge_max_ibat\` | cm_update_charge_info 日志最后一项 \`desc->switch_charge_max_ibat = %d\`（fmt .rodata+0x53a4，0x2454/0x2470） | **高** |
| \`ldr w4,[x24,#648]\` | desc+0x288 | \`cp.vbat_uV\` | cm_cp_tune_algo \`cp\` 日志：vbat=d+648（0x7f28/0x7f3c） | **高** |
| \`ldr w8,[x24,#672]\` | desc+0x2a0 | \`jeita_vbat\`（JEITA 终止电压，µV） | 自身日志 \`jeita_vbat %d\`（0x80e0/0x80e4）；cm_manager_adjust_current 把 jeita 表 \`term_volt\` 写入 d+672（0x2e9c，日志 .rodata+0x1a6a \`...target_cur=%d term_volt=%d\`） | **高**（语义）；stock 新增字段名 \`jeita_vbat\`（自身日志即此名） |
| \`ldr w8,[x24,#608]\` | desc+0x260 | \`cp.cp_target_vbat\` | cm_cp_tune_algo \`cp_target\` 日志（0x7f68 .rodata+0x12b2）；cm_cp_state_check_vbus 等 | **高** |
| \`ldr w9,[x8,#992]\` | desc+0x3e0 | \`desc->pps_reach_drop\` | DT 解析 \`cm-pps-reach-drop\` → d+0x3e0（0x8f14–0x8f2c，name .rodata+0x2085），日志 .rodata+0x5672 | **高** |
| \`ldr w8,[x8,#996]\` | desc+0x3e4 | 电池电流阈值（µA），来自 \`sprd_battery_info+0x10\`；最可能 \`charge_term_current_ua\` | charger_manager_probe：\`ldr w8,[sp,#320]\`(=info+16) → \`str w8,[desc+996]\`（0xa224/0xa22c）；info 缓冲由 \`sprd_battery_get_battery_info\` 填充于 0xa04c–0xa064 | **中/低**（见下注） |
| \`ldr x2,[x8,#56]\` | desc+0x38 | \`desc->psy_charger_stat\`（\`const char **\`） | 作为 \`cm_vote_property\` 的 \`name\` 传入（0x815c/0x8184）；cm_primary_charger_enable 解引用 \`[desc+56][0]\` 作 psy 名（0x4df4–0x4e04） | **高** |
| \`ldr x0,[x19,#1064]\` +264 fn | cm+0x428 | \`cm->cm_charge_vote\`（\`struct sprd_vote *\`），+0x108 = \`->vote\` | 间接调用类型 id=\`.text+0x13128\`=\`__typeid__ZTSFiP9sprd_votebiiiiPvE_global_addr\`，与 \`include/linux/power/sprd_vote.h:103\` 的 \`int (*vote)(struct sprd_vote*, bool, int, int, int, int, void*)\` 完全一致；调用形态同 原厂 \`charger-manager.c:4179-4188\` | **高** |
| \`ldr x0,[x19,#16]\` | cm+0x10 | \`cm->dev\` | \`_dev_info\` 第一参数（0x80dc 等）；原厂 \`charger_manager\` dev@0x10 | **高** |

**desc+996 的推导（附原始地址）**：
- \`charger_manager_probe\` 在 0xa04c–0xa064：\`memset(sp+0x130,0,0x520)\` → \`sprd_battery_get_battery_info(psy, sp+0x130)\`，即 info 基址 = \`sp+0x130\`。
- 0xa1e0–0xa22c 的 \`info→desc\` 拷贝中，\`[sp,#320]\`=\`info+0x10\` 被写入 \`desc+0x3e4\`（0xa224 \`ldr w8,[sp,#320]\`；0xa22c \`str w8,[x9,#996]\`）。
- 以同一拷贝块的其他项做标定：\`info+0x20\`→\`desc+0x204(516)=internal_resist\`（0xa06c–0xa08c，除以 1000 的魔数），原厂 中该值是 \`factory_internal_resistance_uohm\`（原厂 头文件 \`sprd_battery_info.h:120\`，偏移 0x24）；\`info+0x18\`→\`desc+0x2cc(716)=ir_comp.us\` 且→\`cp_target_vbat\`（0xa090–0xa0f4），原厂 中该值是 \`constant_charge_voltage_max_uv\`（原厂 偏移 0x1c）。⇒ **stock \`struct sprd_battery_info\` 相比 原厂 头文件整体前移 4 字节**。因此 \`info+0x10\` 对应 原厂 的 \`charge_term_current_ua\`（0x14）或 \`precharge_current_ua\`（0x10），取决于被删字段位于 0x10 之前还是 0x10–0x18 之间；两者都是 µA 电池电流。语义上该值用作接受新 SW 电流的下界 \`val >= d+996 + 100000\`（0x81c8–0x81d4），最像充电终止电流。
- 确切成员名 **中等偏低置信**（stock 头文件不在工作区，原厂 头文件不同）。

---

## 3. 调用的 helper（readelf -r addend 解析）

| 调用地址 | 真实目标（CALL26 addend） | 原厂 \`charger-manager.c\` | 说明 |
|---|---|---|---|
| 0x80f4 / 0x81a4 / 0x824c / 0x8270 | \`_dev_info\` | **不存在**（内核 \`dev_info\`/dev_printk 实现，非本 .c 函数） | 4 处日志 |
| 0x8184 | \`.text+0x2ab8\` = \`cm_vote_property\` | **存在，line 1840** | \`cm_vote_property(cm, cp_target_vbat+200000, psy_charger_stat, 0x20)\` |
| 0x81e8 / 0x8204 | \`.text+0x4dc4\` = \`cm_primary_charger_enable\` | **存在，line 1390** | false/true |
| 0x8238 | 间接 \`cm->cm_charge_vote->vote\` → 实现 \`sprd_vote_func\` \`sprd_vote.c:108\`；注册于 \`charger-manager.c:7563\` | 间接调用；回调不在本 .c | 原厂 调用形态见 \`charger-manager.c:4179-4188\` |
| 0x829c | \`__cfi_slowpath\` | **不存在**（内核 KCFI 运行时） | slowpath |
| 0x82c8 | \`__dynamic_dev_dbg\` | **不存在**（内核 dynamic debug） | 仅在死块 0x82ac |
| （自身） | — | \`cm_sw_fast_charge_algo\` **在 原厂 charger-manager.c 中缺失** | 无任何同名/等价函数 |

> 注：objdump 对 0x8184/0x81e8/0x8204/0x8238/0x829c/0x82c8 全部显示为 \`<__cfi_check>\`，系 LTO 局部符号重定位被 objdump 误标；以上目标均以 addend 为准。

---

## 4. \`charger_manager_probe\` 中 \`cm-ifly-charge-control\` 的解析语义

在 \`charger_manager_probe\`（被内联的 \`of_cm_parse_desc\` 区段）：

\`\`\`
8ee8: x0=x20(np)  x1=.rodata+0x5246("cm-pps-with-dcdc")  x2=desc+0x3dc(988)  w3=1  x4=0
8ef0: bl of_property_read_variable_u32_array      ; desc+988 = pps_with_dcdc
8f14: x0=np  x1=.rodata+0x2085("cm-pps-reach-drop")  x2=desc+0x3e0(992)  w3=1  x4=0
8f2c: bl of_property_read_variable_u32_array      ; desc+992 = pps_reach_drop
8f48: x0=np  x1=.rodata+0x30dc("cm-ifly-charge-control")  x2=0
8f58: bl of_find_property
8f5c: cbnz x0, 95e0                              ; 属性存在 -> 95e0
8f60: strb wzr, [x21, #1000]                     ; 不存在 -> desc+0x3e8(1000)=0
...
95e0: x1=.rodata+0x30f3("Set ifly_charge_control\n")
95e4: w8=1
95f0: strb w8, [x21, #1000]                      ; 存在 -> desc+1000 = 1
95f4: bl _dev_err                                 ; 打印 "Set ifly_charge_control"
\`\`\`

**结论**：\`cm-ifly-charge-control\` 是**仅存在性（boolean）解析**，不读值：\`of_find_property(np, "cm-ifly-charge-control", NULL)\` 非 NULL 时把 **\`desc+0x3e8(1000)\` 这个字节置 1**，并 \`dev_err\` 打印 \`"Set ifly_charge_control\n"\`（0x95e0–0x95f8）；属性缺失时在 0x8f60 置 0。
- 设置对象：\`struct charger_desc\` 的偏移 **+1000**（1 字节 bool，未在 原厂 头文件出现，stock 新增，可命名 \`desc->ifly_charge_control\`）。
- 它不是 \`desc+636\`（\`sw_charge_status\`）：本函数入口门是 636，与 DT 属性无直接写入关系。
- se50 DT 中该属性为空属性：\`se50.dts:368\` \`cm-ifly-charge-control;\`，同节点 \`cm-pps-with-dcdc=<1>\`、\`cm-pps-reach-drop=<0x7530>\`。
- 该 flag 的消费点：\`charger_stop_store\` 0xc8d4–0xc908 读 \`[cm->desc+1000]\`，非 0 时打印 \`.rodata+0x45fa\`（\`"ifly_charge_control, will decrease input current for aging..."\`）并 \`cm_vote_property(cm, 0, psy_charger_stat, 0x25 /*INPUT_CURRENT_LIMIT*/)\`；另见 0x1080。

---

## 5. \`cm_sw_fast_charge_algo\` 在 \`cm_cp_tune_algo\` 中的调用位置/方式

**调用方式**（原始指令）：
\`\`\`
7af4: ...
7b08: mov w27, #-40000          ; sw_alarm 步长候选
7b0c: mov x0, x19              ; x19 = cm
7b10: bl  cm_sw_fast_charge_algo   ; addend .text+0x8018，仅传 cm
7b14: ldr x8, [x19, #24]       ; 返回值被忽略（无 mov w0 使用）
7b18: ...                      ; 继续算 vbat/ibat/vbus/ibus 各 step
\`\`\`
- 参数：只有 \`x0 = cm\`；**无第二个参数**。
- 返回值：被忽略（本函数为 void）。
- 调用是**条件**的：\`cm_cp_tune_algo\` 开头 \`ldr w8,[desc+0x3dc(988)]\`（0x7984 附近 / Ghidra：d+0x3dc）。若 \`pps_with_dcdc == 0\`，0x7a40–0x7a44 令 w27=60000 并 \`b 0x7b18\`，**跳过调用**；只有 \`pps_with_dcdc != 0\` 才走 0x7a48…0x7b0c 并在 0x7b10 调用。也就是说 iFlytek SW 快充只在 “PPS with DCDC” 配置下生效。

**相对 原厂 源码的位置**：原厂 \`cm_cp_tune_algo\`（line 3092）没有 pps_with_dcdc 分支，其 “check taper done” 块（line 3103-3114）之后直接是 \`cm_cp_vbat_step_algo/ibat/vbus/ibus\`（3117-3127）。stock 在那个 taper 块之后、step 聚合之前插入了一整段 pps_with_dcdc/DCDC 使能逻辑，\`cm_sw_fast_charge_algo\` 就位于该段末尾（等价于 原厂 **line 3114 与 3117 之间**）。之后 stock 才计算各 step 并聚合。

**stock \`cm_cp_tune_algo\` 与 原厂 的其它差异**（除新增调用外）：
1. 头部改为以 \`desc+0x2d0(720)\` 选择 taper 参考（\`d+0x2c\` 或 \`d+0x260\`），并用 \`d+0x2a0(672)=jeita_vbat\` 对 \`cp_target_vbat\` 做 clamp 后回写（0x798c–0x79bc）；原厂 直接比较 \`cp->vbat_uV >= cp->cp_target_vbat-50000\`（line 3104）。
2. taper 计数用 \`d+0x2a8(680)\`、阈用 \`d+0x28c(652)=ibat_uA\`、\`d+0x2b4(692)\`（0x79e0–0x7a30）；原厂 用 \`cp->cp_taper_current\`、\`cp->cp_taper_trigger_cnt\`。
3. 新增 pps_with_dcdc 处理：\`d+988\` 分支里根据 \`d+604/d+324+550000\` 调用 \`cm_primary_charger_enable()\` 使能/关闭 DCDC，写 \`d+632\`（0x7a48–0x7ad4），日志 \`.rodata+0x4fb6\` / \`.rodata+0x3f2f\`。
4. 新增 w27 = “sw_alarm”步长计算（0x7ad8–0x7bc0，阈值 \`d+652/d+656/d+660/d+648\` ×1000，步长 ±40000）。
5. step 计算内联且有更多阈值（10000/20000/40000/60000，边界 0xc351/0x61a9/0x186a1/0xf4241/0x1e8481/0x249f1/0x493e1），原厂 调用 \`cm_cp_vbat_step_algo\` 等独立函数（stock 中这些符号不存在）。
6. 聚合 6 项（多 \`sw_alarm\`），日志改为 \`"%s, tune_step = [vbus:%d ibus:%d vbat:%d ibat:%d alarm:%d sw_alarm:%d]"\`（.rodata+0x3f67，0x7fac），原厂 只有 5 项并直接打 \`vbatt/ibatt/...\`（line 3152-3161）。
7. 最终 \`cp\` 日志多出 \`pps_with_dcdc: %d\`（.rodata+0xcd4，0x7f50）。

---

## 6. 带单位常量与日志字符串

**cm_sw_fast_charge_algo 内常量**
| 立即数 | 原值 | 单位/含义 | 地址 |
|---|---|---|---|
| \`0x86471\` | 550001（源码 \`> 550000\`） | **550000 µA** = 550 mA，与 \`cp.ibus_uA\`(d+656) 比较 | 0x8064/0x806c；比较 0x807c,0x8110,0x8130 |
| \`0x186a0\` | 100000 | **100000 µA** = 100 mA；既作步进乘子（0x8194）又作阈值（0x81cc/0x81d8） | 0x8168/0x817c |
| \`0x1869f\` | 99999（=100000−1，源码 \`> 99999\`） | 100 mA 门限，对 \`sw_charge_current\` | 0x81ac/0x81b0 |
| \`0x30d40\` | 200000 | **200 mV**，\`cp_target_vbat + 200000\` 作为 CONSTANT_CHARGE_VOLTAGE_MAX 投票值 | 0x8160/0x8164/0x8170 |
| \`0xffffb1e0\` | −20000 | **−20000 µV** = −20 mV（pps_reach_drop 保护） | 0x8140/0x8144 |
| \`0x20\` | 32 | \`POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX\` | 0x8174 |
| vote 参数 | (1,1,5,1) | \`SPRD_VOTE_TYPE_IBAT=1\`、\`enable=true\`、**vote_type_id=5**、\`SPRD_VOTE_CMD_MIN=1\` | 0x8224–0x8230 |
| \`-1\` | −1 | step 值 | 0x8084/0x8118/0x827c |

> \`vote_type_id=5\` 在 原厂 \`sprd_vote.h\` 中等于 \`SPRD_VOTE_TYPE_IBAT_ID_MAX\`（越界），说明 stock 的 vote id 枚举新增了第 6 项用于 iFlytek（原厂 参考源缺此枚举项）。

**字符串（.rodata addend）**
| addend | 字符串 | 用处 |
|---|---|---|
| 0x2cb2 | \`cm_sw_fast_charge_algo\` | \`__func__\`（0x80e8、0x82b8） |
| 0x44e2 | \`%s, have switch ovp_flag, jeita_vbat %d\n\` | 0x80e4 |
| 0x4700 | \`>>>>>>charge_current: %d\n\` | 0x8198 |
| 0x13c | \`>>>>>>final sw_charge_current: %d\n\` | 0x8244 |
| 0x1b49 | \`\n>>>>>>>fast_charge_algo:step: %d\n>>>>>> step_otp_charge: %d\n>>>>>>>step_ocp_charge: %d\n>>>>>>>step_ovp_charge: %d\n>>>>>>>sw_charge_status: %d\n\` | 0x8258 |
| 0x1d5c | \`%s, vbat is %d, try decrease\n\` | 0x82b4（死块 dev_dbg） |
| 0x30dc | \`cm-ifly-charge-control\` | DT 属性（0x8f48） |
| 0x30f3 | \`Set ifly_charge_control\n\` | 0x95e0 |
| 0x45fa | \`ifly_charge_control, will decrease input current for aging...\` | 0xc8e4 |
| 0x5246 | \`cm-pps-with-dcdc\` | DT（0x8ed8） |
| 0x2085 | \`cm-pps-reach-drop\` | DT（0x8f14） |
| 0x53a4 | \`... desc->switch_charge_max_ibat = %d\n\` | 命名 d+324（0x245c） |
| 0x48f9 | \`volt tune ok, cp_target_ibat %d switch_charge_max_ibat %d sw_charge_status %d\n\` | 命名 d+604/d+324/d+636（0x6978） |
| 0xcd4 | \`%s, cp = [vbus:%duV ibus:%duA vbat:%duV ibat:%duA], ucp_cnt = %d, pps_with_dcdc: %d\n\` | 命名 d+660/656/648/652/688/988（cm_cp_tune_algo 0x7f50） |
| 0x12b2 | \`%s, cp_target = [vbus:%duV last_vbus:%duV ibus:%duA vbat:%duV ibat:%duA], jeita_status = %d\n\` | 命名 d+616/620/612/608/604/664（0x7f80） |
| 0x1a6a | \`add jeita_status = %d, target target_cur = %d term_volt = %d\n\` | 命名 d+664/668/672（cm_manager_adjust_current 0x2ea4） |
| 0x1d23 | \`%s, cp_max_ibat = %d, jeita_ibat = %d, target_ibat = %d\n\` | 命名 d+624/668/604（0x68e8） |

---

## 附：关键地址速查
- \`cm_sw_fast_charge_algo\` 入口 0x8018；gate 0x8038；ret 0x8058；死块 0x82ac。
- \`cm_vote_property\` .text 0x2ab8；\`cm_primary_charger_enable\` .text 0x4dc4；
  \`cm_cp_tune_algo\` .text 0x7958；\`charger_manager_probe\` .text 0x85e0；
  \`__typeid__ZTSFiP9sprd_votebiiiiPvE_global_addr\` .text 0x13128。
- 目标调用点 0x7b10（addend .text+0x8018）。
- DT 解析：0x8ed8(cp-pps-with-dcdc)、0x8f14(cp-pps-reach-drop)、0x8f48/0x95e0(cp-ifly-charge-control)。

---

## §7 生命周期

> 追加证据来源：同产物 \`modules/sprd_battery_info.ko\`（未 strip，\`sprd_battery_get_battery_info\` @0x364），它用同一份 stock 头文件填充 \`struct sprd_battery_info\`，可用来给偏移定名。

### 7.1 sw_charge_status (desc+0x27c/636) 生命周期

**全 \`.text\` 只有一个写点**：\`cm_primary_charger_enable\` 0x4e40。
- 0x4e08 \`mov w20, w1\`（enable 实参）。
- 0x4e14 \`and w8, w20, #0x1\` → 0x4e24 \`str w8, [sp]\` → 0x4e28 \`power_supply_set_property(psy, 0x44 /*POWER_SUPPLY_PROP_CALIBRATE*/, &val)\`。
- 0x4e38 \`cbnz w21, 4e90\`：**set_property 失败则不写**。
- 0x4e3c \`and w8, w20, #0x1\`；**0x4e40 \`strb w8, [x23, #636]\`**（x23=desc）。即 \`d->sw_charge_status = enable & 1\`，无其它条件、无锁。
- 前置门槛（任一不满足都不写）：0x4df4/0x4e00 \`desc+56(psy_charger_stat)\` 或 \`[psy_charger_stat]\` 为 NULL 直接返回；0x4e10 \`power_supply_get_by_name\` 失败走 0x4e74。
- 结论：\`sw_charge_status\` 只镜像最近一次**成功**的 primary-charger \`CALIBRATE\` 写。

读点（不写）：0x6974（cm_cp_state_check_vbus 日志）、0x7a54（cm_cp_tune_algo pps 分支）、0x7d8c（cm_cp_tune_algo step）、0x8038（cm_sw_fast_charge_algo 门）、0x8254（汇总日志）。

### 7.2 sw_charge_current (desc+0x278/632) 生命周期

**初始化**：本 .ko 内无显式初始化；\`charger_desc\` 由分配时清零，故初值 0（全量 grep 无其它写点）。写点共 4 个：

1. **cm_cp_state_check_vbus 0x696c** \`str w9, [x24, #632]\`
   - 0x691c w9=\`d+324\`(switch_charge_max_ibat)；0x6920 w8=\`d+604\`(cp_target_ibat)；0x6924 w9 += 0x86470(550000)；0x6928 \`cmp\`；0x692c \`b.le 6968\`。
   - 若 \`cp_target_ibat <= sw_max+550000\` → 0x6968 w9=0 → **d+632=0**。
   - 否则（>）：0x6930 打印 .rodata+0x3f2f "target ibat %d when pps_with_dcdc, try enable dcdc"，0x6954 \`cm_primary_charger_enable(cm,1)\`，0x695c w9=\`d+324\` → **d+632=sw_max**。
   - 0x6974 只是**读** d+636 供日志，不写它。

2. **cm_cp_tune_algo 0x7a90** \`str wzr, [x26, #632]\`（清零）
   - 条件：\`cp_target_ibat(d+604) < sw_max(d+324)+550000\`（0x7a48-0x7a64）**且** \`sw_charge_status(d+636)!=0\`（0x7a68）；先 0x7a8c 关闭 DCDC（enable 0），再置 0。

3. **cm_cp_tune_algo 0x7ad4** \`str w8, [x26, #632]\`（=\`sw_max/2\`）
   - 条件：\`cp_target_ibat >= sw_max+550000\`（0x7a64 → 0x7a98）**且** \`sw_charge_status==0\`；先 0x7abc 打开 DCDC（enable 1）；w8=0x7ac0 \`d+324\` → 0x7ac8 \`cmp #0\` → 0x7acc \`cinc\`(有符号修正) → 0x7ad0 \`asr #1\` → 0x7ad4 写 \`sw_max/2\`。

4. **cm_sw_fast_charge_algo 0x81f4** \`str w23, [x24, #632]\`（提交候选 \`val = min(sw,sw_max)+step*100000\`）。

读点：0x69a4（vote）、0x7d94（cm_cp_tune_algo step）、0x808c/0x80b8/0x8150/0x81a8/0x8220/0x8240。

### 7.3 switch_charge_max_ibat (desc+0x144/324)

- **唯一写点**：\`cm_update_charge_info\` **0x2340** \`str w12, [x21, #324]\`，w12=\`d+824\`（0x2330 \`ldr w12,[x21,#824]\`）。
- 条件：\`cp.cp_running(d+596)\` 为真且 \`cp.recovery(d+598)\` 为假（0x2318/0x231c/0x2320/0x2324）——即 \`charger_type==CM_CHARGER_TYPE_ADAPTIVE\` 分支（对应 原厂 charger-manager.c:1707）。同块还写 d+316=d+820、d+320=d+816、d+328/332=\`[d+828]\`(8B)、d+336=d+836。
- \`d+824\` 的来源：\`charger_manager_probe\` 0xa1a0/0xa1a8 从 \`struct sprd_battery_info\` **+0x21c(540)** 拷入；而 \`sprd_battery_info.ko\` 在 0x9a4–0x9b8 用 DT 属性 **"charge-switch-current-microamp"**（.rodata+0x8c4）idx0 写入该 +0x21c。故 \`d+824 = info.cur.switch_charge_max_ibat\`。
- 0x2454 是**日志读**（\`ldr w16,[x21,#324]\`；0x2470 \`str w16,[sp,#72]\`），由 0x245c \`dev_info\`（fmt .rodata+0x53a4）打印为 \`desc->switch_charge_max_ibat = %d\`。
- **与 原厂 字段的对应**：\`switch_charge_max_ibat\` 是 stock 新增字段，**不等于** 原厂 \`desc->cp.cp_max_ibat\`（后者=stock d+624，来自 info+0x218(536)=cur.flash_cur）；也**不是** \`desc->cm_charge_current\`（那是指针，charger-manager.h:773）。它与 \`charge_limit_cur\`(d+316)/\`input_limit_cur\`(d+320) 同块，由 battery-info 的 \`cur\` 里厂商新增的 "charge-switch-current" 字段喂入，仅被 \`cm_sw_fast_charge_algo\` 当作 SW 电流上界。

### 7.4 desc+0x3e4/996 来源确认

**probe 拷贝块**（info 基址 = sp+0x130；0xa050 \`memset(...,0,0x520)\`；0xa064 \`sprd_battery_get_battery_info(psy,&info)\`）在 0xa06c–0xa22c 的完整 info→desc 列表（除特别注明，\`ldr [sp,#N]\`/\`str [desc+M]\`）：

| info 偏移 | desc 偏移 | 指令 |
|---|---|---|
| +0x00 | （未拷贝） | — |
| +0x10 | +0x3e4(996) | 0xa224 \`ldr w8,[sp,#320]\` → 0xa22c \`str w8,[x9,#996]\` |
| +0x18(24) | +0x2cc(716), +0x2c(44), +0x260(608) | 0xa090/0xa0d0/0xa0f0 |
| +0x1c(28) | +0x30(48) | 0xa0d8/0xa0e0 |
| +0x20(32) | +0x204(516)（÷1000） | 0xa06c–0xa08c |
| +0x24(36) | +0x250(592) | 0xa0e4/0xa0ec |
| +0x2c(44) | +0x18(24) | 0xa200/0xa208 |
| +0x30(48) | +0x1c(28) | 0xa20c/0xa214 |
| +0x34(52) | +0x20(32) | 0xa218/0xa220 |
| +0x1ec…+0x238 (492…568) | +0x308…+0x354 (776…852) | 0xa118–0xa1fc |
| +0x214(532) | +0x270(624 \`cp_max_ibat\`)、+0x274(628) | 0xa0f8/0xa10c |
| +0x218(536) | +0x25c(604)、+0x334(820) | 0xa0f8/0xa194 |
| +0x21c(540) | +0x330(816)、+0x338(824) | 0xa10c/0xa1a0 |
| +0x220…+0x22c | +0x33c…+0x340 | 0xa1ac–0xa1c0 |
| +0x510(1296) | +0x2f0(752) | 0xa09c/0xa0a4 |
| +0x514(1300) | +0x2f4(756) | 0xa0c4/0xa0cc |

**原厂 \`include/linux/power/sprd_battery_info.h\` 成员顺序**（行号）：
103 \`charge_full_design_uah\` → 104 \`charge_full_uah\` → 106 \`voltage_min_design_uv\` → 108 \`batt_ovp_threshold_uv\` → **110 \`precharge_current_ua\`** → **112 \`charge_term_current_ua\`** → 114 \`constant_charge_current_max_ua\` → 116 \`constant_charge_voltage_max_uv\` → 118 \`fullbatt_voltage_offset_uv\` → 120 \`factory_internal_resistance_uohm\` → 122 \`fast_charge_ocv_threshold_uv\`。

**判定（高置信）**：\`sprd_battery_info.ko\` 同一构建的解析函数给出 DT 属性→info 偏移：
- 0x668 "charge-full-design-microamp-hours" → info+0x00
- 0x684 "voltage-min-design-microvolt" → info+0x04
- 0x6a0 "batt-ovp-threshold-microvolt" → info+0x08
- 0x6bc **"precharge-current-microamp" → info+0x0c**
- 0x6d8 **"charge-term-current-microamp" → info+0x10**
- 0x6f4 "constant-charge-current-max-microamp" → info+0x14
- 0x710 "constant-charge-voltage-max-microvolt" → info+0x18
- 0x72c "fullbatt-voltage-offset-microvolt" → info+0x1c

交叉验证：charger .ko 的 info+0x18→\`cp_target_vbat/ir_comp.us\`(=cv_max)、info+0x20→\`internal_resist\`(factory)、info+0x24→\`fchg_ocv\`，与上表一致。
⇒ stock 相对 原厂 少了一个开头的 \`charge_full_uah\`（line 104），故 **info+0x10 = \`charge_term_current_ua\`（头文件 line 112）**，而 \`precharge_current_ua\` 是 info+0x0c（line 110）。因此 **desc+996 = \`charge_term_current_ua\`**，用作 iFlytek 算法接受新 SW 电流的下界。

### 7.5 cm_cp_tune_algo pps_with_dcdc 分支伪 C（0x7984–0x7b18）

\`\`\`c
/* x26 = cm->desc, x19 = cm; 0x7984 起 */
if (d->d720 == 0) goto L7fb8;                             /* 0x7984/0x7988 */

/* --- jeita clamp of cp_target_vbat --- */
v = d->cp_target_vbat;                                    /* 0x7990 */
if (d->jeita_vbat > 0)                                    /* 0x7994/0x79a4 */
    v = min(v, d->jeita_vbat);                            /* 0x79a0/0x79a8 */
d->cp_target_vbat = v;                                    /* 0x79b8 */

/* --- taper-done --- */
if (d->cp.vbat_uV < v - 50000 &&                          /* 0x79ac-0x79bc */
    !(d->pps_reach_drop != 0 &&                           /* 0x79c0-0x79c8 */
      d->cp.vbat_uV > v - d->pps_reach_drop - 20000))     /* 0x79cc-0x79dc */
    goto L7a34;
if (d->cp.ibat_uA < d->d692) {                            /* 0x79e0-0x79ec */
    if (++d->d680 > 6) { d->d680 = 0; return 1; }         /* 0x79f0-0x7a2c */
} else {
    d->d680 = 0;                                          /* 0x7a30 */
}

L7a34:  /* ===== pps_with_dcdc ===== */
if (d->pps_with_dcdc == 0) {                              /* 0x7a34/0x7a38/0x7a3c */
    sw_alarm = 60000;                                     /* 0x7a40 */
    goto L7b18;                                           /* 0x7a44 — 跳过算法调用 */
}
if (d->cp_target_ibat < d->switch_charge_max_ibat + 550000) {   /* 0x7a48-0x7a64 */
    if (d->sw_charge_status) {                            /* 0x7a54/0x7a68 */
        dev_info(cm->dev, "%s, target ibat %d is too low for pps_with_dcdc, try disable dcdc\n",
                 "cm_cp_tune_algo", d->cp_target_ibat);   /* 0x7a6c-0x7a80, .rodata+0x4fb6 */
        cm_primary_charger_enable(cm, 0);                 /* 0x7a84-0x7a8c */
        d->sw_charge_current = 0;                         /* 0x7a90 */
    }
} else {
    if (!d->sw_charge_status) {                           /* 0x7a98 */
        dev_info(cm->dev, "%s, target ibat %d when pps_with_dcdc, try enable dcdc\n",
                 "cm_cp_tune_algo", d->cp_target_ibat);   /* 0x7a9c-0x7ab0, .rodata+0x3f2f */
        cm_primary_charger_enable(cm, 1);                 /* 0x7ab4-0x7abc */
        d->sw_charge_current = d->switch_charge_max_ibat / 2;  /* 0x7ac0-0x7ad4 */
    }
}
/* --- sw_alarm (w27) --- */
if (d->d168 * 1000 < d->cp.ibus_uA ||                     /* 0x7ad8-0x7af0 */
    d->d160 * 1000 < d->cp.ibat_uA) {                     /* 0x7af4-0x7b04 */
    sw_alarm = -40000;                                    /* 0x7b08 */
} else {
    /* 0x7b04 b.le -> 0x7b8c (在 0x7b18 之后)：*/
    if (d->d164 * 1000 < d->cp.vbus_uV) sw_alarm = -40000;/* 0x7b8c-0x7ba8 */
    else sw_alarm = (d->d156 * 1000 < d->cp.vbat_uV) ? 60000 : -40000; /* 0x7ba8-0x7bbc */
}
cm_sw_fast_charge_algo(cm);                               /* 0x7b0c-0x7b10 (addend .text+0x8018) */
L7b18:  /* 0x7b18 起继续 vbat/ibat/vbus/ibus 各 step 计算与聚合 */
\`\`\`

- 只有在 \`pps_with_dcdc != 0\` 且未在 taper-done 提前 \`return 1\` 时才可能到达 0x7b0c；\`pps_with_dcdc == 0\` 时 0x7a44 直接跳 0x7b18，**不调用** \`cm_sw_fast_charge_algo\`。
- 调用点只传 x0=cm（0x7b0c \`mov x0,x19\`），返回值忽略（0x7b14 立刻覆盖 x8）。

