#!/bin/sh
# 打包成可刷入的 Magisk/KernelSU 模块 zip
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
OUT="$ROOT/artifacts"
mkdir -p "$OUT"

cp "$ROOT/tools/tca-tune.sh" "$DIR/tca-tune.sh"
chmod 0755 "$DIR/tca-tune.sh"

VER=$(sed -n 's/^version=//p' "$DIR/module.prop" | head -1)
ZIP="$OUT/tca-cycle-tuner-$VER.zip"
rm -f "$ZIP"
( cd "$DIR" && zip -r9 "$ZIP" module.prop customize.sh service.sh uninstall.sh tca-tune.sh webroot >/dev/null )
echo "$ZIP"
