#!/usr/bin/env python3
# reverse-abi.py — 从原厂 .ko 的只读实例（如 *_desc / 初始化结构体）反推字段布局。
# 用法: reverse-abi.py <ko> <symbol> [size]
# 指针字段显示为 PTR -> 目标；标量显示为 imm=值。
import subprocess, re, sys, os
env = dict(os.environ, LC_ALL='C')
def run(*a): return subprocess.run(a, capture_output=True, text=True, env=env).stdout
f, sym = sys.argv[1], sys.argv[2]
size = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x100
secs = {}
for line in run('aarch64-linux-gnu-readelf','-SW',f).splitlines():
    m = re.match(r'\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', line)
    if m: secs[int(m.group(1))] = (m.group(2), int(m.group(4),16), int(m.group(5),16), int(m.group(6),16))
saddr = None
for line in run('aarch64-linux-gnu-readelf','-sW',f).splitlines():
    p = line.split()
    if len(p) >= 8 and p[-1] == sym: saddr = int(p[1],16); ssec = int(p[6]); break
if saddr is None: print('symbol not found:', sym); sys.exit(1)
secname, secaddr, secoff, _ = secs[ssec]
rels = {}; cur = None
for line in run('aarch64-linux-gnu-readelf','-rW',f).splitlines():
    m = re.match(r"Relocation section '(\S+)'", line)
    if m: cur = m.group(1); continue
    m = re.match(r'^([0-9a-f]{8,})\s+([0-9a-f]+)\s+(\S+)\s+([0-9a-f]+)\s+(\S+)\s*\+?\s*(.*)$', line)
    if m and cur and (cur == '.rela'+secname or cur.startswith('.rela'+secname+'.')):
        rels[int(m.group(1),16)] = m.group(5) + ('+'+m.group(6).strip() if m.group(6).strip() else '')
data = open(f,'rb').read(); base = secoff + (saddr - secaddr)
print('%s  %s  (section %s)' % (f, sym, secname))
for i in range(0, size, 8):
    v = int.from_bytes(data[base+i:base+i+8],'little'); r = rels.get(saddr+i)
    print('  +0x%03x  %s' % (i, ('PTR  %-24s' % r) if r else ('imm=%d' % v if v else '0')))