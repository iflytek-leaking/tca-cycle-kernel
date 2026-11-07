#!/usr/bin/env python3
"""Abstract recovery lifecycle tests; does NOT execute or verify kernel C.

Models the gate lock and PM mutex as atomic transitions, with distinct normal
IRQ and recovery-owned disable references. Run with Python unittest.
Scope is common display suspend/resume/remove in the active harrow profile.
Optional HX_CONFIG_PM callbacks are disabled there and are NOT modeled/audited.
"""
import unittest
from itertools import permutations


class Recovery:
    def __init__(self):
        self.normal = 0
        self.owned = 0
        self.pending = False
        self.running = False
        self.blocked = False
        self.display = False
        self.removing = False
        self.freed = False
        self.reloads = 0

    @property
    def depth(self):
        return self.normal + self.owned

    def schedule(self):
        if self.blocked or self.pending:
            return False
        assert not self.freed
        self.pending = True
        self.owned = 1
        return True

    def run(self):
        assert self.pending and not self.freed
        self.running = True
        if not self.blocked:
            self.reloads += 1

    def finish(self):
        self.owned = 0
        self.pending = self.running = False

    def stop(self):
        self.blocked = True
        # cancel_work_sync either cancels queued work or waits for running work.
        # Both paths then release exactly the owned reference, never normal.
        self.finish()

    def start(self):
        if not (self.display or self.removing):
            self.blocked = False

    def suspend(self):
        self.normal = 1
        self.stop()
        self.display = True

    def resume(self):
        self.stop()
        self.display = False
        self.reloads += 1
        self.normal = 0
        self.start()
        return True

    def remove(self):
        self.removing = True
        self.stop()
        assert not self.pending and not self.owned
        self.freed = True


class RecoveryTests(unittest.TestCase):
    def test_queued_cancel_and_running_drain(self):
        for running in (False, True):
            with self.subTest(running=running):
                r = Recovery()
                self.assertTrue(r.schedule())
                if running:
                    r.run()
                r.suspend()
                self.assertEqual(r.depth, 1)
                self.assertFalse(r.pending)
                self.assertFalse(r.schedule())
                r.resume()
                self.assertEqual(r.depth, 0)
                self.assertTrue(r.schedule())

    def test_normal_disable_worker_finish_order(self):
        for order in permutations(('normal_disable', 'finish')):
            r = Recovery()
            r.schedule()
            r.run()
            for op in order:
                if op == 'normal_disable':
                    r.normal = 1
                else:
                    r.finish()
            self.assertEqual(r.depth, 1)
            r.stop()  # idempotent: no second put
            self.assertEqual(r.depth, 1)
            r.resume()
            self.assertEqual(r.depth, 0)

    def test_gate_enqueue_stop_serializations(self):
        for order in permutations(('schedule', 'stop')):
            r = Recovery()
            for op in order:
                getattr(r, op)()
            self.assertFalse(r.pending)
            self.assertEqual(r.owned, 0)
            self.assertFalse(r.schedule())

    def test_remove_queued_running_and_late_resume(self):
        for running in (False, True):
            r = Recovery()
            r.schedule()
            if running:
                r.run()
            r.remove()
            r.start()  # late rearm cannot undo permanent removal gate
            self.assertFalse(r.schedule())
            self.assertFalse(r.pending)
            self.assertEqual(r.owned, 0)

    def test_display_gate_stays_closed_until_resume(self):
        r = Recovery()
        r.suspend()
        r.start()
        self.assertFalse(r.schedule())
        self.assertTrue(r.resume())
        self.assertTrue(r.schedule())

    def test_lcd_resume_drains_recovery_without_common_suspend(self):
        r = Recovery()
        r.schedule()
        r.run()
        r.normal = 1  # stock LCD caller uses himax_int_enable(0)
        r.resume()
        self.assertFalse(r.pending)
        self.assertEqual(r.depth, 0)
        self.assertEqual(r.reloads, 2)
        self.assertTrue(r.schedule())

    def test_duplicate_schedule_owns_one_reference(self):
        r = Recovery()
        self.assertTrue(r.schedule())
        self.assertFalse(r.schedule())
        self.assertEqual(r.depth, 1)
        r.finish()
        self.assertEqual(r.depth, 0)


if __name__ == '__main__':
    unittest.main()
