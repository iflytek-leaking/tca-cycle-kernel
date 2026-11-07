cmd_/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o := clang -Wp,-MD,/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/.kasumi_patch_memory.o.d -nostdinc -isystem /home/misakapardola/leaking-iflytek/recovery_twrp/prebuilts/clang/host/linux-x86/clang-r416183b/lib64/clang/12.0.5/include -I/home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include -I./arch/arm64/include/generated -I/home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include -I./include -I/home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I/home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi -I./include/generated/uapi -include /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kconfig.h -include /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compiler_types.h -D__KERNEL__ -mlittle-endian -DKASAN_SHADOW_SCALE_SHIFT=3 -Qunused-arguments -Wall -Werror -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu89 --target=aarch64-linux-gnu --prefix=/usr/bin/aarch64-linux-gnu- --gcc-toolchain=/usr -Werror=unknown-warning-option -mgeneral-regs-only -DCONFIG_AS_LSE=1 -DCONFIG_CC_HAS_K_CONSTRAINT=1 -fno-asynchronous-unwind-tables -Wno-psabi -mbranch-protection=none -ffixed-x18 -DKASAN_SHADOW_SCALE_SHIFT=3 -fno-delete-null-pointer-checks -Wno-frame-address -Wno-address-of-packed-member -O2 -Wframe-larger-than=4096 -fstack-protector-strong -Wno-format-invalid-specifier -Wno-gnu -Wno-tautological-compare -mno-global-merge -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang -g -gdwarf-4 -pg -fsanitize=shadow-call-stack -flto=thin -fsplit-lto-unit -fvisibility=default -fsanitize=cfi -fno-sanitize-cfi-canonical-jump-tables -fno-sanitize-blacklist -fsanitize-cfi-cross-dso -ftrap-function=__ubsan_handle_cfi_check_fail_abort -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-array-bounds -fno-strict-overflow -fno-merge-all-constants -fno-stack-check -Werror=date-time -Werror=incompatible-pointer-types -fmacro-prefix-map=/home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/= -Wno-initializer-overrides -Wno-format -Wno-sign-compare -Wno-format-zero-length -Wno-pointer-to-enum-cast -Wno-error -gz=none -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/include -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/internal -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/core -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/control -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/policy -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/overlay -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks -I/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/features   -fsanitize=bounds  -fsanitize-undefined-trap-on-error  -DMODULE  -DKBUILD_BASENAME='"kasumi_patch_memory"' -DKBUILD_MODNAME='"kasumi_lkm"' -c -o /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.c

source_/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o := /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.c

deps_/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o := \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kconfig.h \
    $(wildcard include/config/cc/version/text.h) \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compiler_attributes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compiler-clang.h \
    $(wildcard include/config/lto/clang.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/../kasumi_patch_memory.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/types.h \
  arch/arm64/include/generated/uapi/asm/types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/int-ll64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/int-ll64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/bitsperlong.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitsperlong.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/bitsperlong.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/posix_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stddef.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/stddef.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/posix_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/posix_types.h \
  include/generated/uapi/linux/version.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/insn.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/build_bug.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/debug/entry.h) \
    $(wildcard include/config/kasan.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/barrier.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kasan-checks.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/barrier.h \
    $(wildcard include/config/smp.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/atomic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/atomic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cmpxchg.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/lse.h \
    $(wildcard include/config/as/lse.h) \
    $(wildcard include/config/arm64/lse/atomics.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/cc/has/k/constraint.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stringify.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/export.h \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
    $(wildcard include/config/unused/symbols.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/jump_label.h \
    $(wildcard include/config/jump/label.h) \
    $(wildcard include/config/have/arch/jump/label/relative.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/jump_label.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/alternative.h \
    $(wildcard include/config/arm64/uao.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cpucaps.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/init.h \
    $(wildcard include/config/strict/kernel/rwx.h) \
    $(wildcard include/config/strict/module/rwx.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/atomic_lse.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/atomic-instrumented.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/atomic-fallback.h \
    $(wildcard include/config/generic/atomic64.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/atomic-long.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/sprd/core/ctl.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/arch/has/refcount.h) \
    $(wildcard include/config/panic/timeout.h) \
    $(wildcard include/config/tracing.h) \
  /home/misakapardola/leaking-iflytek/recovery_twrp/prebuilts/clang/host/linux-x86/clang-r416183b/lib64/clang/12.0.5/include/stdarg.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/limits.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/limits.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/limits.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/linkage.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/linkage.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bitops.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bits.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/const.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/const.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/const.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/bits.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/bitops.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/builtin-__ffs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/builtin-ffs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/builtin-__fls.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/builtin-fls.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/ffz.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/fls64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/sched.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/hweight.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/arch_hweight.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/const_hweight.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/atomic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/lock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/non-atomic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/le.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/byteorder.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/byteorder/little_endian.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/byteorder/little_endian.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/swab.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/swab.h \
  arch/arm64/include/generated/uapi/asm/swab.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/swab.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/byteorder/generic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/typecheck.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/printk.h \
    $(wildcard include/config/message/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/quiet.h) \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk/nmi.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
    $(wildcard include/config/dynamic/debug/core.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kern_levels.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/kernel.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/sysinfo.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/kasan/sw/tags.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cputype.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/broken/gas/inst.h) \
    $(wildcard include/config/arm64/pa/bits/52.h) \
    $(wildcard include/config/arm64/4k/pages.h) \
    $(wildcard include/config/arm64/16k/pages.h) \
    $(wildcard include/config/arm64/64k/pages.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/dynamic_debug.h \
  arch/arm64/include/generated/asm/div64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/div64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bitmap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
    $(wildcard include/config/fortify/source.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/string.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/string.h \
    $(wildcard include/config/arch/has/uaccess/flushcache.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bug.h \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/bug/on/data/corruption.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/bug.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/debug/bugverbose.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/brk-imm.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/gfp.h \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/zone/dma32.h) \
    $(wildcard include/config/zone/device.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/pm/sleep.h) \
    $(wildcard include/config/contig/alloc.h) \
    $(wildcard include/config/cma.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mmdebug.h \
    $(wildcard include/config/debug/vm.h) \
    $(wildcard include/config/debug/virtual.h) \
    $(wildcard include/config/debug/vm/pgflags.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/memory/isolation.h) \
    $(wildcard include/config/shuffle/page/allocator.h) \
    $(wildcard include/config/shadow/call/stack.h) \
    $(wildcard include/config/lru/balance/base/thrashing.h) \
    $(wildcard include/config/memcg.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/compaction.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/transparent/hugepage.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/page/extension.h) \
    $(wildcard include/config/deferred/struct/page/init.h) \
    $(wildcard include/config/have/memory/present.h) \
    $(wildcard include/config/have/memoryless/nodes.h) \
    $(wildcard include/config/have/memblock/node/map.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
    $(wildcard include/config/have/arch/early/pfn/to/nid.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/memory/hotremove.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
    $(wildcard include/config/holes/in/zone.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/preemption.h) \
    $(wildcard include/config/debug/lock/alloc.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/preempt.h \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/trace/preempt/toggle.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
    $(wildcard include/config/page/poisoning/zero.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/preempt.h \
    $(wildcard include/config/preempt.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/thread_info.h \
    $(wildcard include/config/thread/info/in/task.h) \
    $(wildcard include/config/have/arch/within/stack/frames.h) \
    $(wildcard include/config/hardened/usercopy.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/restart_block.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/time64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/math64.h \
    $(wildcard include/config/arch/supports/int128.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/math64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/time64.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/time.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/time_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/errno.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/errno.h \
  arch/arm64/include/generated/uapi/asm/errno.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/errno.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/errno-base.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/current.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/arm64/sw/ttbr0/pan.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/arm64/va/bits.h) \
    $(wildcard include/config/arm64/va/bits/52.h) \
    $(wildcard include/config/kasan/shadow/offset.h) \
    $(wildcard include/config/vmap/stack.h) \
    $(wildcard include/config/debug/align/rodata.h) \
    $(wildcard include/config/efi.h) \
    $(wildcard include/config/arm/gic/v3/its.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sizes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/page-def.h \
    $(wildcard include/config/arm64/page/shift.h) \
    $(wildcard include/config/arm64/cont/shift.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/memory_model.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pfn.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/stack_pointer.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/irqflags.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/ptrace.h \
    $(wildcard include/config/compat.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/arm64/sve.h) \
    $(wildcard include/config/arm64/cnp.h) \
    $(wildcard include/config/arm64/ptr/auth.h) \
    $(wildcard include/config/arm64/pseudo/nmi.h) \
    $(wildcard include/config/arm64/debug/priority/masking.h) \
    $(wildcard include/config/arm64/ssbd.h) \
    $(wildcard include/config/arm64/pa/bits.h) \
    $(wildcard include/config/arm64/amu/extn.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/hwcap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/hwcap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/ptrace.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/sve_context.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bottom_half.h \
  arch/arm64/include/generated/asm/mmiowb.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/mmiowb.h \
    $(wildcard include/config/mmiowb.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/spinlock_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/spinlock_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/paravirt.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/qrwlock_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/lockdep.h \
    $(wildcard include/config/lock/stat.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rwlock_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/spinlock.h \
  arch/arm64/include/generated/asm/qrwlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/qrwlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/kuser/helpers.h) \
    $(wildcard include/config/arm64/force/52bit.h) \
    $(wildcard include/config/have/hw/breakpoint.h) \
    $(wildcard include/config/arm64/tagged/addr/abi.h) \
    $(wildcard include/config/gcc/plugin/stackleak.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/processor.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/vdso/processor.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/hw_breakpoint.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/virt.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/sections.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/sections.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/pgtable/levels.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/pointer_auth.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/fpsimd.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/sigcontext.h \
  arch/arm64/include/generated/asm/qspinlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/qspinlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rwlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/inline/spin/lock.h) \
    $(wildcard include/config/inline/spin/lock/bh.h) \
    $(wildcard include/config/inline/spin/lock/irq.h) \
    $(wildcard include/config/inline/spin/lock/irqsave.h) \
    $(wildcard include/config/inline/spin/trylock.h) \
    $(wildcard include/config/inline/spin/trylock/bh.h) \
    $(wildcard include/config/uninline/spin/unlock.h) \
    $(wildcard include/config/inline/spin/unlock/bh.h) \
    $(wildcard include/config/inline/spin/unlock/irq.h) \
    $(wildcard include/config/inline/spin/unlock/irqrestore.h) \
    $(wildcard include/config/generic/lockbreak.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/inline/read/lock.h) \
    $(wildcard include/config/inline/write/lock.h) \
    $(wildcard include/config/inline/read/lock/bh.h) \
    $(wildcard include/config/inline/write/lock/bh.h) \
    $(wildcard include/config/inline/read/lock/irq.h) \
    $(wildcard include/config/inline/write/lock/irq.h) \
    $(wildcard include/config/inline/read/lock/irqsave.h) \
    $(wildcard include/config/inline/write/lock/irqsave.h) \
    $(wildcard include/config/inline/read/trylock.h) \
    $(wildcard include/config/inline/write/trylock.h) \
    $(wildcard include/config/inline/read/unlock.h) \
    $(wildcard include/config/inline/write/unlock.h) \
    $(wildcard include/config/inline/read/unlock/bh.h) \
    $(wildcard include/config/inline/write/unlock/bh.h) \
    $(wildcard include/config/inline/read/unlock/irq.h) \
    $(wildcard include/config/inline/write/unlock/irq.h) \
    $(wildcard include/config/inline/read/unlock/irqrestore.h) \
    $(wildcard include/config/inline/write/unlock/irqrestore.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/wait.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/wait.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/seqlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/nodemask.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pageblock-flags.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/hugetlb/page/size/variable.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/page-flags-layout.h \
    $(wildcard include/config/numa/balancing.h) \
  include/generated/bounds.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/sparsemem.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mm_types.h \
    $(wildcard include/config/have/aligned/struct/page.h) \
    $(wildcard include/config/userfaultfd.h) \
    $(wildcard include/config/swap.h) \
    $(wildcard include/config/speculative/page/fault/debug.h) \
    $(wildcard include/config/have/arch/compat/mmap/bases.h) \
    $(wildcard include/config/membarrier.h) \
    $(wildcard include/config/aio.h) \
    $(wildcard include/config/mmu/notifier.h) \
    $(wildcard include/config/arch/want/batched/unmap/tlb/flush.h) \
    $(wildcard include/config/gki/opt/features.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mm_types_task.h \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/arch/enable/split/pmd/ptlock.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/page.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/personality.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/personality.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/pgtable-types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/pgtable-nopud.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/pgtable-nop4d-hack.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/5level-fixup.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/getorder.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/auxvec.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/auxvec.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/auxvec.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rbtree.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcupdate.h \
    $(wildcard include/config/preempt/rcu.h) \
    $(wildcard include/config/rcu/stall/common.h) \
    $(wildcard include/config/no/hz/full.h) \
    $(wildcard include/config/rcu/nocb/cpu.h) \
    $(wildcard include/config/tasks/rcu.h) \
    $(wildcard include/config/tree/rcu.h) \
    $(wildcard include/config/tiny/rcu.h) \
    $(wildcard include/config/debug/objects/rcu/head.h) \
    $(wildcard include/config/prove/rcu.h) \
    $(wildcard include/config/rcu/boost.h) \
    $(wildcard include/config/arch/weak/release/acquire.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcutree.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rwsem.h \
    $(wildcard include/config/rwsem/spin/on/owner.h) \
    $(wildcard include/config/debug/rwsems.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/err.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/osq_lock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/android_vendor.h \
    $(wildcard include/config/android/vendor/oem/data.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/completion.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/uprobes.h \
    $(wildcard include/config/uprobes.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/uprobes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/debug-monitors.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/esr.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/probes.h \
    $(wildcard include/config/kprobes.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
    $(wildcard include/config/sysfs.h) \
    $(wildcard include/config/wq/watchdog.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timer.h \
    $(wildcard include/config/debug/objects/timers.h) \
    $(wildcard include/config/preempt/rt.h) \
    $(wildcard include/config/no/hz/common.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ktime.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/time32.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timex.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/timex.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/param.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/param.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/param.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/timex.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/arm/arch/timer/ool/workaround.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/smp.h \
    $(wildcard include/config/up/late/init.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/llist.h \
    $(wildcard include/config/arch/have/nmi/safe/cmpxchg.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/arm64/acpi/parking/protocol.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/percpu.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/percpu.h \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/arm/arch/timer.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timecounter.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/timex.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/time32.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/time.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/jiffies.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/vdso/ktime.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timekeeping.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timekeeping32.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/android_kabi.h \
    $(wildcard include/config/android/kabi/reserve.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/mmu.h \
    $(wildcard include/config/unmap/kernel/at/el0.h) \
    $(wildcard include/config/randomize/base.h) \
    $(wildcard include/config/cavium/erratum/27456.h) \
    $(wildcard include/config/harden/branch/predictor.h) \
    $(wildcard include/config/harden/el2/vectors.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/page-flags.h \
    $(wildcard include/config/arch/uses/pg/uncached.h) \
    $(wildcard include/config/memory/failure.h) \
    $(wildcard include/config/idle/page/tracking.h) \
    $(wildcard include/config/thp/swap.h) \
    $(wildcard include/config/ksm.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/memory_hotplug.h \
    $(wildcard include/config/arch/has/add/pages.h) \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
    $(wildcard include/config/have/bootmem/info/node.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/notifier.h \
    $(wildcard include/config/tree/srcu.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mutex.h \
    $(wildcard include/config/mutex/spin/on/owner.h) \
    $(wildcard include/config/debug/mutexes.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/debug_locks.h \
    $(wildcard include/config/debug/locking/api/selftests.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/srcu.h \
    $(wildcard include/config/tiny/srcu.h) \
    $(wildcard include/config/srcu.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcu_segcblist.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/srcutree.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcu_node_tree.h \
    $(wildcard include/config/rcu/fanout.h) \
    $(wildcard include/config/rcu/fanout/leaf.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/topology.h \
    $(wildcard include/config/use/percpu/numa/node/id.h) \
    $(wildcard include/config/sched/smt.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/arch_topology.h \
    $(wildcard include/config/generic/arch/topology.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/percpu.h \
    $(wildcard include/config/need/per/cpu/embed/first/chunk.h) \
    $(wildcard include/config/need/per/cpu/page/first/chunk.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/topology.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/topology.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mm.h \
    $(wildcard include/config/sysctl.h) \
    $(wildcard include/config/have/arch/mmap/rnd/bits.h) \
    $(wildcard include/config/have/arch/mmap/rnd/compat/bits.h) \
    $(wildcard include/config/mem/soft/dirty.h) \
    $(wildcard include/config/arch/uses/high/vma/flags.h) \
    $(wildcard include/config/arch/has/pkeys.h) \
    $(wildcard include/config/ppc.h) \
    $(wildcard include/config/x86.h) \
    $(wildcard include/config/parisc.h) \
    $(wildcard include/config/ia64.h) \
    $(wildcard include/config/sparc64.h) \
    $(wildcard include/config/x86/intel/mpx.h) \
    $(wildcard include/config/have/arch/userfaultfd/minor.h) \
    $(wildcard include/config/stack/growsup.h) \
    $(wildcard include/config/speculative/page/fault.h) \
    $(wildcard include/config/shmem.h) \
    $(wildcard include/config/arch/has/pte/devmap.h) \
    $(wildcard include/config/dev/pagemap/ops.h) \
    $(wildcard include/config/device/private.h) \
    $(wildcard include/config/pci/p2pdma.h) \
    $(wildcard include/config/e/show/mem.h) \
    $(wildcard include/config/debug/vm/rb.h) \
    $(wildcard include/config/page/poisoning.h) \
    $(wildcard include/config/init/on/alloc/default/on.h) \
    $(wildcard include/config/init/on/free/default/on.h) \
    $(wildcard include/config/debug/pagealloc.h) \
    $(wildcard include/config/arch/has/set/direct/map.h) \
    $(wildcard include/config/hibernation.h) \
    $(wildcard include/config/hugetlbfs.h) \
    $(wildcard include/config/process/reclaim.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/mmap_lock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/range.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/percpu-refcount.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/bit_spinlock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/shrinker.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/resource.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/resource.h \
  arch/arm64/include/generated/uapi/asm/resource.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/resource.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/resource.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/page_ext.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stacktrace.h \
    $(wildcard include/config/stacktrace.h) \
    $(wildcard include/config/arch/stackwalk.h) \
    $(wildcard include/config/have/reliable/stacktrace.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stackdepot.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/page_ref.h \
    $(wildcard include/config/debug/page/ref.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/tracepoint-defs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/static_key.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/memremap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ioport.h \
    $(wildcard include/config/io/strict/devmem.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/overflow.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/pgtable.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/proc-fns.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/pgtable-prot.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/tlbflush.h \
    $(wildcard include/config/arm64/workaround/repeat/tlbi.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched.h \
    $(wildcard include/config/virt/cpu/accounting/native.h) \
    $(wildcard include/config/sched/info.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/fair/group/sched.h) \
    $(wildcard include/config/rt/group/sched.h) \
    $(wildcard include/config/uclamp/task.h) \
    $(wildcard include/config/uclamp/buckets/count.h) \
    $(wildcard include/config/cgroup/sched.h) \
    $(wildcard include/config/blk/dev/io/trace.h) \
    $(wildcard include/config/psi.h) \
    $(wildcard include/config/compat/brk.h) \
    $(wildcard include/config/cgroups.h) \
    $(wildcard include/config/blk/cgroup.h) \
    $(wildcard include/config/stackprotector.h) \
    $(wildcard include/config/arch/has/scaled/cputime.h) \
    $(wildcard include/config/cpu/freq/times.h) \
    $(wildcard include/config/virt/cpu/accounting/gen.h) \
    $(wildcard include/config/posix/cputimers.h) \
    $(wildcard include/config/keys.h) \
    $(wildcard include/config/sysvipc.h) \
    $(wildcard include/config/detect/hung/task.h) \
    $(wildcard include/config/audit.h) \
    $(wildcard include/config/auditsyscall.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/ubsan.h) \
    $(wildcard include/config/ubsan/trap.h) \
    $(wildcard include/config/block.h) \
    $(wildcard include/config/task/xacct.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/x86/cpu/resctrl.h) \
    $(wildcard include/config/futex.h) \
    $(wildcard include/config/perf/events.h) \
    $(wildcard include/config/rseq.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/fault/injection.h) \
    $(wildcard include/config/latencytop.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/kcov.h) \
    $(wildcard include/config/bcache.h) \
    $(wildcard include/config/livepatch.h) \
    $(wildcard include/config/security.h) \
    $(wildcard include/config/arch/task/struct/on/stack.h) \
    $(wildcard include/config/debug/rseq.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/sched.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pid.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rculist.h \
    $(wildcard include/config/prove/rcu/list.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/refcount.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sem.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/sem.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ipc.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/uidgid.h \
    $(wildcard include/config/multiuser.h) \
    $(wildcard include/config/user/ns.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/highuid.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rhashtable-types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/ipc.h \
  arch/arm64/include/generated/uapi/asm/ipcbuf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/ipcbuf.h \
  arch/arm64/include/generated/uapi/asm/sembuf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/sembuf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/shm.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/shm.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/hugetlb_encode.h \
  arch/arm64/include/generated/uapi/asm/shmbuf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/shmbuf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/shmparam.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/shmparam.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kcov.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/kcov.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/plist.h \
    $(wildcard include/config/debug/plist.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/hrtimer.h \
    $(wildcard include/config/high/res/timers.h) \
    $(wildcard include/config/time/low/res.h) \
    $(wildcard include/config/timerfd.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/hrtimer_defs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/timerqueue.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
    $(wildcard include/config/have/arch/seccomp/filter.h) \
    $(wildcard include/config/seccomp/filter.h) \
    $(wildcard include/config/checkpoint/restore.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/seccomp.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/seccomp.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/unistd.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/unistd.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/unistd.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/seccomp.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/unistd.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/latencytop.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/prio.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/signal_types.h \
    $(wildcard include/config/old/sigaction.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/signal.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/signal.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/signal.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/signal.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/signal-defs.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/siginfo.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/task_io_accounting.h \
    $(wildcard include/config/task/io/accounting.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/posix-timers.h \
    $(wildcard include/config/posix/timers.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/alarmtimer.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/rseq.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/fixmap.h \
    $(wildcard include/config/acpi/apei/ghes.h) \
    $(wildcard include/config/arm/sde/interface.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/boot.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/fixmap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/pgtable.h \
    $(wildcard include/config/have/arch/transparent/hugepage/pud.h) \
    $(wildcard include/config/have/arch/soft/dirty.h) \
    $(wildcard include/config/arch/enable/thp/migration.h) \
    $(wildcard include/config/have/arch/huge/vmap.h) \
    $(wildcard include/config/x86/espfix64.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/huge_mm.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/coredump.h \
    $(wildcard include/config/core/dump/default/elf/headers.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/fs.h \
    $(wildcard include/config/sprd/debug.h) \
    $(wildcard include/config/read/only/thp/for/fs.h) \
    $(wildcard include/config/fs/posix/acl.h) \
    $(wildcard include/config/cgroup/writeback.h) \
    $(wildcard include/config/ima.h) \
    $(wildcard include/config/file/locking.h) \
    $(wildcard include/config/fsnotify.h) \
    $(wildcard include/config/fs/encryption.h) \
    $(wildcard include/config/fs/verity.h) \
    $(wildcard include/config/epoll.h) \
    $(wildcard include/config/unicode.h) \
    $(wildcard include/config/quota.h) \
    $(wildcard include/config/fs/dax.h) \
    $(wildcard include/config/mandatory/file/locking.h) \
    $(wildcard include/config/migration.h) \
    $(wildcard include/config/io/uring.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/wait_bit.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kdev_t.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/kdev_t.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/dcache.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rculist_bl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/list_bl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/lockref.h \
    $(wildcard include/config/arch/use/cmpxchg/lockref.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stringhash.h \
    $(wildcard include/config/dcache/word/access.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/hash.h \
    $(wildcard include/config/have/arch/hash.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/path.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/stat.h \
  arch/arm64/include/generated/uapi/asm/stat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/stat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/compat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/compat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/task_stack.h \
    $(wildcard include/config/debug/stack/usage.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/magic.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/stat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/list_lru.h \
    $(wildcard include/config/memcg/kmem.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/radix-tree.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/xarray.h \
    $(wildcard include/config/xarray/multi.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/capability.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/capability.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/semaphore.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/fcntl.h \
    $(wildcard include/config/arch/32bit/off/t.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/fcntl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/uapi/asm/fcntl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/fcntl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/fiemap.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/migrate_mode.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/percpu-rwsem.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcuwait.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rcu_sync.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/delayed_call.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/uuid.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/uuid.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/errseq.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ioprio.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/rt.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/iocontext.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/fs_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/fs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/ioctl.h \
  arch/arm64/include/generated/uapi/asm/ioctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/ioctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/ioctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/quota.h \
    $(wildcard include/config/quota/netlink/interface.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/percpu_counter.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/dqblk_xfs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/dqblk_v1.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/dqblk_v2.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/dqblk_qtree.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/projid.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/quota.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/nfs_fs_i.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/vmstat.h \
    $(wildcard include/config/vm/event/counters.h) \
    $(wildcard include/config/debug/tlbflush.h) \
    $(wildcard include/config/debug/vm/vmacache.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/vm_event_item.h \
    $(wildcard include/config/memory/balloon.h) \
    $(wildcard include/config/balloon/compaction.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/stop_machine.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cpu.h \
    $(wildcard include/config/pm/sleep/smp.h) \
    $(wildcard include/config/pm/sleep/smp/nonzero/cpu.h) \
    $(wildcard include/config/hotplug/smt.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/node.h \
    $(wildcard include/config/hmem/reporting.h) \
    $(wildcard include/config/memory/hotplug/sparse.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/device.h \
    $(wildcard include/config/acpi.h) \
    $(wildcard include/config/debug/devres.h) \
    $(wildcard include/config/generic/msi/irq/domain.h) \
    $(wildcard include/config/pinctrl.h) \
    $(wildcard include/config/generic/msi/irq.h) \
    $(wildcard include/config/dma/declare/coherent.h) \
    $(wildcard include/config/dma/cma.h) \
    $(wildcard include/config/arch/has/sync/dma/for/device.h) \
    $(wildcard include/config/arch/has/sync/dma/for/cpu.h) \
    $(wildcard include/config/arch/has/sync/dma/for/cpu/all.h) \
    $(wildcard include/config/of.h) \
    $(wildcard include/config/devtmpfs.h) \
    $(wildcard include/config/sysfs/deprecated.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kobject.h \
    $(wildcard include/config/uevent/helper.h) \
    $(wildcard include/config/debug/kobject/release.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sysfs.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kernfs.h \
    $(wildcard include/config/kernfs.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/idr.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kobject_ns.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kref.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/klist.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pm.h \
    $(wildcard include/config/vt/console/sleep.h) \
    $(wildcard include/config/pm.h) \
    $(wildcard include/config/pm/clk.h) \
    $(wildcard include/config/pm/generic/domains.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ratelimit.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/device.h \
    $(wildcard include/config/iommu/api.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pm_wakeup.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cpuhotplug.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/uaccess.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/uaccess.h \
    $(wildcard include/config/arm64/pan.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/kernel-pgtable.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/extable.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/cacheflush.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kgdb.h \
    $(wildcard include/config/have/arch/kgdb.h) \
    $(wildcard include/config/kgdb.h) \
    $(wildcard include/config/serial/kgdb/nmi.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/kgdb.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ptrace.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/signal.h \
    $(wildcard include/config/sched/autogroup.h) \
    $(wildcard include/config/bsd/process/acct.h) \
    $(wildcard include/config/taskstats.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/signal.h \
    $(wildcard include/config/proc/fs.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/jobctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/task.h \
    $(wildcard include/config/have/copy/thread/tls.h) \
    $(wildcard include/config/have/exit/thread.h) \
    $(wildcard include/config/arch/wants/dynamic/task/struct.h) \
    $(wildcard include/config/have/arch/thread/struct/whitelist.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cred.h \
    $(wildcard include/config/debug/credentials.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/key.h \
    $(wildcard include/config/net.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sysctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/sysctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/assoc_array.h \
    $(wildcard include/config/associative/array.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/sched/user.h \
    $(wildcard include/config/fanotify.h) \
    $(wildcard include/config/posix/mqueue.h) \
    $(wildcard include/config/bpf/syscall.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/pid_namespace.h \
    $(wildcard include/config/pid/ns.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/nsproxy.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ns_common.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/ptrace.h \
  /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/core/kasumi_runtime.h \
  /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/internal/kasumi_base.h \
  /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/include/kasumi_uapi.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/anon_inodes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kprobes.h \
    $(wildcard include/config/kretprobes.h) \
    $(wildcard include/config/kprobes/sanity/test.h) \
    $(wildcard include/config/optprobes.h) \
    $(wildcard include/config/kprobes/on/ftrace.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/ftrace.h \
    $(wildcard include/config/function/tracer.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/dynamic/ftrace/with/regs.h) \
    $(wildcard include/config/stack/tracer.h) \
    $(wildcard include/config/frame/pointer.h) \
    $(wildcard include/config/function/profiler.h) \
    $(wildcard include/config/ftrace/syscalls.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/trace_clock.h \
  arch/arm64/include/generated/asm/trace_clock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/trace_clock.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kallsyms.h \
    $(wildcard include/config/kallsyms/all.h) \
    $(wildcard include/config/kallsyms.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/module.h \
    $(wildcard include/config/modules/tree/lookup.h) \
    $(wildcard include/config/cfi/clang.h) \
    $(wildcard include/config/tracepoints.h) \
    $(wildcard include/config/bpf/events.h) \
    $(wildcard include/config/event/tracing.h) \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/constructors.h) \
    $(wildcard include/config/function/error/injection.h) \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/module/sig.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kmod.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/umh.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/elf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/compat/vdso.h) \
  arch/arm64/include/generated/asm/user.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/user.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/elf.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/elf-em.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/moduleparam.h \
    $(wildcard include/config/alpha.h) \
    $(wildcard include/config/ppc64.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/rbtree_latch.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/error-injection.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/error-injection.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/cfi.h \
    $(wildcard include/config/cfi/clang/shadow.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/module.h \
    $(wildcard include/config/arm64/module/plts.h) \
    $(wildcard include/config/arm64/erratum/843419.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/module.h \
    $(wildcard include/config/have/mod/arch/specific.h) \
    $(wildcard include/config/modules/use/elf/rel.h) \
    $(wildcard include/config/modules/use/elf/rela.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/ftrace.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/compat.h \
    $(wildcard include/config/arch/has/syscall/wrapper.h) \
    $(wildcard include/config/x86/x32/abi.h) \
    $(wildcard include/config/compat/old/sigaction.h) \
    $(wildcard include/config/odd/rt/sigaction.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/socket.h \
  arch/arm64/include/generated/uapi/asm/socket.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/socket.h \
  arch/arm64/include/generated/uapi/asm/sockios.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/asm-generic/sockios.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/sockios.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/uio.h \
    $(wildcard include/config/arch/has/uaccess/mcsafe.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/crypto/hash.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/crypto.h \
    $(wildcard include/config/crypto/stats.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/slab.h \
    $(wildcard include/config/debug/slab.h) \
    $(wildcard include/config/failslab.h) \
    $(wildcard include/config/have/hardened/usercopy/allocator.h) \
    $(wildcard include/config/slab.h) \
    $(wildcard include/config/slub.h) \
    $(wildcard include/config/slob.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/kasan.h \
    $(wildcard include/config/kasan/generic.h) \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/uio.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/socket.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/if.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/libc-compat.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/hdlc/ioctl.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/uapi/linux/aio_abi.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/syscall_wrapper.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/arch/arm64/include/asm/kprobes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/asm-generic/kprobes.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/seq_file.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/task_work.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/vmalloc.h \
  /home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/internal/kasumi_types.h \
  /home/misakapardola/leaking-iflytek/tcakernel/kernel_ums9620_se50/include/linux/hashtable.h \

/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o: $(deps_/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o)

$(deps_/home/misakapardola/leaking-iflytek/tcakernel/tools/kasumi/src/hooks/arm64/kasumi_patch_memory.o):
