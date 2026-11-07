#!/usr/bin/env python3
# stock-crc.py — 把 genksyms 输出的 __crc_<sym> 值替换成原厂内核的 CRC。
# 用法: ... | stock-crc.py <kernel-tree>/scripts/stock-crc.map
import sys
m = {}
if len(sys.argv) > 1:
    for line in open(sys.argv[1]):
        p = line.split()
        if len(p) == 2:
            m[p[0]] = p[1].lower()
import re
pat = re.compile(r'^(__crc_([A-Za-z0-9_]+)\s*=\s*)0x[0-9a-fA-F]+(;.*)$')
for line in sys.stdin:
    mt = pat.match(line)
    if mt and mt.group(2) in m:
        sys.stdout.write('%s%s%s\n' % (mt.group(1), m[mt.group(2)], mt.group(3)))
    else:
        sys.stdout.write(line)