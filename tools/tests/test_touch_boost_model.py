#!/usr/bin/env python3
"""Model-only regression for boost ownership; does NOT execute kernel C/locks."""
import unittest


class Boost:
    def __init__(self):
        self.enabled = dict.fromkeys(('freq', 'uclamp', 'idle'), True)
        self.active = dict.fromkeys(self.enabled, False)
        self.pending = self.queued = self.stopping = self.fail = False
        self.expiry = 0

    def release(self, key):
        if key != 'uclamp' or not self.fail:
            self.active[key] = False

    def release_all(self):
        for key in self.active:
            self.release(key)

    def hit(self, now):
        if self.stopping:
            return
        for key in self.enabled:
            if self.enabled[key]:
                self.active[key] = True
        if any(self.enabled.values()):
            self.pending = self.queued = True
            self.expiry = now + 150

    def off(self, key):
        self.enabled[key] = False
        self.release(key)
        if not any(self.enabled.values()):
            self.pending = False
            self.release_all()
            self.queued = False
        if self.active['uclamp'] and not self.enabled['uclamp']:
            self.queued = True

    def work(self, now):
        self.queued = False
        if self.stopping:
            return
        if not self.enabled['uclamp']:
            self.release('uclamp')
        if self.pending and now < self.expiry:
            self.queued = True
            return
        self.pending = False
        self.release_all()
        if self.active['uclamp']:
            self.queued = True

    def stop(self):
        self.stopping = True
        self.enabled = dict.fromkeys(self.enabled, False)
        self.pending = self.queued = False
        self.release_all()


class BoostModelTests(unittest.TestCase):
    def test_individual_disable_preserves_other_expiry(self):
        for key in ('freq', 'uclamp', 'idle'):
            with self.subTest(key=key):
                b = Boost()
                b.hit(0)
                b.off(key)
                self.assertFalse(b.active[key])
                self.assertTrue(all(v for k, v in b.active.items() if k != key))
                self.assertTrue(b.queued)
                b.work(150)
                self.assertFalse(any(b.active.values()))

    def test_old_worker_defers_to_new_hit(self):
        b = Boost()
        b.hit(0)
        b.hit(149)
        b.work(150)
        self.assertTrue(all(b.active.values()))
        b.work(299)
        self.assertFalse(any(b.active.values()))

    def test_all_off_order_and_stale_worker(self):
        for order in [('freq', 'uclamp', 'idle'), ('idle', 'freq', 'uclamp'), ('uclamp', 'idle', 'freq')]:
            with self.subTest(order=order):
                b = Boost()
                b.hit(0)
                for key in order:
                    b.off(key)
                b.work(50)
                self.assertFalse(any(b.active.values()))
                self.assertFalse(b.queued)

    def test_restore_failure_retains_ownership_until_retry(self):
        b = Boost()
        b.hit(0)
        b.fail = True
        for key in b.enabled:
            b.off(key)
        b.work(1000)
        self.assertTrue(b.active['uclamp'])
        self.assertTrue(b.queued)
        b.fail = False
        b.work(2000)
        self.assertFalse(any(b.active.values()))
        self.assertFalse(b.queued)

    def test_teardown_forbids_requeue(self):
        b = Boost()
        b.hit(0)
        b.stop()
        b.hit(100)
        b.work(200)
        self.assertFalse(b.queued)
        self.assertFalse(any(b.active.values()))


if __name__ == '__main__':
    unittest.main()
