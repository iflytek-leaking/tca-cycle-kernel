#!/usr/bin/env bash
# 从 spd_dump 的 super 备份里解出「HAL 层」相关分区。
#
#   system_a / vendor_a / odm_a / system_ext_a / product_a / vendor_dlkm_a
#       -> lpunpack 拆逻辑分区 -> fsck.erofs 展开成目录
#
# 产物默认放在 <repo>/out/lp/<part>_rd/，对应关系：
#   system_a.img      -> system_rd/       (LOS 要替换的 framework 侧)
#   vendor_a.img      -> vendor_rd/       (HAL server + impl lib + 配置，必须保留)
#   odm_a.img         -> odm_rd/          (板级配置 + camera/fingerprint impl lib，必须保留)
#   system_ext_a.img  -> system_ext_rd/   (展锐 framework 扩展 + 讯飞定制 app)
#   product_a.img     -> product_rd/      (通用 AOSP 应用)
#   vendor_dlkm_a.img -> vendor_dlkm_rd/  (134 个 vendor 内核模块)
#
# 用法:
#   tools/extract-stock-hal.sh [DUMP_DIR] [OUT_DIR]
#   HAL_PARTS="vendor_dlkm_a vendor_a" tools/extract-stock-hal.sh   # 只解部分
#
# 注意:
#   * 260606_190243/super 是 4.35 GB 截断镜像，不要用；请用 260606_193212/super（10 GB 完整）。
#   * 这份 super 带 system_a-cow（虚拟 A/B OTA 快照），lpunpack 取到的是合并前 base；
#     vendor/odm/system_ext/product/vendor_dlkm 不受影响，system 反正会被 LOS 整个替换。
#   * 全量解包约需 20 GB（镜像 + 展开后），system 展开约 8 GB。
set -euo pipefail

DUMP_DIR="${1:-/home/misakapardola/下载/spd_dump_it_main_238_c3d2c5ceeb92a511e00d197c7f7a94a6108a9b43_x86_64-unknown-linux-musl/260606_193212}"
OUT_DIR="${2:-$(cd "$(dirname "$0")/.." && pwd)/out/lp}"
SUPER="$DUMP_DIR/super"
PARTS="${HAL_PARTS:-system_a vendor_a odm_a system_ext_a product_a vendor_dlkm_a}"

die() { echo "[error] $*" >&2; exit 1; }

command -v lpunpack   >/dev/null || die "缺少 lpunpack（android-sdk-libsparse-utils 或 android-tools）"
command -v fsck.erofs >/dev/null || die "缺少 fsck.erofs（erofs-utils）"
command -v file       >/dev/null || die "缺少 file"
[ -f "$SUPER" ] || die "找不到 super 镜像: $SUPER"

# 磁盘余量粗查（镜像 + 展开 ≈ 20 GB）
free_gb=$(df -Pk "$(dirname "$OUT_DIR")" 2>/dev/null | awk 'NR==2{printf "%d", $4/1048576}')
if [ -n "${free_gb:-}" ] && [ "$free_gb" -lt 25 ]; then
    echo "[warn] 目标盘可用 ${free_gb} GB，全量解包约需 20 GB，可能不够"
fi

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"

echo "== 1/2 lpunpack（$SUPER）"
for p in $PARTS; do
    if [ -f "${p}.img" ]; then
        echo "  [skip] ${p}.img 已存在"
        continue
    fi
    echo "  [unpack] $p"
    lpunpack -p "$p" "$SUPER" . >/dev/null || die "lpunpack $p 失败（super 是否截断？）"
done

echo "== 2/2 展开文件系统"
for p in $PARTS; do
    img="${p}.img"
    [ -f "$img" ] || { echo "  [miss] $img"; continue; }
    base="${p%_a}"; [ "$base" = "$p" ] || true
    name="${base}_rd"
    if [ -d "$name" ]; then
        echo "  [skip] $name/ 已存在"
        continue
    fi
    typ=$(file -b "$img")
    case "$typ" in
        *EROFS*)
            mkdir -p "$name"
            fsck.erofs --extract="$name" "$img" >/dev/null
            echo "  [erofs] $img -> $name/"
            ;;
        *ext4*|*ext3*|*ext2*|*"Linux rev"*)
            mkdir -p "$name"
            if ! debugfs -R "rdump / $name" "$img" >/dev/null 2>&1; then
                echo "  [warn] $img 是 ext 镜像但 debugfs 展开失败（需要 e2fsprogs）"
            else
                echo "  [ext]  $img -> $name/"
            fi
            ;;
        *)
            echo "  [warn] 未知文件系统，跳过: $img ($typ)"
            ;;
    esac
done

echo
echo "== 解包汇总（$OUT_DIR）"
printf "  %-18s %8s %8s  %s\n" "目录" "大小" "文件数" "说明"
note() { case "$1" in
    system_rd)       echo "LOS 替换掉（framework 侧）";;
    vendor_rd)       echo "HAL server / impl lib / 配置，必须保留";;
    odm_rd)          echo "板级配置 + camera/fingerprint impl，必须保留";;
    system_ext_rd)   echo "展锐 framework 扩展 + 讯飞定制 app";;
    product_rd)      echo "通用 AOSP 应用";;
    vendor_dlkm_rd)  echo "vendor 内核模块（.ko）";;
    *)               echo "";; esac; }
for p in $PARTS; do
    base="${p%_a}"; name="${base}_rd"
    [ -d "$name" ] || continue
    printf "  %-18s %8s %8s  %s\n" "$name/" "$(du -sh "$name" | cut -f1)" "$(find "$name" -type f | wc -l)" "$(note "$name")"
done

echo
echo "HAL 层要点："
echo "  vendor/bin/hw     : $(ls vendor_rd/bin/hw 2>/dev/null | wc -l) 个二进制（HAL server 为主）"
echo "  vendor/lib64/hw   : $(ls vendor_rd/lib64/hw 2>/dev/null | wc -l) 个 HAL impl lib"
echo "  odm/lib64/hw      : $(ls odm_rd/lib64/hw 2>/dev/null | wc -l) 个（camera / fingerprint）"
echo "  vendor/etc/vintf  : $(ls vendor_rd/etc/vintf/manifest/*.xml 2>/dev/null | wc -l) 个 manifest 片段 + manifest.xml"
echo "  vendor/etc/init   : $(ls vendor_rd/etc/init/*.rc 2>/dev/null | wc -l) 个 rc"
echo "  vendor_dlkm ko    : $(ls vendor_dlkm_rd/lib/modules/*.ko 2>/dev/null | wc -l) 个"
echo
echo "清单文档：reports/HAL-inventory-ums9620_se50.md"
