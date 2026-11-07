#!/system/bin/sh
# 开机只读取空白分隔的命令参数，不执行 shell 语法/引号/通配符。
MODDIR=${0%/*}
CONF=/data/adb/tca-webui.conf

# 无论是否有配置，停止旧收容循环；不读取 PID、不杀进程、不解除限值。
rm -f /data/adb/tca-ds-guard || { echo "TCA: 无法移除旧内存守护标志" >&2; exit 1; }
[ -f "$MODDIR/tca-tune.sh" ] || exit 0
# 安装目录可能从 modules_update 移到 modules，修复便捷入口。
ln -sf "$MODDIR/tca-tune.sh" /data/adb/tca-tune.sh || echo "TCA: 无法更新便捷入口" >&2
[ -f "$CONF" ] || exit 0

sleep 5
set -f
IFS=$(printf ' \t\n_')
IFS=${IFS%_}
rc=0
while IFS= read -r line || [ -n "$line" ]; do
  # 此处仅拆分旧格式的空白参数；禁止 eval，也不展开文件名。
  set -- $line
  [ "$#" -gt 0 ] || continue
  case "$1" in
    '#'*) continue ;;
    mem)
      case "$*" in
        'mem'|'mem status'|'mem show'|'mem guard off') ;;
        *) echo "TCA: 拒绝旧内存开机写入命令: $line" >&2; rc=1; continue ;;
      esac ;;
    gc)
      case "$*" in
        'gc'|'gc status'|'gc show') ;;
        *) echo "TCA: 拒绝 GC 开机写入/重启命令: $line；回滚请手动执行 gc rollback 后完整重启。" >&2; rc=1; continue ;;
      esac ;;
  esac
  sh "$MODDIR/tca-tune.sh" "$@" || rc=1
done < "$CONF"
exit "$rc"
