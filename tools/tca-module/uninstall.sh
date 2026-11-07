#!/system/bin/sh
# 卸载清理：停守护、删便捷入口与保存的原始值
# 旧内存守护仅通过移除标志退出；现有组/限值不解除，不按 PID 杀进程。
rm -f /data/adb/tca-ds-guard
[ -f /data/adb/tca-charge-limit.pid ] && kill "$(cat /data/adb/tca-charge-limit.pid)" 2>/dev/null
[ -f /data/adb/tca-audio-hold.pid ] && kill "$(cat /data/adb/tca-audio-hold.pid)" 2>/dev/null
rm -f /data/adb/tca-tune.sh
rm -f /data/adb/tca-tune.saved
rm -f /data/adb/tca-charge.saved
rm -f /data/adb/tca-charge-limit.pid
rm -f /data/adb/tca-audio-hold.pid
rm -f /data/adb/tca-webui.conf
