# TCA Cycle Kernel - 充电优化可行性（Plan A：不改原厂 .ko）

## 1. 结论

原厂充电算法（含软件快充）跑在原厂模块 sprd-charger-manager.ko 里，Plan A 不改 .ko，所以算法本身动不了。
但 charger-manager 暴露了一整套**写权限 sysfs 节点**，运行时可以调：

- 降温：功率墙 / 充电电流墙 / 输入电流墙
- 延寿：限充（到百分比停充）/ 手动停充
- 温控：JEITA 开关
- 改不了：快充曲线与握手、旁路供电（本平台没实现成真旁路）

因此 tools/tca-tune.sh 新增 charge 子命令，全部默认不动，只在显式调用时生效，随时 restore。

## 2. 充电栈（全部原厂 .ko，只读使用）

- sprd-charger-manager.ko：充电状态机 + 软件快充算法（cm_sw_fast_charge_algo / cm_cp_ibus_step_algo / switch_charge_max_ibat）
- sprd_battery_info / sprd_tcpm / sc27xx_fuel_gauge / cw221x_battery
- bq2560x（主充 IC，支持 HIZ）/ bq2597x / sc8551（电荷泵）/ sgm415xx / sy65153

对应源码就在内核树 drivers/power/supply/，但设备上加载的是原厂 .ko，只能按它对外暴露的接口在运行时操作。
charger-manager 注册的 power_supply 名字是 battery（driver 里 psy_default.name），sysfs 组 charger.0 挂在它下面。

## 3. 可写节点

脚本自动扫描 /sys/class/power_supply/*/constant_charge_current 找到节点，不硬编码 battery。

| 节点 | 单位/取值 | 作用 | 备注 |
|---|---|---|---|
| battery/charge_control_limit | mW（>=2） | 热限功率 thm_pwr，CM 反算 thm_adjust_cur = thm_pwr / 适配器电压 | 写 0/1 会走 USB sink 分支，脚本已避开 |
| battery/constant_charge_current | uA | 充电电流墙（cm_charge_vote, MIN） | 只能压得更低；写大值 = 不约束 |
| battery/input_current_limit | uA | 输入电流墙（cm_charge_vote, MIN） | 同上 |
| battery/voltage_max_design | uV | 充电截止电压上限 | 可做"少充一点"延寿，风险高，暂不提供 |
| battery/charger.0/stop_charge | 0/1 | 1=停充，0=恢复 | 写 1 会置 externally_control=1，CM 主循环让位 |
| battery/charger.0/jeita_control | 0/1 | 1=开 JEITA 温度降流，0=关 | 关掉高温更热也更快，默认别动 |
| battery/charger.0/enable_power_path | 0/1 | 1=正常(HIZ off)，0=HIZ 高阻断输入 | **不是旁路供电**，别当边充边玩用 |
| battery/charger.0/support_fast_charge | 只读 | 是否支持快充 | |
| battery/charger.0/cp_num | 只读 | 电荷泵数量 | |

关键机制：

- constant_charge_current / input_current_limit 走 cm_charge_vote 的 SPRD_VOTE_CMD_MIN，只能"压得更低"，写大值等于不约束（原厂算法更低时以它为准）。
- stop_charge 写 1 时 charger-manager.c:6223 会置 sysfs->externally_control，4563 处 CM 主循环跳过该 charger；写 0 时 try_charger_enable(true) 并交回控制。所以可以安全地反复切换来做限充。
- charge_control_limit 的 0/1 是 USB sink 语义（cm_source_try_sink_limit_current），不是功率，脚本不会写 0/1。

## 4. tca-tune.sh charge 用法

    su -c "sh /data/adb/tca-tune.sh charge show"
    su -c "sh /data/adb/tca-tune.sh charge cool 15000 2000"   # 功率墙 15W + 电流墙 2A
    su -c "sh /data/adb/tca-tune.sh charge power 20000"       # 只设功率墙
    su -c "sh /data/adb/tca-tune.sh charge current 3000"      # 只设充电电流墙(mA)
    su -c "sh /data/adb/tca-tune.sh charge stop on|off"
    su -c "sh /data/adb/tca-tune.sh charge jeita on|off"
    su -c "sh /data/adb/tca-tune.sh charge limit 80"          # 80% 限充守护（后台轮询，3% 回差）
    su -c "sh /data/adb/tca-tune.sh charge limit stop"
    su -c "sh /data/adb/tca-tune.sh charge restore"           # 还原

- 默认一律不改；cool/power/current 会先把原值存到 /data/adb/tca-charge.saved。
- limit 守护每 20s 轮询 status/capacity：插电且 >= 上限写 stop_charge=1；低于（上限-3）写 0；拔电自动复位。pid 存在 /data/adb/tca-charge-limit.pid。
- 功率墙/电流墙是相对原厂算法的天花板，不是绝对值。

## 5. 不能做的

- 改快充曲线 / 握手 / 电荷泵分档：都在 sprd-charger-manager.ko 内（Plan A 不改 .ko）。
- 真·旁路供电 / 充电分离：本平台把 enable_power_path 实现成 BQ2560x 的 EN_HIZ（写 0 = 高阻断输入），不是"系统直接吃适配器"。
- 无线充、PD 档位：原厂状态机 + tcpm 管，无用户开关。

## 6. 风险与边界

- 功率/电流墙写太低：边玩边充可能掉电或充得极慢；写高无效果（不会超过原厂）。
- 长时间 stop_charge：CM 把该 charger 交给用户态，脚本在取消/拔电时会写回 0。
- 全部改动重启即失效，不写盘（除 saved 文件）。注意 stock 子命令不碰充电，充电还原用 charge restore。

## 7. 待真机确认

- /sys/class/power_supply/ 里 charger-manager 的 psy 名与 charger.0 是否如源码所示（脚本已自动发现）。
- charge_control_limit 写入后 cat /sys/kernel/debug/charger_manager 或 dmesg 里 thm_pwr / thm_adjust_cur 是否随之变化。
- limit 守护实测：插电到 80% 后 stop_charge 是否变 1、继续插着是否维持、拔插是否复位。
- 原厂是否会在插拔/重协商时把 externally_control 复位（若会，限充守护下一轮会重新置 1）。
