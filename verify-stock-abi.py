#!/usr/bin/env python3
# verify-stock-abi.py — 校验我们编译的模块/内核导出的 CRC 是否与原厂模块期望一致。
# 用法: python3 verify-stock-abi.py <ours_dir> [<stock_dir>]
import glob, subprocess, os, sys
from collections import defaultdict
def vers(ko):
    d = {}
    for l in subprocess.run(['modprobe','--dump-modversions',ko],capture_output=True,text=True).stdout.splitlines():
        p = l.split('\t')
        if len(p) >= 2: d[p[1]] = p[0]
    return d
ours_dir  = sys.argv[1] if len(sys.argv) > 1 else 's5-ship/modules'
stock_dir = sys.argv[2] if len(sys.argv) > 2 else 'stock/modules'
ours  = defaultdict(set); stock = defaultdict(set)
for ko in glob.glob(ours_dir + '/*.ko'):
    for s,c in vers(ko).items(): ours[s].add(c)
for ko in glob.glob(stock_dir + '/*.ko'):
    for s,c in vers(ko).items(): stock[s].add(c)
common = set(ours) & set(stock)
diff = sorted(s for s in common if ours[s] != stock[s])
print('ours modules : %d, stock modules: %d' % (len(glob.glob(ours_dir+'/*.ko')), len(glob.glob(stock_dir+'/*.ko'))))
print('common symbols: %d, CRC mismatch: %d' % (len(common), len(diff)))
for s in diff[:30]:
    print('  %-45s ours=%s stock=%s' % (s, sorted(ours[s]), sorted(stock[s])))
sys.exit(1 if diff else 0)