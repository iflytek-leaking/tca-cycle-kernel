#!/system/bin/sh
# Magisk / KernelSU 模块安装脚本
SKIPUNZIP=0

ui_print "- TCA Cycle Tuner 0.5.2 安全版本"
# 旧循环通过标志退出；绝不按旧 PID 文件发送信号。
rm -f /data/adb/tca-ds-guard || abort "无法移除旧内存守护标志"
ui_print "- 已撤回内存写入/GC 预设；已有 cgroup 限制不会自动解除"
ui_print "- 升级包不含旧 GC system.prop；须完整重启，不修改运行属性"

for f in tca-tune.sh customize.sh service.sh uninstall.sh; do
  [ -f "$MODPATH/$f" ] && set_perm "$MODPATH/$f" 0 0 0755
done
[ -d "$MODPATH/webroot" ] && set_perm_recursive "$MODPATH/webroot" 0 0 0755 0644

# 便捷入口：/data/adb/tca-tune.sh -> 模块内脚本
rm -f /data/adb/tca-tune.sh
ln -s "$MODPATH/tca-tune.sh" /data/adb/tca-tune.sh 2>/dev/null

ui_print "- 安装完成：重启后在 KernelSU/BakaSU 模块页点开 WebUI"
