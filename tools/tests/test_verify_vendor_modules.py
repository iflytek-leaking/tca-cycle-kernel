#!/usr/bin/env python3
"""Host-only preflight tests; no device or kernel writes."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "verify", Path(__file__).resolve().parents[1] / "verify-vendor-modules.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.modules = self.root / "modules"
        self.modules.mkdir()
        self.image = self.root / "Image"
        self.image.write_bytes(b"\0kernel_symbol\0")
        (self.modules / "a.ko").write_bytes(b"fixture")
        (self.modules / "modules.load").write_text("# comment\na.ko\n")

    def symbols(self, path, undefined=False):
        if Path(path).name == "vmlinux":
            return ["__ksymtab_kernel_symbol"]
        return ["kernel_symbol"] if undefined else ["__ksymtab_vendor_symbol"]

    def run_check(self, elf=False):
        with patch.object(verify, "nm_symbols", side_effect=self.symbols), contextlib.redirect_stdout(io.StringIO()):
            return verify.preflight(self.modules, self.image,
                                    self.root / "vmlinux" if elf else None)

    def test_nonexistent_directory(self):
        with self.assertRaises(verify.PreflightError):
            verify.preflight(self.root / "missing", self.image)

    def test_zero_modules_rejected(self):
        (self.modules / "a.ko").unlink()
        with self.assertRaises(verify.PreflightError):
            self.run_check()

    def test_missing_load_rejected(self):
        (self.modules / "modules.load").unlink()
        with self.assertRaises(verify.PreflightError):
            self.run_check()

    def test_missing_load_entry_rejected(self):
        (self.modules / "modules.load").write_text("missing.ko\n")
        with self.assertRaises(verify.PreflightError):
            self.run_check()

    def test_empty_image_rejected(self):
        self.image.write_bytes(b"")
        with self.assertRaises(verify.PreflightError):
            self.run_check()

    def test_empty_imports_rejected(self):
        self.symbols = lambda path, undefined=False: [] if undefined else ["__ksymtab_vendor"]
        with self.assertRaises(verify.PreflightError):
            self.run_check()

    def test_nm_failure_rejected(self):
        with patch.object(verify.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "nm")):
            with self.assertRaises(verify.PreflightError):
                verify.nm_symbols(self.modules / "a.ko")

    def test_exact_string_passes(self):
        self.assertEqual(self.run_check(), 0)

    def test_substring_is_not_symbol(self):
        self.image.write_bytes(b"\0prefix_kernel_symbol_suffix\0")
        self.assertEqual(self.run_check(), 1)

    def test_exports_mode_passes(self):
        self.assertEqual(self.run_check(elf=True), 0)

    def test_regular_elf_symbol_is_not_export(self):
        original = self.symbols
        self.symbols = lambda path, undefined=False: ["kernel_symbol"] if Path(path).name == "vmlinux" else original(path, undefined)
        with self.assertRaises(verify.PreflightError):
            self.run_check(elf=True)


if __name__ == "__main__":
    unittest.main()
