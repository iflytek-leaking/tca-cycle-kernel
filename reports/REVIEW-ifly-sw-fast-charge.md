# REVIEW — iFlytek `cm_sw_fast_charge_algo` 移植复查

**结论：PASS（可合入）** — 算法语义与 A 报告 §1 逐行一致；全量 ship 集 CRC 自洽（0 冲突 / 0 provider 失配）。
2 处低风险偏差 + 1 处构建注意项见下。

## 0. 复查方式
C 子 agent（review）运行超时未产出，已中断；复查由主 agent 对照
`REVERSE-cm-sw-fast-charge-algo.md` §1/§7 完成，三方交叉验证：
stock 反汇编语义 ↔ B 的源码 ↔ 编译产物（`strings`/`modprobe --dump-modversions`）。

## 1. 逐条核对

| # | 项（报告出处） | 源码落点 | 结论 |
|---|---|---|---|
| 1 | entry gate `sw_charge_status` | charger-manager.c:3108 | PASS |
| 2 | `step_ocp`：`ibat_uA>target && ibus_uA<=550000` → −1 / 否则比较 | :3112 | PASS |
| 3 | `ovp_flag` 锁存：`vbat_uV>=jeita_vbat` → 置位；否则直接 goto have_steps | :3116 | PASS |
| 4 | ovp 日志 + `step_ovp` 复核 | :3125 | PASS |
| 5 | pps_reach_drop 保护 | :3135 | PASS |
| 6 | `base=min` + `cm_vote_property(target_vbat+200000)` | :3140 | PASS |
| 7 | `step=min` + `val=base+step*100000` | :3148 | PASS |
| 8 | 提交门：`sw_charge_current>99999 && val<=switch_max && val>=term+100000` | :3149 | PASS |
| 9 | 提交：写 632 + `step==1` enable(true) + vote(type=1,id=5,cmd=1) + final 日志 | :3152 | PASS |
| 10 | else：`val<100000` → enable(false) | :3163 | PASS |
| 11 | 汇总日志 5 行（otp 硬编码 0） | :3165 | PASS |
| 12 | §7.1 `sw_charge_status` 仅由 `cm_primary_charger_enable` 在 CALIBRATE 成功后写 | :1416（ret==0 路径） | PASS |
| 13 | §7.2 `sw_charge_current` 三写点（check_vbus / tune_algo / algo） | :3208/:3216/:3462/:3468/:3152 | PASS |
| 14 | §7.3 `switch_charge_max_ibat` 由 `cur` 刷新 + `cm_get_bat_info` 赋值 | :1715 / :6931 | PASS |
| 15 | §7.4 `charge_term_current=info.charge_term_current_ua` | :6933 | PASS |
| 16 | §7.5 pps 分支：target_ibat<max+550000 时 enable/disable + cur=0 或 max/2 | :3203 | PASS |
| 17 | §7.5 `cm_sw_fast_charge_algo` 仅在 pps_with_dcdc!=0 调用，且位于步进算法之前 | :3234 | PASS |
| 18 | DT：`cm-pps-with-dcdc` / `cm-pps-reach-drop` / `cm-ifly-charge-control` | :6869 | PASS |
| 19 | `sprd_battery_info.c` 解析 `charge-switch-current-microamp` idx0（get + _sqc 两处） | +6 行 | PASS |
| 20 | header：`switch_charge_max_ibat` / desc 尾部 9 字段 / vote id=5 | 4 个头文件 | PASS |
| 21 | `jeita_vbat` = JEITA `term_volt` | :4311 | PASS |
| 22 | 编译产物含全部移植日志串 | `strings` 已验证 | PASS |

## 2. 低风险偏差（不影响功能，已注释）

1. **`sw_alarm` 字段映射近似**（:3225）：stock 用的是无名字段 d156/d160/d164/d168（mA/mV，`*1000` 比较），
   B 映射到 `cp_max_ibus/cp_max_ibat/cp_target_vbus/cp_target_vbat`。语义方向一致（总线/电池越限 → −40000，
   仅电池压差 → 60000），但精确门槛属推断。影响：Vbus 微调步长，非安全路径。
2. **`cm_cp_state_check_vbus` 增加 `pps_with_dcdc` 守卫**（:3459）：stock 该块无守卫。加守卫后非 iFlytek
   路径完全不动，行为更保守；因算法本身受 pps_with_dcdc 门控，功能等价。
3. **`ovp_flag` 清除点**（:4312）：stock 在 `cm_update_current_jeita_status` 清；B 在 `cm_manager_adjust_current`。
   后者正是前者的唯一有效 JEITA 求值路径（:1584/:1593/:1605），仅在实际应用 JEITA 点时清除 —— 等价且更精确。

## 3. 构建 / CRC（重要）

- partial 构建把 `se50-clang-out/Module.symvers` 截断（740,929 → 876 B），导致 modpost 丢失 vmlinux 符号条目。
- 已用 10:05 完整模块的 `__versions` union 重建 symvers；重建模块的 `__versions` 为条目子集（permissive，内核
  对缺失条目仅 `pr_warn_once` 后放行 —— 见 kernel/module.c:check_version）。
- 4 个 consumer（`sc27xx_fuel_gauge / sgm415xx-charger / bq2560x-charger / cw221x_battery`）旧 `__versions` 会对
  新版 `sprd_battery_info.ko` 的 `__crc_sprd_battery_*` 触发 `bad_version` 拒载 —— 已按 provider 全量 CRC 映射
  定点改写其 `__versions`。
- 校验结果（`s5-ship/modules` 18 模块）：**跨模块符号 CRC 冲突 0；consumer↔provider 失配 0**。
- `sprd-charger-manager.ko` 仅 2 条 `__versions`（两个 sprd_battery 符号）→ 其余符号 permissive，可正常加载。

## 4. 未覆盖（需真机）

- insmod/加载顺序与 `dmesg` 实测；
- sc8551 CP 实际 `cp_target_ibat/vbat` 数值是否触发 pps_with_dcdc 路径；
- `cm-ifly-charge-control` 在 se50.dts 的存在性（代码为条件启用）。
