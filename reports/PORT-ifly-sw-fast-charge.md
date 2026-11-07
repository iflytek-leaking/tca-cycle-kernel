# PORT — iFlytek `cm_sw_fast_charge_algo`（CP 电荷泵软件快充闭环）

把 iFlytek/原厂 stock `sprd-charger-manager.ko` 里独有、Unisoc 基线源码没有的
**软件快充电流闭环** 逐字段还原进本仓库，使 se50（UMS9620 + sc8551 CP + PPS）的
`cm-pps-with-dcdc` 直充路径可用。

- 逆向依据：`REVERSE-cm-sw-fast-charge-algo.md`（A 子 agent，反汇编）
- 代码：本分支 `ifly-sw-fast-charge`
- 复查：`REVIEW-ifly-sw-fast-charge.md`
- 背景缺口：`REVERSE-charger-logic-audit.md`（唯一功能缺口即本算法）

## 1. 问题

Unisoc 基线 `sprd-charger-manager` 只跑 `cm_cp_tune_algo` 的电压/电流步进；
stock iFlytek 版本在 `pps_with_dcdc` 打开时额外运行 `cm_sw_fast_charge_algo`，
用「PPS 调压 + 2:1 电荷泵 + 软件 Ibat 闭环」把充电电流拉到 `charge-switch-current-microamp` 上限。

## 2. 改动清单（18 文件，+325/−29）

| 文件 | 改动 |
|---|---|
| `drivers/power/supply/charger-manager.c` | +164：`cm_sw_fast_charge_algo()`；`sw_charge_status` 写入；`switch_charge_max_ibat` 刷新；pps 分支 + `sw_alarm` 聚合；check_vbus 记账；JEITA `jeita_vbat`/`ovp_flag`；`charge_term_current`；DT 解析 |
| `drivers/power/supply/sprd_battery_info.c` | +6：解析 `charge-switch-current-microamp` idx0（get + _sqc） |
| `include/linux/power/charger-manager.h` | +13：`switch_charge_max_ibat`（charge_current）+ desc 尾部 9 字段 |
| `include/linux/power/sprd_battery_info.h` | +2：`switch_charge_max_ibat` |
| `include/linux/power/sprd_vote.h` | +2：`SPRD_VOTE_TYPE_IBAT_ID_IFLYTEK`（stock id=5） |

（其余 13 文件为仓库既有双 DSI / 触摸 / typec 移植，非本次改动。）

## 3. 算法要点（与 stock 逐行一致）

- 门控 `desc->sw_charge_status`（仅在 `cm_primary_charger_enable` 的 CALIBRATE 成功后置位）。
- 双闭环步长：过流 OCP（`ibat>target && ibus<=550000` → −1）与过压/回落 OVP（`vbat>=jeita_vbat` 锁存），
  叠加 `pps_reach_drop` 保护；`step=min`，`val=base+step*100000`。
- 提交门 `>99999 && <=switch_max && >=term+100000`，提交时写 `sw_charge_current`、
  `step==1` 使能主充、经 `cm_charge_vote->vote(type=1,id=5,cmd=1)` 投票。
- `cm_cp_tune_algo` 中：`target_ibat<switch_max+550000` → 关 DCDC/清 0；否则开 DCDC/`switch_max/2`；
  `sw_alarm` 作为第 6 步长参与 `cp_target_vbus` 聚合。

## 4. 构建

```sh
export PATH=<clang-r416183b>/bin:$PATH
cd kernel_ums9620_se50
make O=../se50-clang-out ARCH=arm64 LLVM=1 LLVM_IAS=1 \
     CROSS_COMPILE=aarch64-linux-gnu- KCFLAGS="-Wno-error" \
     drivers/power/supply/          # EXIT=0
```

产物：`sprd-charger-manager.ko`（1498296 B，含全部移植日志串）、`sprd_battery_info.ko`（283656 B）。

> 注意：子目录构建会重写 `se50-clang-out/Module.symvers`（截断）。本次已重建 symvers 并
> 对 4 个 consumer 的 `__versions` 按 provider 全量 CRC 定点改写；详见复查报告 §3。

## 5. ship 集

`s5-ship/modules/` 已更新 6 个：`sprd-charger-manager`、`sprd_battery_info`、
`sc27xx_fuel_gauge`、`sgm415xx-charger`、`bq2560x-charger`、`cw221x_battery`（均已 strip debug）。

CRC 校验（18 模块）：**跨模块符号 CRC 冲突 0，consumer↔provider 失配 0**。
关键值：`sprd_battery_get_battery_info=0x36d894fb`、`put=0x50216b27`、
`find_ocv2cap_table=0xd018edbb`、`find_resistance_table=0x87c75c4a`、`parse_battery_id=0x2183dd7c`。

加载顺序：`sprd_battery_info` → `sprd-charger-manager` 及 charger IC；见 `modules.load.charger`。

## 6. 验证状态

- [x] 编译 EXIT=0；日志串全部编入
- [x] 算法与反汇编 §1 逐行一致（22 项核对全 PASS）
- [x] ship 集 CRC 自洽
- [ ] 真机 insmod / dmesg / 实际快充功率
