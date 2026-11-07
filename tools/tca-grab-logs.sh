#!/system/bin/sh
# ============================================================
#  TCA Cycle Kernel - 崩溃/日志一键抓取
#  用法: su -c "sh /data/adb/tca-grab-logs.sh"
#  产出: /sdcard/tca-logs-<时间戳>.tar.gz
#  抓: pstore(内核 panic 现场) + tombstones + dmesg + logcat + KSU 日志 + 调优状态
# ============================================================
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=/data/local/tmp/tca-logs-$STAMP
PB=/data/adb/ksu/bin/busybox
[ -x "$PB" ] || PB=busybox
BB="$PB"
mkdir -p "$OUT"

echo "[1/7] pstore (内核 panic 现场)..."
if [ -d /sys/fs/pstore ]; then
  ls -la /sys/fs/pstore/ > "$OUT/pstore-list.txt" 2>&1
  for f in /sys/fs/pstore/*; do
    [ -f "$f" ] || continue
    $BB cat "$f" > "$OUT/pstore-$(basename $f).txt" 2>/dev/null
  done
else
  echo "/sys/fs/pstore 不存在" > "$OUT/pstore-list.txt"
fi
[ -f /proc/last_kmsg ] && $BB cat /proc/last_kmsg > "$OUT/last_kmsg.txt" 2>/dev/null

echo "[2/7] tombstones..."
if [ -d /data/tombstones ]; then
  mkdir -p "$OUT/tombstones"
  ls -lat /data/tombstones/ | head -40 > "$OUT/tombstones/list.txt"
  for f in $(ls -t /data/tombstones/tombstone_[0-9]* 2>/dev/null | head -6); do
    case "$f" in *.pb) continue;; esac
    $BB cp "$f" "$OUT/tombstones/" 2>/dev/null
  done
fi
[ -d /data/system/dropbox ] && ls -lat /data/system/dropbox | head -30 > "$OUT/dropbox-list.txt" 2>/dev/null

echo "[3/7] dmesg..."
$BB dmesg > "$OUT/dmesg.txt" 2>/dev/null || dmesg > "$OUT/dmesg.txt" 2>/dev/null
$BB dmesg | $BB grep -iE "panic|BUG:|Oops|call trace|Unable to handle|Kernel panic|watchdog|WARN" > "$OUT/dmesg-crash-only.txt" 2>/dev/null

echo "[4/7] logcat..."
logcat -d -b all -v threadtime > "$OUT/logcat.txt" 2>/dev/null
logcat -d -b crash -v threadtime > "$OUT/logcat-crash.txt" 2>/dev/null

echo "[5/7] KernelSU 日志..."
mkdir -p "$OUT/ksu"
$BB cp -r /data/adb/ksu/log/* "$OUT/ksu/" 2>/dev/null
for f in /data/adb/ksu/log/*.log /data/adb/ksud.log; do [ -f "$f" ] && $BB cp "$f" "$OUT/ksu/" 2>/dev/null; done

echo "[6/7] 系统信息..."
{
  echo "== uname =="; uname -a
  echo; echo "== /proc/version =="; cat /proc/version
  echo; echo "== cmdline =="; cat /proc/cmdline
  echo; echo "== CPU =="; for p in /sys/devices/system/cpu/cpufreq/policy*; do echo "$p gov=$(cat $p/scaling_governor 2>/dev/null) cur=$(cat $p/scaling_cur_freq 2>/dev/null)"; done
  echo; echo "== 调度器 =="; for b in /sys/block/*/queue/scheduler; do echo "$b = $(cat $b)"; done
  echo; echo "== sched =="; for k in sched_wakeup_granularity_ns sched_min_granularity_ns sched_migration_cost_ns sched_latency_ns sched_schedstats sched_energy_aware; do echo "$k = $(cat /proc/sys/kernel/$k 2>/dev/null)"; done
  echo; echo "== vm =="; for k in swappiness vfs_cache_pressure dirty_ratio watermark_scale_factor; do echo "$k = $(cat /proc/sys/vm/$k 2>/dev/null)"; done
  echo; echo "== net =="; echo "cc=$(cat /proc/sys/net/ipv4/tcp_congestion_control) qdisc=$(cat /proc/sys/net/core/default_qdisc)"
  echo "available cc: $(cat /proc/sys/net/ipv4/tcp_available_congestion_control)"
  echo; echo "== lsmod =="; lsmod
  echo; echo "== 已加载模块数 =="; lsmod | wc -l
  echo; echo "== pstore 剩余 =="; ls -la /sys/fs/pstore/ 2>/dev/null
  echo; echo "== 最近重启原因 =="; getprop ro.boot.bootreason; getprop sys.boot.reason
} > "$OUT/sysinfo.txt" 2>&1

echo "[7/7] 打包..."
cd /data/local/tmp
TARBALL=/sdcard/tca-logs-$STAMP.tar.gz
$BB tar czf "$TARBALL" "tca-logs-$STAMP" 2>/dev/null || tar czf "$TARBALL" "tca-logs-$STAMP"
$BB rm -rf "$OUT" 2>/dev/null
ls -la "$TARBALL"
echo
echo "完成: $TARBALL"
echo "（如果没有 panic，pstore 目录会是空的——那就说明这次不是内核 panic）"
