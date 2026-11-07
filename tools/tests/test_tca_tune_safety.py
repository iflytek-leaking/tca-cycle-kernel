#!/usr/bin/env python3
"""Isolated host regressions: temp copies and mocks, no real tuning writes."""
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
MASTER = (TOOLS / 'tca-tune.sh').read_text()
MODULE = TOOLS / 'tca-module'


class SafetyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="tca safety '")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.script = self.root / 'tca-tune.sh'
        self.script.write_text(MASTER)
        self.prop = self.root / 'system.prop'
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ['PATH'])

    def cli(self, *args, script=None):
        return subprocess.run(['sh', str(script or self.script), *args],
                              text=True, capture_output=True, env=self.env, timeout=10)

    def mock(self, name, body):
        p = self.bin / name
        p.write_text('#!/bin/sh\n' + body + '\n')
        p.chmod(0o755)

    def test_gc_final_keys_and_idempotence(self):
        for key in ('heapstartsize', 'heaptargetutilization', 'heapminfree', 'heapmaxfree'):
            for newline in ('', '\n'):
                self.prop.write_text('dalvik.vm.' + key + '=value' + newline)
                result = self.cli('gc', 'rollback')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('完整重启', result.stdout)
                self.assertEqual(self.prop.read_bytes(), b'')
                self.assertEqual(self.cli('gc', 'preset', 'stock').returncode, 0)
                self.assertEqual(self.prop.read_bytes(), b'')

    def test_exact_keys_and_permissions(self):
        keep = '# dalvik.vm.heapstartsize=comment\nother=value=2\ndalvikXvmXheapstartsize=keep\ndalvik.vm.heapstartsize.extra=keep\n\n'
        self.prop.write_text(keep + ' dalvik.vm.heapstartsize =16m\ndalvik.vm.heapstartsize=duplicate\ndalvik.vm.heapmaxfree=32m\n')
        self.prop.chmod(0o640)
        for _ in range(2):
            self.assertEqual(self.cli('gc', 'rollback').returncode, 0)
            self.assertEqual(self.prop.read_text(), keep)
        self.assertEqual(self.prop.stat().st_mode & 0o777, 0o640)
        self.assertEqual(list(self.root.glob('system.prop.tmp.*')), [])

    def test_gc_failures_leave_original(self):
        for command in ('awk', 'mv', 'cp', 'mktemp'):
            original = 'dalvik.vm.heapmaxfree=32m\nother=keep\n'
            self.prop.write_text(original)
            self.mock(command, 'exit 7')
            result = self.cli('gc', 'rollback')
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('完成。', result.stdout)
            self.assertEqual(self.prop.read_text(), original)
            self.assertEqual(list(self.root.glob('system.prop.tmp.*')), [])
            (self.bin / command).unlink()

    def test_missing_and_symlink_files(self):
        self.assertEqual(self.cli('gc', 'rollback').returncode, 0)
        self.assertFalse(self.prop.exists())
        target = self.root / 'other.prop'
        target.write_text('dalvik.vm.heapmaxfree=32m\n')
        self.prop.symlink_to(target)
        self.assertNotEqual(self.cli('gc', 'rollback').returncode, 0)
        self.assertEqual(target.read_text(), 'dalvik.vm.heapmaxfree=32m\n')
        self.prop.unlink()
        self.prop.write_text(target.read_text())
        entrydir = self.root / 'entry'
        entrydir.mkdir()
        entry = entrydir / 'tca-tune.sh'
        entry.symlink_to(self.script)
        self.assertEqual(self.cli('gc', 'rollback', script=entry).returncode, 0)
        self.assertEqual(self.prop.read_text(), '')
        self.assertFalse((entrydir / 'system.prop').exists())

    def test_withdrawn_commands(self):
        cases = [('mem', cmd) for cmd in ('cap', 'uncap', 'run', 'adopt', 'root', 'daemon', 'preset')]
        cases += [('mem', 'guard', 'on'), ('gc', 'preset', 'smooth'), ('gc', 'apply'), ('gc', 'soft-restart')]
        sentinel = self.root / 'should-not-exist'
        self.prop.write_text('other=keep\n')
        for args in cases + [('mem', 'run', 'touch', str(sentinel))]:
            result = self.cli(*args)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('错误', result.stderr)
            self.assertNotIn('完成。', result.stdout)
        self.assertFalse(sentinel.exists())
        self.assertEqual(self.prop.read_text(), 'other=keep\n')

    def test_flag_only_stop(self):
        flag = self.root / 'guard'
        flag.touch()
        pid = self.root / 'guard.pid'
        pid.write_text(str(os.getpid()))
        self.script.write_text(MASTER.replace('DS_FLAG=/data/adb/tca-ds-guard', 'DS_FLAG="' + str(flag) + '"'))
        result = self.cli('mem', 'guard', 'off')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(flag.exists())
        self.assertTrue(pid.exists())
        self.assertIn('限值', result.stdout)
        self.assertEqual(self.cli('mem', 'guard', 'off').returncode, 0)

    def test_readonly_status(self):
        self.mock('getprop', 'printf "mock-property"')
        for command in ('mem', 'gc'):
            for subcommand in ('status', 'show'):
                result = self.cli(command, subcommand)
                self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.prop.exists())

    def test_argv_dispatch(self):
        marker = 'case "${1:-show}" in  show)'
        override = '\n'.join('do_' + name + '() { printf "<%s>\n" "$@"; }' for name in ('mem', 'gc', 'charge')) + '\n'
        self.assertEqual(MASTER.count(marker), 1)
        self.script.write_text(MASTER.replace(marker, override + marker))
        args = ['run', 'a b', '', '*', "quote'arg", '$HOME', 'six']
        for command in ('mem', 'gc', 'charge'):
            result = self.cli(command, *args)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(result.stdout.startswith(''.join('<' + a + '>\n' for a in args)))

    def test_boot_filter_and_literal_glob(self):
        adb = self.root / 'adb'
        adb.mkdir()
        (adb / 'tca-ds-guard').touch()
        (adb / 'tca-webui.conf').write_text('# comment\n  mem guard on\nmem cap 3072\ngc apply\ngc preset stock\ngc soft-restart\nmem status\ngc status\nmem guard off\ncharge cool 15000 1700\nfas set min_khz *')
        service = self.root / 'service.sh'
        service.write_text((MODULE / 'service.sh').read_text().replace('/data/adb', shlex.quote(str(adb))))
        self.script.write_text('printf "CALL"; printf " <%s>" "$@"; printf "\n"\n')
        self.mock('sleep', 'exit 0')
        result = self.cli(script=service)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((adb / 'tca-ds-guard').exists())
        self.assertEqual(result.stdout.splitlines(), ['CALL <mem> <status>', 'CALL <gc> <status>', 'CALL <mem> <guard> <off>', 'CALL <charge> <cool> <15000> <1700>', 'CALL <fas> <set> <min_khz> <*>'])
        self.assertIn('拒绝', result.stderr)

    @unittest.skipUnless(shutil.which('node'), 'node required')
    def test_webui_quote(self):
        source = (MODULE / 'webroot/app.js').read_text()
        fn = next(line.strip() for line in source.splitlines() if 'function shq(' in line)
        value = "path with ' apostrophe; $HOME *"
        result = subprocess.run(['node', '-e', fn + ';process.stdout.write(shq(process.argv[1]));', value], text=True, capture_output=True, check=True)
        echo = subprocess.run(['sh', '-c', 'printf "%s" ' + result.stdout], text=True, capture_output=True, check=True)
        self.assertEqual(echo.stdout, value)


if __name__ == '__main__':
    unittest.main(verbosity=2)
