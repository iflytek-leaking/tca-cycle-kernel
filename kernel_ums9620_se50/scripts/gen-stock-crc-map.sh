#!/bin/sh
# gen-stock-crc-map.sh — 从原厂模块的 __versions 提取 symbol->CRC，写入 scripts/stock-crc.map
# 用法: ./scripts/gen-stock-crc-map.sh <stock_modules_dir> [out.map]
set -e
DIR="${1:?usage: $0 <stock_modules_dir> [out.map]}"
OUT="${2:-$(dirname "$0")/stock-crc.map}"
tmp="$(mktemp)"
for f in "$DIR"/*.ko; do
  modprobe --dump-modversions "$f" 2>/dev/null | awk -F'\t' 'NF>=2{print $2" "$1}'
done | sort -u > "$tmp"
mv "$tmp" "$OUT"
echo "wrote $OUT ($(wc -l < "$OUT") entries)"