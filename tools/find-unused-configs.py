#!/usr/bin/env python3
# find-unused-configs.py — 找「启用了、但 of_device_id compatible 不在设备 DT 里」的驱动配置。
# 用法: find-unused-configs.py <kernel_src> <O_dir/config> <dt_dts...>
# 注意: 只是候选；框架类(CONFIG_ARM64/PM/CRYPTO/...)和 OF_DECLARE 驱动会误报，需人工复核。
import os, re, sys
ROOT = sys.argv[1]
CFG  = sys.argv[2]
DTF  = sys.argv[3:]
DT = set()
for f in DTF:
    for line in open(f, errors='ignore'):
        if 'compatible' in line and '=' in line:
            for m in re.findall(r'"([^"]+)"', line): DT.add(m)
enabled = set()
for line in open(CFG):
    m = re.match(r'(CONFIG_[A-Za-z0-9_]+)=y$', line.strip())
    if m: enabled.add(m.group(1))
cfg2obj = {}
for dp, dirs, ns in os.walk(ROOT):
    if '/.git' in dp: continue
    for n in ns:
        if n not in ('Makefile','Kbuild'): continue
        p = os.path.join(dp, n); d = os.path.dirname(p)
        try: t = open(p, errors='ignore').read()
        except: continue
        for line in t.splitlines():
            m = re.search(r'\bobj-\$\((CONFIG_[A-Za-z0-9_]+)\)\s*[:+]?=\s*(.+)$', line)
            if m:
                for tok in m.group(2).split(): cfg2obj.setdefault(m.group(1), []).append(os.path.join(d, tok.strip()))
cre = re.compile(r'compatible\s*=\s*"([^"]+)"')
def cs(p):
    if not p.endswith('.c') or not os.path.isfile(p): return []
    try: t = open(p, errors='ignore').read()
    except: return []
    return cre.findall(t) if 'compatible' in t else []
for cfg in sorted(enabled):
    objs = cfg2obj.get(cfg)
    if not objs: continue
    allc = []; srcs = []
    for o in objs:
        if o.endswith('/'):
            for dp,_,ns in os.walk(o):
                for n in ns:
                    if n.endswith('.c'):
                        c = cs(os.path.join(dp,n))
                        if c: allc += c; srcs.append(os.path.join(dp,n))
        else:
            f = o[:-2]+'.c' if o.endswith('.o') else o
            c = cs(f)
            if c: allc += c; srcs.append(f)
    allc = sorted(set(allc))
    if not allc or len(srcs) > 3: continue
    un = [c for c in allc if c not in DT]
    if un and len(un) == len(allc):
        print('%-46s %s' % (cfg, ', '.join(os.path.relpath(s, ROOT) for s in srcs)))
        print('      compat: %s' % ', '.join(allc[:5]))