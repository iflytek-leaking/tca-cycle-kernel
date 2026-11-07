#!/usr/bin/env python3
"""Static vendor-module preflight, NOT a runtime ABI/load guarantee.

Usage: verify-vendor-modules.py MODULE_DIR Image [--vmlinux matching/vmlinux]
Without --vmlinux only exact NUL-terminated symbol strings are checked.
With it, require actual __ksymtab_* exports from the matching ELF instead.
CRC/type/CFI compatibility and load order still require device validation.
"""
import argparse
from pathlib import Path
import subprocess
import sys


class PreflightError(Exception):
    pass


def nm_symbols(path, undefined=False):
    cmd = ["aarch64-linux-gnu-nm"]
    if undefined:
        cmd.append("-u")
    cmd.append(str(path))
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise PreflightError(f"nm failed for {path}: {exc}") from exc
    symbols = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2:
            symbols.append(fields[-1])
    if not undefined and not symbols:
        raise PreflightError(f"empty symbol table: {path}")
    return symbols


def exported(symbols):
    prefix = "__ksymtab_"
    return {name[len(prefix):] for name in symbols if name.startswith(prefix)}


def preflight(module_dir, image, vmlinux=None):
    root = Path(module_dir)
    if not root.is_dir():
        raise PreflightError(f"module directory does not exist: {root}")
    kos = sorted(root.glob("*.ko"))
    if not kos:
        raise PreflightError(f"no .ko modules: {root}")
    load_path = root / "modules.load"
    if not load_path.is_file():
        raise PreflightError(f"missing modules.load: {load_path}")
    load = [line.split("#", 1)[0].strip()
            for line in load_path.read_text().splitlines()]
    load = [name for name in load if name]
    if not load:
        raise PreflightError(f"empty modules.load: {load_path}")
    available = {ko.name for ko in kos}
    for name in load:
        if name not in available:
            raise PreflightError(f"modules.load entry missing or not a basename: {name}")
    data = Path(image).read_bytes()
    if not data:
        raise PreflightError(f"empty Image: {image}")
    providers = set()
    needed = {}
    for ko in kos:
        providers.update(exported(nm_symbols(ko)))
        for name in nm_symbols(ko, undefined=True):
            # CRC compatibility is not checked here; retain the old exclusion.
            if name.startswith(("__crc_", "_GLOBAL_OFFSET")):
                continue
            needed.setdefault(name, []).append(ko.name)
    if not needed:
        raise PreflightError("no undefined imports found; refusing an empty preflight")
    if vmlinux:
        kernel_exports = exported(nm_symbols(Path(vmlinux)))
        if not kernel_exports:
            raise PreflightError("vmlinux has no __ksymtab exports")
        provided_by_kernel = kernel_exports.__contains__
        mode = "ELF export table (caller must supply matching vmlinux)"
    else:
        provided_by_kernel = lambda name: (b"\0" + name.encode() + b"\0") in data
        mode = "Image symbol-string heuristic only"
    missing = {name: users for name, users in needed.items()
               if name not in providers and not provided_by_kernel(name)}
    print(f"vendor modules: {len(kos)}; modules.load: {len(load)}")
    print(f"mode: {mode}")
    print(f"imports: {len(needed)}; vendor-provided: {len(set(needed) & providers)}")
    for name, users in sorted(missing.items()):
        print(f"  MISSING {name} <- {', '.join(sorted(set(users)))}")
    print(f"unresolved imports: {len(missing)}")
    print("STATIC PREFLIGHT:", "FAIL" if missing else "PASS")
    print("Runtime module loading, CRC/ABI/CFI and hardware behavior are NOT verified.")
    return 1 if missing else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module_dir")
    parser.add_argument("image")
    parser.add_argument("--vmlinux")
    args = parser.parse_args(argv)
    try:
        return preflight(args.module_dir, args.image, args.vmlinux)
    except (PreflightError, OSError, UnicodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
