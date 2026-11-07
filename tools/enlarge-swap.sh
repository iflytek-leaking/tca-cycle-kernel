#!/bin/bash
# enlarge-swap.sh — 扩容 /swapfile（ext4，fallocate 可用）
# 用法: sudo ./tools/enlarge-swap.sh [大小GB]   (默认 48)
set -e
SIZE="${1:-48}"
NEW="/swapfile"
echo "[i] 当前:"; swapon --show; free -h | head -3
echo "[i] 关闭 swap $NEW ..."
swapoff "$NEW"
echo "[i] 重建为 ${SIZE}G ..."
fallocate -l "${SIZE}G" "$NEW" || dd if=/dev/zero of="$NEW" bs=1M count=$((SIZE*1024)) status=progress
chmod 600 "$NEW"
mkswap "$NEW"
swapon "$NEW"
echo "[i] 完成:"; swapon --show; free -h | head -3
