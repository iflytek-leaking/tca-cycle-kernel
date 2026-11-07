#!/usr/bin/env python3
"""Host-only checks of extracted FAS C; no kernel build or target access."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "kernel_ums9620_se50/drivers/tca_fas/tca_fas.c"


def function(source, name):
    match = re.search(r"static (?:void|int) " + name + r"\([^)]*\)\s*\{", source)
    if not match:
        raise AssertionError(f"missing function: {name}")
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PRELUDE = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint64_t u64;
#define S32_MAX INT32_MAX
#define TCA_FAS_MAX_POL 4
#define TCA_FAS_QUANT_KHZ 25000U
#define FREQ_QOS_MAX 1
#define FREQ_QOS_MIN 2
#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define for_each_possible_cpu(cpu) for ((cpu) = 0; (cpu) < 4; (cpu)++)
#define pr_info(...) ((void)0)
struct freq_constraints { int unused; };
struct freq_qos_request { bool active; unsigned int value; };
struct cpufreq_policy {
    struct { unsigned int min_freq, max_freq; } cpuinfo;
    struct freq_constraints constraints;
    int *related_cpus;
};
static struct cpufreq_policy policies[2];
static int leaders[2] = {0, 2};
static int refs, live, calls, fail_at, success_value;
static struct cpufreq_policy *cpufreq_cpu_get(int cpu)
{
    refs++;
    return &policies[cpu / 2];
}
static void cpufreq_cpu_put(struct cpufreq_policy *p)
{
    (void)p;
    assert(refs > 0);
    refs--;
}
static int cpumask_first(int *p) { return *p; }
static int freq_qos_add_request(struct freq_constraints *q,
                               struct freq_qos_request *r, int type,
                               unsigned int value)
{
    (void)q;
    (void)type;
    assert(!r->active);
    if (calls++ == fail_at)
        return -ENOMEM;
    r->active = true;
    r->value = value;
    live++;
    return success_value;
}
static int freq_qos_remove_request(struct freq_qos_request *r)
{
    assert(r->active && live > 0);
    r->active = false;
    live--;
    return 1;
}
static int freq_qos_update_request(struct freq_qos_request *r, unsigned int v)
{
    assert(r->active);
    r->value = v;
    return 1;
}
"""

GLOBALS = r"""
static struct fas_cluster fas_cl[TCA_FAS_MAX_POL];
static int fas_ncl, fas_big = -1;
static unsigned int fas_applied, fas_applied_little;
static unsigned int fas_ceiling, fas_max_khz, fas_min_pct;
static u64 fas_cl_busy(struct fas_cluster *c) { (void)c; return 123; }
"""

TESTS = r"""
static void setup(unsigned int low, unsigned int high)
{
    assert(!refs && !live);
    policies[0].cpuinfo.min_freq = 307123;
    policies[0].cpuinfo.max_freq = 1700999;
    policies[1].cpuinfo.min_freq = low;
    policies[1].cpuinfo.max_freq = high;
    policies[0].related_cpus = &leaders[0];
    policies[1].related_cpus = &leaders[1];
    calls = 0;
    fail_at = -1;
}
static void bounds(void)
{
    unsigned int inputs[] = {0, 1, 611122, 611123, 611124, 999999,
                             2799122, 2799123, 2799124, UINT32_MAX};
    unsigned int i;
    setup(611123, 2799123);
    assert(fas_policy_init() == 0 && fas_big == 1);
    for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        unsigned int x = inputs[i];
        unsigned int expected = clamp(x, 611123U, 2799123U);
        fas_apply(x);
        assert(fas_applied == expected);
        assert(fas_cl[1].qmax.value == expected);
        assert(fas_applied_little >= 307123 && fas_applied_little <= 1700999);
        if (x <= 611123)
            assert(fas_applied_little == 307123);
        if (x >= 2799123)
            assert(fas_applied_little == 1700999);
        fas_apply_floor(x);
        assert(fas_cl[1].qmin.value == (x ? expected : 0));
        if (x)
            assert(fas_cl[0].qmin.value >= 307123 && fas_cl[0].qmin.value <= 1700999);
        else
            assert(fas_cl[0].qmin.value == 0);
    }
    fas_policy_cleanup();
    setup(2799123, 2799123);
    assert(fas_policy_init() == 0 && fas_big == 1);
    fas_apply(0);
    assert(fas_applied == 2799123 && fas_applied_little == 307123);
    fas_apply(UINT32_MAX);
    assert(fas_applied == 2799123 && fas_applied_little == 307123);
    fas_apply_floor(1);
    assert(fas_cl[1].qmin.value == 2799123 && fas_cl[0].qmin.value == 307123);
    fas_apply_floor(0);
    assert(!fas_cl[0].qmin.value && !fas_cl[1].qmin.value);
    fas_policy_cleanup();
}
static void qos(void)
{
    int result, failure;
    for (result = 0; result <= 1; result++) {
        for (failure = -1; failure < 4; failure++) {
            setup(611123, 2799123);
            success_value = result;
            fail_at = failure;
            if (failure < 0) {
                assert(fas_policy_init() == 0);
                assert(fas_ncl == 2 && refs == 2 && live == 4);
                assert(fas_cl[0].has_max && fas_cl[0].has_min);
                assert(fas_cl[1].has_max && fas_cl[1].has_min);
                fas_policy_cleanup();
            } else {
                assert(fas_policy_init() == -ENOMEM);
            }
            assert(!fas_ncl && fas_big == -1 && !refs && !live);
            fail_at = -1;
            calls = 0;
            assert(fas_policy_init() == 0);
            fas_policy_cleanup();
            assert(!refs && !live);
        }
    }
    setup(2799123, 611123);
    assert(fas_policy_init() == -EINVAL);
    assert(!refs && !live && !fas_ncl && fas_big == -1);
}
static void quant(void)
{
    unsigned int cap, pct, need;
    setup(611123, 2799123);
    assert(fas_policy_init() == 0);
    for (cap = 0; cap <= 3000000; cap += 7777) {
        fas_max_khz = cap;
        for (pct = 5; pct <= 90; pct += 5) {
            fas_min_pct = pct;
            for (need = 0; need <= 3200000; need += 13217) {
                struct fas_cluster *b = &fas_cl[fas_big];
                unsigned int hi = cap ? clamp(cap, b->hw_min, b->hw_max) : b->hw_max;
                unsigned int lo = b->hw_min + (u64)(b->hw_max - b->hw_min) * pct / 100;
                unsigned int q;
                if (lo > hi)
                    lo = hi;
                fas_ceiling = need;
                q = extracted_quant(b);
                assert(lo <= q && q <= hi);
                fas_apply(q);
                assert(fas_applied == q);
            }
        }
    }
    fas_policy_cleanup();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "bounds")) bounds();
    else if (!strcmp(argv[1], "qos")) qos();
    else if (!strcmp(argv[1], "quant")) quant();
    else abort();
    assert(!refs && !live);
    return 0;
}
"""


class FasBoundsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shlex.split(os.environ.get("CC", "cc"))
        if not cc or not shutil.which(cc[0]):
            raise unittest.SkipTest("host C compiler unavailable")
        source = SOURCE.read_text()
        cluster = re.search(r"struct fas_cluster \{.*?\n\};", source, re.S).group()
        quant_start = source.index("\tlo = b->hw_min", source.index("static void tca_fas_work_fn"))
        quant_end = source.index("\n\tif (q != fas_applied)", quant_start)
        quant_source = ("static unsigned int extracted_quant(struct fas_cluster *b) {\n"
                        "unsigned int lo, hi, q;\n" + source[quant_start:quant_end]
                        + "\nreturn q;\n}\n")
        actual = "\n".join(function(source, name) for name in (
            "fas_policy_cleanup", "fas_policy_init", "fas_apply", "fas_apply_floor"))
        cls.temp = tempfile.TemporaryDirectory(prefix="tca-fas-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        path = Path(cls.temp.name)
        harness = path / "fas_test.c"
        harness.write_text(PRELUDE + cluster + GLOBALS + actual + quant_source + TESTS)
        cls.binary = path / "fas_test"
        subprocess.run(cc + ["-std=gnu99", "-Wall", "-Wextra", "-Werror",
                             str(harness), "-o", str(cls.binary)], check=True)

    def test_hardware_bounds_and_zero_range(self):
        subprocess.run([str(self.binary), "bounds"], check=True)

    def test_qos_success_and_failure_rollback(self):
        subprocess.run([str(self.binary), "qos"], check=True)

    def test_quantization_stays_inside_effective_bounds(self):
        subprocess.run([str(self.binary), "quant"], check=True)


if __name__ == "__main__":
    unittest.main()
