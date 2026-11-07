#!/system/bin/sh
# ============================================================
#  TCA Cycle Kernel - 运行时调优脚本
#  用法:  su -c "sh /data/adb/tca-tune.sh <命令>"
#    show     打印当前状态
#    smooth   滑动跟手优先（日常推荐）
#    game     游戏帧率优先
#    net      只调网络（BBR + fq_codel）
#    io       只调 I/O（BFQ + 预读/队列）
#    audio    音频低延迟（RT 节流 + 大核地板 + 音频 IRQ 绑核）
#    sched    调度模型 uclamp 档位
#    touch    触摸调优（Boost/uclamp/IRQ）
#    charge   充电（限流/功率墙/限充/停充/JEITA，走原厂模块）
#    mem      内存诊断 / 停止旧守护（不解除已有组和限制）
#    gc       ART 属性诊断 / 移除旧 GC 属性（需完整重启）
#    stock    还原到调优前
#  不刷机；常规调优写 /proc/sys 和 /sys，并在 /data/adb 保存部分原值。
#  GC 回滚会修改脚本同目录的 system.prop；旧 mem 设置不自动还原。
# ============================================================
set -u
SAVED=/data/adb/tca-tune.saved

w() {
  [ -e "$1" ] || return 0
  cur=$(cat "$1" 2>/dev/null)
  [ "$cur" = "$2" ] && return 0
  echo "$2" > "$1" 2>/dev/null && echo "  $1 = $2  (was $cur)"
}
ws() { w "/proc/sys/$1" "$2"; }

save() {
  [ -f "$SAVED" ] && return 0
  : > "$SAVED"
  for k in kernel/sched_wakeup_granularity_ns kernel/sched_min_granularity_ns \
           kernel/sched_migration_cost_ns kernel/sched_schedstats kernel/sched_energy_aware \
           kernel/sched_rt_runtime_us kernel/sched_rt_period_us \
           vm/swappiness vm/vfs_cache_pressure vm/dirty_ratio vm/dirty_background_ratio \
           vm/watermark_scale_factor \
           net/ipv4/tcp_congestion_control net/core/default_qdisc net/ipv4/tcp_fastopen \
           net/core/rmem_max net/core/wmem_max; do
    [ -e "/proc/sys/$k" ] && echo "$k=$(cat /proc/sys/$k)" >> "$SAVED"
  done
  for b in /sys/block/*/queue/scheduler; do
    [ -e "$b" ] && echo "$b=$(sed "s/.*\[\(.*\)\].*/\1/" $b)" >> "$SAVED"
  done
  for g in top-app foreground background system-background restricted; do
    for f in uclamp.min uclamp.max; do
      p=/dev/cpuctl/$g/$f
      [ -e "$p" ] && echo "$p=$(cat $p)" >> "$SAVED"
    done
  done
  for p in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$p" ] || continue
    for f in scaling_governor scaling_min_freq scaling_max_freq; do
      [ -e "$p/$f" ] && echo "$p/$f=$(cat $p/$f)" >> "$SAVED"
    done
  done
  echo "  (原始值已存到 $SAVED)"
}

do_show() {
  echo "--- CPU 调频 ---"
  for p in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$p" ] && echo "  $p gov=$(cat $p/scaling_governor 2>/dev/null) cur=$(cat $p/scaling_cur_freq 2>/dev/null) min=$(cat $p/scaling_min_freq 2>/dev/null) max=$(cat $p/scaling_max_freq 2>/dev/null)"
  done
  echo "--- 调度器 ---"
  for k in sched_wakeup_granularity_ns sched_min_granularity_ns sched_migration_cost_ns sched_latency_ns sched_schedstats sched_energy_aware; do
    [ -e /proc/sys/kernel/$k ] && echo "  kernel.$k = $(cat /proc/sys/kernel/$k)"
  done
  echo "--- 实时/空闲 ---"
  for k in sched_rt_runtime_us sched_rt_period_us; do
    [ -e /proc/sys/kernel/$k ] && echo "  kernel.$k = $(cat /proc/sys/kernel/$k)"
  done
  [ -e /sys/devices/system/cpu/cpuidle/current_governor ] && echo "  cpuidle governor = $(cat /sys/devices/system/cpu/cpuidle/current_governor 2>/dev/null)"
  [ -f /data/adb/tca-audio-hold.pid ] && echo "  cpu_dma_latency 锁定 = pid $(cat /data/adb/tca-audio-hold.pid)"
  echo "--- I/O ---"
  for b in /sys/block/*/queue/scheduler; do [ -e "$b" ] && echo "  $b = $(cat $b)"; done
  for d in /sys/block/sd*/queue/read_ahead_kb; do [ -e "$d" ] && echo "  $d = $(cat $d)"; done
  echo "--- 内存 ---"
  for k in swappiness vfs_cache_pressure dirty_ratio dirty_background_ratio watermark_scale_factor; do
    [ -e /proc/sys/vm/$k ] && echo "  vm.$k = $(cat /proc/sys/vm/$k)"
  done
  echo "--- 网络 ---"
  echo "  cc      = $(cat /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null)"
  echo "  qdisc   = $(cat /proc/sys/net/core/default_qdisc 2>/dev/null)"
  echo "  可选 cc = $(cat /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null)"
  echo "--- zram ---"
  if [ -d /sys/block/zram0 ]; then echo "  zram0 size=$(cat /sys/block/zram0/disksize 2>/dev/null) algo=$(cat /sys/block/zram0/comp_algorithm 2>/dev/null)"; else echo "  (未启用)"; fi
}

do_net() {
  echo "== 网络：BBR + fq_codel =="
  ws net/ipv4/tcp_congestion_control bbr
  ws net/core/default_qdisc fq_codel
  ws net/ipv4/tcp_fastopen 3
  ws net/core/rmem_max 6291456
  ws net/core/wmem_max 6291456
  for i in $(ls /sys/class/net 2>/dev/null); do
    [ "$i" = "lo" ] && continue
    if command -v tc >/dev/null 2>&1; then tc qdisc replace dev "$i" root fq_codel 2>/dev/null && echo "  tc: $i -> fq_codel"; fi
  done
}

do_io() {
  echo "== I/O：BFQ + 预读/队列 =="
  for b in /sys/block/*/queue/scheduler; do
    [ -e "$b" ] && grep -q bfq "$b" 2>/dev/null && w "$b" bfq
  done
  for q in /sys/block/sd*/queue /sys/block/mmcblk*/queue /sys/block/dm-*/queue; do
    [ -d "$q" ] || continue
    w "$q/read_ahead_kb" 512
    w "$q/nr_requests" 128
    w "$q/rq_affinity" 2
  done
}

do_smooth() {
  save
  echo "== 滑动跟手（日常推荐）=="
  ws kernel/sched_wakeup_granularity_ns 1000000
  ws kernel/sched_min_granularity_ns   1500000
  ws kernel/sched_migration_cost_ns     250000
  ws kernel/sched_schedstats 0
  ws kernel/sched_energy_aware 1
  ws vm/swappiness 100
  ws vm/vfs_cache_pressure 100
  ws vm/dirty_ratio 20
  ws vm/dirty_background_ratio 5
  ws vm/watermark_scale_factor 150
  do_io
  do_net
}

do_game() {
  do_smooth
  echo "== 游戏：大核锁 performance =="
  i=0
  for p in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$p" ] || continue
    if [ "$i" -ge 1 ]; then
      w "$p/scaling_governor" performance
    else
      max=$(cat "$p/cpuinfo_max_freq" 2>/dev/null)
      [ -n "$max" ] && w "$p/scaling_min_freq" $((max / 2))
    fi
    i=$((i + 1))
  done
}

do_stock() {
  echo "== 还原 =="
  [ -f "$SAVED" ] || { echo "  没保存过，无需还原"; return 0; }
  while IFS="=" read -r k v; do
    case "$k" in
      /*) w "$k" "$v" ;;
      *)  ws "$k" "$v" ;;
    esac
  done < "$SAVED"
  rm -f "$SAVED"
}

TOUCH_DIR=/sys/touchscreen

touch_resolve() {
  [ -e "$TOUCH_DIR/touch_boost_enable" ] && return 0
  f=$(ls /sys/bus/spi/devices/*/touch_boost_enable 2>/dev/null | head -1)
  [ -n "$f" ] && TOUCH_DIR=$(dirname "$f")
}

do_touch_show() {
  touch_resolve
  echo "--- 触摸调优 (默认关闭) ---"
  if [ -e "$TOUCH_DIR/touch_boost_enable" ]; then
    echo "  boost enable=$(cat $TOUCH_DIR/touch_boost_enable) freq=$(cat $TOUCH_DIR/touch_boost_freq)kHz tail=$(cat $TOUCH_DIR/touch_boost_ms)ms"
    echo "  irq   affinity=$(cat $TOUCH_DIR/touch_irq_affinity) cpu=$(cat $TOUCH_DIR/touch_irq_cpu)"
    echo "  idle  latency=$(cat $TOUCH_DIR/touch_idle_latency_us 2>/dev/null || echo n/a)us  # 触摸期间锁浅 C-state,0=关"
    echo "  uclamp enable=$(cat $TOUCH_DIR/touch_uclamp_enable) pct=$(cat $TOUCH_DIR/touch_uclamp_pct) path=$(cat $TOUCH_DIR/touch_uclamp_path)"
    echo "  节点: $TOUCH_DIR"
  else
    echo "  (这个内核没有触摸调优节点)"
  fi
}

do_touch() {
  touch_resolve
  [ -e "$TOUCH_DIR/touch_boost_enable" ] || { echo "  没有触摸调优节点"; return 1; }
  case "$1" in
    on|enable)
      echo "== 触摸 Boost: 开 =="
      w "$TOUCH_DIR/touch_boost_enable" 1
      echo "  boost freq=$(cat $TOUCH_DIR/touch_boost_freq)kHz tail=$(cat $TOUCH_DIR/touch_boost_ms)ms"
      echo "  想再绑触摸 IRQ 到大核: $0 touch affinity 1" ;;
    boost) w "$TOUCH_DIR/touch_boost_enable" "$2"; echo "  boost=$(cat $TOUCH_DIR/touch_boost_enable 2>/dev/null)" ;;
    off|disable)
      echo "== 触摸 Boost + uclamp + IRQ 绑核: 关 =="
      w "$TOUCH_DIR/touch_uclamp_enable" 0
      w "$TOUCH_DIR/touch_irq_affinity" 0
      w "$TOUCH_DIR/touch_boost_enable" 0 ;;
    all)
      echo "== 触摸 Boost + uclamp + IRQ 绑核: 全开 =="
      w "$TOUCH_DIR/touch_boost_enable" 1
      w "$TOUCH_DIR/touch_uclamp_enable" 1
      w "$TOUCH_DIR/touch_irq_affinity" 1 ;; 
    uclamp)  w "$TOUCH_DIR/touch_uclamp_enable" "$2"; do_touch_show ;;
    uclamp_pct) w "$TOUCH_DIR/touch_uclamp_pct" "$2"; echo "  uclamp_pct=$(cat $TOUCH_DIR/touch_uclamp_pct)" ;;
    freq)  w "$TOUCH_DIR/touch_boost_freq" "$2"; echo "  freq=$(cat $TOUCH_DIR/touch_boost_freq)kHz" ;;
    ms)    w "$TOUCH_DIR/touch_boost_ms" "$2";   echo "  tail=$(cat $TOUCH_DIR/touch_boost_ms)ms" ;;
    affinity) w "$TOUCH_DIR/touch_irq_affinity" "$2"; echo "  affinity=$(cat $TOUCH_DIR/touch_irq_affinity) cpu=$(cat $TOUCH_DIR/touch_irq_cpu)" ;;
    cpu)   w "$TOUCH_DIR/touch_irq_cpu" "$2";    echo "  cpu=$(cat $TOUCH_DIR/touch_irq_cpu)" ;;
    idle)  w "$TOUCH_DIR/touch_idle_latency_us" "$2"; echo "  idle_latency=$(cat $TOUCH_DIR/touch_idle_latency_us)us  # 0=关 建议 100~300" ;;
    *)     do_touch_show ;;
  esac
}

CPUCTL=/dev/cpuctl

do_sched_show() {
  echo "--- 当前 uclamp（cpuctl, 百分比）---"
  for g in top-app foreground background system-background restricted; do
    [ -e "$CPUCTL/$g/uclamp.min" ] && echo "  $g min=$(cat $CPUCTL/$g/uclamp.min) max=$(cat $CPUCTL/$g/uclamp.max)"
  done
  echo "  sched_clamp_min2boost = $(cat /proc/sys/kernel/sched_clamp_min2boost 2>/dev/null)"
}

cu() {
  w "$CPUCTL/$1/uclamp.min" "$2"
  w "$CPUCTL/$1/uclamp.max" "$3"
}

do_sched() {
  echo "== 调度模型（参考 Uperf: uclamp 档位 + clamp_min2boost）=="
  ws kernel/sched_clamp_min2boost 1
  case "$1" in
    balance)
      cu top-app 20 max;  cu foreground 5 max;  cu background 0 60
      cu system-background 0 60; cu restricted 0 40 ;;
    performance)
      cu top-app 60 max;  cu foreground 40 max; cu background 0 max
      cu system-background 0 max; cu restricted 0 80 ;;
    fast)
      cu top-app max max; cu foreground 60 max; cu background 0 max
      cu system-background 0 max; cu restricted 0 max ;;
    powersave)
      cu top-app 10 max;  cu foreground 0 max;  cu background 0 30
      cu system-background 0 30; cu restricted 0 20 ;;
    *) do_sched_show; return ;;
  esac
  do_sched_show
}

AUDIO_HOLD=/data/adb/tca-audio-hold.pid

audio_idle_on() {
  if [ -f "$AUDIO_HOLD" ] && kill -0 "$(cat "$AUDIO_HOLD" 2>/dev/null)" 2>/dev/null; then
    echo "  cpu_dma_latency 已经是 0 (pid $(cat "$AUDIO_HOLD"))"
    return 0
  fi
  sh -c 'exec 3<>/dev/cpu_dma_latency 2>/dev/null || exit 1; printf "\000\000\000\000" >&3; exec sleep 86400' &
  echo $! > "$AUDIO_HOLD"
  echo "  已锁 cpu_dma_latency=0 (pid $(cat "$AUDIO_HOLD"))，禁深 C-state"
}

audio_idle_off() {
  if [ -f "$AUDIO_HOLD" ]; then
    kill "$(cat "$AUDIO_HOLD" 2>/dev/null)" 2>/dev/null
    rm -f "$AUDIO_HOLD"
    echo "  已释放 cpu_dma_latency"
  else
    echo "  没有在锁"
  fi
}

do_audio_irq() {
  echo "  --- 音频 IRQ -> 大核 (best effort) ---"
  grep -iE 'mcdt|vbc|agdsp|audio|sprd.*dma|sprd-dma' /proc/interrupts 2>/dev/null | while read -r line; do
    irq=$(echo "$line" | cut -d: -f1 | tr -d ' ')
    case "$irq" in ''|*[!0-9]*) continue ;; esac
    if [ -w "/proc/irq/$irq/smp_affinity_list" ]; then
      echo 4-7 > "/proc/irq/$irq/smp_affinity_list" 2>/dev/null && echo "    irq $irq -> cpu4-7"
    elif [ -w "/proc/irq/$irq/smp_affinity" ]; then
      echo f0 > "/proc/irq/$irq/smp_affinity" 2>/dev/null && echo "    irq $irq -> 0xf0"
    fi
  done
}

do_audio() {
  case "$1" in
    idle)
      case "$2" in
        off) audio_idle_off ;;
        *)   audio_idle_on ;;
      esac
      return ;;
    off) do_stock; return ;;
  esac
  save
  echo "== 音频低延迟 =="
  ws kernel/sched_rt_runtime_us 995000
  ws kernel/sched_schedstats 0
  ws kernel/sched_wakeup_granularity_ns 1000000
  ws kernel/sched_migration_cost_ns 250000
  echo "  --- 大核频率地板 1.0GHz ---"
  i=0
  for p in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$p" ] || continue
    if [ "$i" -ge 1 ]; then
      max=$(cat "$p/cpuinfo_max_freq" 2>/dev/null)
      floor=1000000
      [ -n "$max" ] && [ "$floor" -gt "$max" ] && floor=$max
      w "$p/scaling_min_freq" "$floor"
    fi
    i=$((i + 1))
  done
  do_audio_irq
  echo "  想禁深 C-state: $0 audio idle on   （会费电，用完 audio idle off）"
}



# ============ 充电（原厂 charger-manager，全程运行时可调，默认不动） ============
CHARGE_SAVED=/data/adb/tca-charge.saved
CHARGE_LIMIT_PID=/data/adb/tca-charge-limit.pid
PS_DIR=""

ps_find() {
  PS_DIR=""
  for d in /sys/class/power_supply/*; do
    [ -w "$d/constant_charge_current" ] && { PS_DIR="$d"; return 0; }
  done
  for d in /sys/class/power_supply/*; do
    [ -d "$d/charger.0" ] && { PS_DIR="$d"; return 0; }
  done
  return 1
}

ps_get() { cat "$PS_DIR/$1" 2>/dev/null; }

ps_set() {
  [ -w "$PS_DIR/$1" ] || return 0
  cur=$(ps_get "$1")
  [ "$cur" = "$2" ] && return 0
  echo "$2" > "$PS_DIR/$1" 2>/dev/null && echo "  $1 = $2  (was $cur)"
}

do_charge_show() {
  if ! ps_find; then echo "  (没找到 charger-manager 的 battery 节点)"; return 1; fi
  echo "--- 充电节点 $PS_DIR ---"
  echo "  status=$(ps_get status)  capacity=$(ps_get capacity)%  health=$(ps_get health)  temp=$(ps_get temp)"
  echo "  voltage_now=$(ps_get voltage_now)uV  current_now=$(ps_get current_now)uA"
  echo "  充电电流墙 constant_charge_current=$(ps_get constant_charge_current)uA"
  echo "  输入电流墙 input_current_limit=$(ps_get input_current_limit)uA"
  echo "  功率墙     charge_control_limit=$(ps_get charge_control_limit)mW"
  echo "  截止电压   voltage_max_design=$(ps_get voltage_max_design)uV"
  if [ -d "$PS_DIR/charger.0" ]; then
    echo "  charger.0: stop_charge=$(cat "$PS_DIR/charger.0/stop_charge" 2>/dev/null)  jeita=$(cat "$PS_DIR/charger.0/jeita_control" 2>/dev/null)  fast=$(cat "$PS_DIR/charger.0/support_fast_charge" 2>/dev/null)"
  fi
  [ -f "$CHARGE_LIMIT_PID" ] && echo "  限充守护 pid=$(cat "$CHARGE_LIMIT_PID" 2>/dev/null)"
}

charge_save() {
  ps_find || return 1
  [ -f "$CHARGE_SAVED" ] && return 0
  : > "$CHARGE_SAVED"
  for f in constant_charge_current input_current_limit charge_control_limit; do
    [ -e "$PS_DIR/$f" ] && echo "$f=$(cat "$PS_DIR/$f")" >> "$CHARGE_SAVED"
  done
  echo "  (充电原始值 -> $CHARGE_SAVED)"
}

charge_limit_kill() {
  [ -f "$CHARGE_LIMIT_PID" ] || return 0
  kill "$(cat "$CHARGE_LIMIT_PID" 2>/dev/null)" 2>/dev/null
  rm -f "$CHARGE_LIMIT_PID"
  echo "  已停限充守护"
}

# 轮询容量：插电时 >= 上限就停充，低于 上限-3 再放行；拔电恢复
charge_limit_loop() {
  LIM="$1"
  while :; do
    st=$(cat "$PS_DIR/status" 2>/dev/null)
    c=$(cat "$PS_DIR/capacity" 2>/dev/null)
    s=$(cat "$PS_DIR/charger.0/stop_charge" 2>/dev/null)
    case "$st" in
      Charging|Full)
        if [ -n "$c" ] && [ "$c" -ge "$LIM" ]; then
          [ "$s" = 1 ] || echo 1 > "$PS_DIR/charger.0/stop_charge"
        elif [ -n "$c" ] && [ "$c" -le $((LIM - 3)) ]; then
          [ "$s" = 0 ] || echo 0 > "$PS_DIR/charger.0/stop_charge"
        fi ;;
      *) [ "$s" = 0 ] || echo 0 > "$PS_DIR/charger.0/stop_charge" ;;
    esac
    sleep 20
  done
}

do_charge() {
  case "${1:-show}" in
    show|"")
      do_charge_show ;;
    cool)
      ps_find || { echo "  没找到充电节点"; return 1; }
      charge_save
      mw="${2:-15000}"; ma="${3:-2000}"
      echo "== 充电降温：功率墙 ${mw}mW + 电流墙 ${ma}mA =="
      ps_set charge_control_limit "$mw"
      ps_set constant_charge_current $((ma * 1000))
      ps_set input_current_limit $((ma * 1000))
      echo "  还原: $0 charge restore"
      do_charge_show ;;
    power)
      ps_find || return 1; charge_save
      ps_set charge_control_limit "${2:-15000}"
      echo "  功率墙 = $(ps_get charge_control_limit)mW   还原: $0 charge restore" ;;
    current)
      ps_find || return 1; charge_save
      ps_set constant_charge_current $(( ${2:-2000} * 1000 ))
      echo "  充电电流墙 = $(ps_get constant_charge_current)uA   还原: $0 charge restore" ;;
    stop)
      ps_find || return 1
      case "${2:-}" in
        on|1)  w "$PS_DIR/charger.0/stop_charge" 1 ;;
        off|0) w "$PS_DIR/charger.0/stop_charge" 0 ;;
        *) echo "  stop_charge=$(cat "$PS_DIR/charger.0/stop_charge" 2>/dev/null)  (1=停充, 0=充电)" ;;
      esac ;;
    jeita)
      ps_find || return 1
      case "${2:-}" in
        on|1)  w "$PS_DIR/charger.0/jeita_control" 1 ;;
        off|0) w "$PS_DIR/charger.0/jeita_control" 0 ;;
        *) echo "  jeita_control=$(cat "$PS_DIR/charger.0/jeita_control" 2>/dev/null)  (1=开, 0=关)" ;;
      esac ;;
    limit)
      ps_find || return 1
      case "${2:-}" in
        stop|off)
          charge_limit_kill
          [ -e "$PS_DIR/charger.0/stop_charge" ] && w "$PS_DIR/charger.0/stop_charge" 0 ;;
        ""|*[!0-9]*) echo "用法: $0 charge limit <20-100|stop>" ;;
        *)
          LIM="$2"
          charge_limit_kill
          echo "== 限充 ${LIM}%（插电生效，3% 回差）=="
          charge_limit_loop "$LIM" &
          echo $! > "$CHARGE_LIMIT_PID"
          echo "  守护 pid=$(cat "$CHARGE_LIMIT_PID")，取消: $0 charge limit stop" ;;
      esac ;;
    restore)
      charge_limit_kill
      ps_find || return 1
      if [ -f "$CHARGE_SAVED" ]; then
        while IFS="=" read -r k v; do
          [ -n "$k" ] || continue
          case "$k:$v" in
            charge_control_limit:0|charge_control_limit:1) echo "  跳过 $k=$v (USB sink 语义，不还原)" ;;
            *) ps_set "$k" "$v" ;;
          esac
        done < "$CHARGE_SAVED"
        rm -f "$CHARGE_SAVED"
      else
        echo "  没保存过充电原始值"
      fi ;;
    *) echo "用法: $0 charge {show|cool [mW] [mA]|power <mW>|current <mA>|stop on|off|jeita on|off|limit <pct>|limit stop|restore}" ;;
  esac
}


# ============================================================
#  FAS（帧率感知，FEAS 风格）：机制在内核 /sys/kernel/tca_fas，
#  策略全在这里 —— on/off/status/live/set/preset/testperf
# ============================================================
FAS=/sys/kernel/tca_fas

fas_have() {
  [ -d "$FAS" ] || { echo "  内核无 $FAS（需要 TCA Cycle Kernel 的 FAS v2）"; return 1; }
}

do_fas_status() {
  fas_have || return 1
  echo "--- FAS 实时 ---"
  [ -e "$FAS/state" ] && echo "  $(cat $FAS/state)"
  echo "--- 旋钮 ---"
  for k in enable adaptive target_fps margin_us headroom_pct gain_khz_per_ms \
           smooth_shift min_pct min_khz max_khz; do
    [ -e "$FAS/$k" ] && echo "  $k = $(cat $FAS/$k)"
  done
}

do_fas() {
  case "${1:-status}" in
    on)
      fas_have || return 1
      w "$FAS/enable" 1 ;;
    off)
      fas_have || return 1
      w "$FAS/enable" 0 ;;
    status|show)
      do_fas_status ;;
    live)
      fas_have || return 1
      cat "$FAS/state" 2>/dev/null || echo "FAS 未就绪" ;;
    set)
      fas_have || return 1
      k="$2"; v="$3"
      case "$k" in
        adaptive|target_fps|margin_us|headroom_pct|gain_khz_per_ms|smooth_shift|min_pct|min_khz|max_khz)
          [ -n "$v" ] || { echo "用法: $0 fas set <节点> <值>"; return 1; }
          w "$FAS/$k" "$v" ;;
        *) echo "  不可写/未知节点: $k"
           echo "  可写: adaptive target_fps margin_us headroom_pct gain_khz_per_ms smooth_shift min_pct min_khz max_khz"
           return 1 ;;
      esac ;;
    preset)
      fas_have || return 1
      echo "== FAS 预设: ${2:-?} =="
      case "${2:-}" in
        smooth)  # 日常均衡
          w "$FAS/adaptive" 1; w "$FAS/margin_us" 1500
          w "$FAS/headroom_pct" 125; w "$FAS/gain_khz_per_ms" 40000
          w "$FAS/smooth_shift" 3; w "$FAS/min_pct" 30; w "$FAS/min_khz" 0 ;;
        game)    # 游戏：低 margin + 快升 + 高下限
          w "$FAS/adaptive" 1; w "$FAS/margin_us" 1000
          w "$FAS/headroom_pct" 140; w "$FAS/gain_khz_per_ms" 80000
          w "$FAS/smooth_shift" 2; w "$FAS/min_pct" 45; w "$FAS/min_khz" 0 ;;
        video)   # 视频/省电：宽容 + 慢升 + 低下限
          w "$FAS/adaptive" 1; w "$FAS/margin_us" 2500
          w "$FAS/headroom_pct" 115; w "$FAS/gain_khz_per_ms" 20000
          w "$FAS/smooth_shift" 4; w "$FAS/min_pct" 20; w "$FAS/min_khz" 0 ;;
        pin60)   # 绝对锁 60 档（关自适应）
          w "$FAS/adaptive" 0; w "$FAS/target_fps" 60
          w "$FAS/margin_us" 1200; w "$FAS/headroom_pct" 130
          w "$FAS/gain_khz_per_ms" 60000; w "$FAS/smooth_shift" 3
          w "$FAS/min_pct" 35 ;;
        *) echo "用法: $0 fas preset <smooth|game|video|pin60>"; return 1 ;;
      esac
      do_fas_status ;;
    testperf)
      # FEAS 判别法：锁 performance 调速器后跑典型应用（王者荣耀等）：
      #   有 FAS → 大核不钉死最高频，沿天花板带状波动；
      #   无 FAS → 与常规 performance 无异（钉死）。
      case "${2:-}" in
        on)
          echo "== 锁 performance 调速器（FEAS 验证模式）=="
          for p in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
            w "$p" performance
          done
          echo "  跑应用看大核频率曲线；还原: $0 fas testperf off" ;;
        off)
          echo "== 恢复 schedutil 调速器 =="
          for p in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
            w "$p" schedutil
          done ;;
        *) echo "用法: $0 fas testperf <on|off>"; return 1 ;;
      esac ;;
    *) echo "用法: $0 fas {on|off|status|live|set <节点> <值>|preset <smooth|game|video|pin60>|testperf <on|off>}"; return 1 ;;
  esac
}

# ============ 内存诊断（0.5.2 撤回未经验证的容器写入） ============
DS_FLAG=/data/adb/tca-ds-guard

mem_guard_off() {
  # 旧循环每轮检查此标志；不信任旧 PID 文件，不发送信号、不迁移进程。
  rm -f "$DS_FLAG" || { echo "错误: 无法移除旧守护标志" >&2; return 1; }
  echo "已移除守护标志；旧循环在当前扫描/休眠结束后退出，不保证立即停止。"
  echo "已有 cgroup 限值、组内进程及旧全局内存设置保持不变；本命令不解除限制。"
}

mem_status() {
  echo "--- 内存概览 ---"
  grep -E 'MemTotal|MemAvailable|MemFree|Cached|SwapTotal|SwapFree|Dirty|Writeback' /proc/meminfo
  echo "--- zram ---"
  local z stat
  for z in /sys/block/zram*; do
    [ -d "$z" ] || continue
    echo "  ${z##*/}:"
    if [ -r "$z/mm_stat" ]; then
      echo "    mm_stat（前3列: orig_data_size compr_data_size mem_used_total，字节；后续列依内核）:"
      sed 's/^/      /' "$z/mm_stat"
    else
      for stat in orig_data_size compr_data_size; do
        [ -r "$z/$stat" ] && echo "    $stat=$(cat "$z/$stat")B"
      done
    fi
    [ ! -r "$z/io_stat" ] || { echo "    io_stat（failed_reads failed_writes invalid_io notify_free）:"; sed 's/^/      /' "$z/io_stat"; }
  done
  echo "--- swap 设备 ---"
  cat /proc/swaps 2>/dev/null
  echo "--- 回收累计计数（需间隔采样差值，不能单次判断压力） ---"
  grep -E '^(allocstall|pgscan|pgsteal|pswpin|pswpout|oom_kill|compact)[^ ]* ' /proc/vmstat 2>/dev/null
  echo "--- PSI (memory) ---"
  sed 's/^/  /' /proc/pressure/memory 2>/dev/null
  echo "--- OOM/回收策略 ---"
  echo "  panic_on_oom=$(cat /proc/sys/vm/panic_on_oom 2>/dev/null) oom_kill_allocating_task=$(cat /proc/sys/vm/oom_kill_allocating_task 2>/dev/null)"
  echo "  swappiness=$(cat /proc/sys/vm/swappiness 2>/dev/null) watermark_scale_factor=$(cat /proc/sys/vm/watermark_scale_factor 2>/dev/null) min_free_kbytes=$(cat /proc/sys/vm/min_free_kbytes 2>/dev/null)"
  [ ! -r /proc/sys/vm/direct_swappiness ] || echo "  direct_swappiness=$(cat /proc/sys/vm/direct_swappiness)"
  echo "--- 旧 tca_ds 组（仅诊断，不验证容器归属） ---"
  local cg key found=0
  for cg in /sys/fs/cgroup/memory/tca_ds /dev/memcg/tca_ds /sys/fs/cgroup/tca_ds; do
    [ -d "$cg" ] || continue
    found=1
    echo "  $cg"
    for key in memory.limit_in_bytes memory.memsw.limit_in_bytes memory.usage_in_bytes memory.max memory.swap.max memory.current memory.oom.group memory.events; do
      [ -f "$cg/$key" ] && { echo "  $key:"; sed 's/^/    /' "$cg/$key" 2>/dev/null; }
    done
  done
  [ "$found" = 1 ] || echo "  常见路径未发现旧组；不代表设备没有其他层级或限制。"
  echo "  旧守护标志: $([ -f "$DS_FLAG" ] && echo 存在 || echo 不存在)（不是进程存活检测）"
  echo "  0.5.2 仅提供诊断和 mem guard off；已有设置不会自动还原。"
  return 0
}

do_mem() {
  case "${1:-status}" in
    status|show) mem_status ;;
    guard)
      if [ "${2:-}" = off ] && [ "$#" = 2 ]; then
        mem_guard_off
      else
        echo "错误: 内存守护已撤回；仅支持 mem guard off（保留已有组和限值）。" >&2
        return 1
      fi ;;
    cap|uncap|run|adopt|root|daemon|preset)
      echo "错误: mem $1 已在 0.5.2 禁用；设备 cgroup 布局/归属及安全限值未经验证，不写入、不启动命令。" >&2
      return 1 ;;
    *) echo "用法: $0 mem {status|show|guard off}" >&2; return 1 ;;
  esac
}

# ============ ART 属性诊断 / 旧 GC 属性回滚 ============
GC_PROP_KEYS="dalvik.vm.heapstartsize dalvik.vm.heaptargetutilization dalvik.vm.heapminfree dalvik.vm.heapmaxfree"

gc_propfile() {
  local d script
  # 便捷入口可能是符号链接，回滚必须定位实际模块目录。
  script=$(readlink -f "$0") || return 1
  d=$(cd "$(dirname "$script")" 2>/dev/null && pwd) || return 1
  echo "$d/system.prop"
}

gc_strip() (  # 同目录临时文件 + 原子替换；空结果也必须提交。
  pf=$1
  [ ! -L "$pf" ] || { echo "错误: 拒绝修改符号链接 $pf" >&2; exit 1; }
  [ -e "$pf" ] || exit 0
  [ -f "$pf" ] || { echo "错误: 非普通文件 $pf" >&2; exit 1; }
  tmp=$(mktemp "$pf.tmp.XXXXXX") || exit 1
  trap 'rm -f "$tmp"' EXIT
  trap 'exit 1' HUP INT TERM
  # 保留原文件权限；任一步失败均不替换原文件。
  cp -p "$pf" "$tmp" || exit 1
  awk -v keys="$GC_PROP_KEYS" '
    BEGIN { n = split(keys, list, " "); for (i = 1; i <= n; i++) managed[list[i]] = 1 }
    {
      pos = index($0, "=")
      key = substr($0, 1, pos - 1)
      sub(/^[ \t]+/, "", key); sub(/[ \t]+$/, "", key)
      if (!pos || !(key in managed)) print
    }
  ' "$pf" > "$tmp" || exit 1
  mv -f "$tmp" "$pf" || exit 1
)

gc_rollback() {
  local pf
  pf=$(gc_propfile) || return 1
  gc_strip "$pf" || { echo "错误: GC 回滚失败，未确认移除。" >&2; return 1; }
  echo "已移除本脚本旧版管理的四个 GC 属性（如存在）：$pf"
  echo "必须完整重启设备才能重新读取启动配置；未改运行属性、未重启 zygote。"
  echo "其他模块/系统仍可能设置这些属性，不保证恢复原厂值。"
}

gc_status() {
  local k pf
  echo "--- ART 属性服务读数（不代表现有 ART/zygote 实际堆配置） ---"
  for k in $GC_PROP_KEYS dalvik.vm.heapgrowthlimit dalvik.vm.heapsize dalvik.vm.gctype; do
    echo "  $k=$(getprop $k)"
  done
  echo "--- 模块 system.prop ---"
  pf=$(gc_propfile) || return 1
  if [ -f "$pf" ]; then sed 's/^/  /' "$pf"; else echo "  (未设置)"; fi
}

do_gc() {
  case "${1:-status}" in
    status|show) gc_status ;;
    rollback) [ "$#" = 1 ] || { echo "用法: $0 gc rollback" >&2; return 1; }; gc_rollback ;;
    preset)
      # 仅保留旧回滚命令的兼容别名。
      if [ "${2:-}" = stock ] && [ "$#" = 2 ]; then gc_rollback
      else echo "错误: GC 预设已在 0.5.2 撤回；仅支持 gc rollback（或 gc preset stock）。" >&2; return 1; fi ;;
    apply|soft-restart)
      echo "错误: gc $1 已禁用；不写运行属性、不重启 zygote。回滚后须完整重启设备。" >&2
      return 1 ;;
    *) echo "用法: $0 gc {status|show|rollback|preset stock}" >&2; return 1 ;;
  esac
}

case "${1:-show}" in  show)   do_show ;;
  smooth) do_smooth ;;
  game)   do_game ;;
  net)    save; do_net ;;
  io)     save; do_io ;;
  stock)  do_stock ;;
  sched)  do_sched "${2:-}" ;;
  touch)  do_touch "${2:-}" "${3:-}" ;;  # on|off|all|show|freq <kHz>|ms <n>|uclamp <0|1>|uclamp_pct <n>|affinity <0|1>|cpu <n>|idle <us>
  audio)  do_audio "${2:-}" "${3:-}" ;;  # on|off|idle on|idle off
  charge) shift; do_charge "$@" ;; # show|cool|power|current|stop|jeita|limit|restore
  fas)    do_fas "${2:-}" "${3:-}" "${4:-}" ;; # on|off|status|live|set|preset|testperf
  mem)    shift; do_mem "$@" ;;  # status|show|guard off
  gc)     shift; do_gc "$@" ;;  # status|show|rollback|preset stock
  *) echo "用法: $0 {show|smooth|game|net|io|stock|sched ...|touch ...|audio ...|charge ...|fas ...|mem ...|gc ...}"; exit 1 ;;
esac
rc=$?
[ "$rc" -eq 0 ] || exit "$rc"
echo "完成。show=看状态；stock 仅还原有保存记录的常规调优，不还原旧 mem/GC。"
