# PANIC 2026-09-26 - xfrm netns teardown double free (a real MM bug)

## 1. Conclusion

Stopping a Droidspaces container panics the kernel. It IS a memory-management bug,
not a guess:

- Oops header: 'Internal error: Oops - BUG: 0 [#1] PREEMPT SMP'
- Crash PC : __slab_free+0x30c/0x370    LR: kfree+0x2dc/0x444
- Workqueue: netns cleanup_net  (network namespace teardown)
- Trace:

    __slab_free+0x30c/0x370
    kfree+0x2dc/0x444
    xfrm_sysctl_fini+0x40/0x50
    xfrm_net_exit+0x18/0x50
    cleanup_net+0x2e4/0x55c
    process_one_work / worker_thread / kthread

  then 'Kernel panic - not syncing: Fatal exception'.

## 2. Root cause: vendor CONFIG_XFRM_FRAGMENT keeps global state in a per-net path

net/xfrm/xfrm_sysctl.c (before fix):

    static int enable_xfrm_fragment __read_mostly = 1;
    static struct ctl_table *sprd_sys_ctl_table;      /* file-scope global */
    static struct ctl_table_header *sprd_xfrm_hdr;    /* file-scope global */

    int  __net_init xfrm_sysctl_init(struct net *net)   /* per netns */
    void __net_exit xfrm_sysctl_fini(struct net *net)   /* per netns */
    {
        ...
        if (sprd_xfrm_hdr) {
            unregister_net_sysctl_table(sprd_xfrm_hdr);  /* last netns that init'ed */
            kfree(sprd_sys_ctl_table);                   /* <-- crash here (+0x40/0x50) */
        }
    }

Both sprd_* variables are global, but init/fini run once per netns. Every new
netns overwrites them, so tearing down any netns that is not the last one to
init kfree()s another netns' (or an already freed) table -> invalid free ->
__slab_free BUG().

Droidspaces creates/destroys a netns on every container start/stop, so it hits
this constantly. Stock Android hardly ever churns netns, so the vendor bug sat
dormant; enabling the container configs (IPC_NS/PID_NS/USER_NS + Droidspaces)
is what made it reachable. Not introduced by us, but exposed by us.

## 3. Fix

enable_xfrm_fragment is a single global switch, so register it once for
init_net only, and stop touching the globals in xfrm_sysctl_fini().
One file changed (net/xfrm/xfrm_sysctl.c), no struct layout change -> ABI safe.

    #ifdef CONFIG_XFRM_FRAGMENT
        if (net == &init_net) {
            sprd_sys_ctl_table = kmemdup(sprd_net_sys_table, ...);
            sprd_sys_ctl_table[0].data = &enable_xfrm_fragment;
            sprd_xfrm_hdr = register_net_sysctl(net, "net/core", sprd_sys_ctl_table);
            ...
        }
    #endif

    void __net_exit xfrm_sysctl_fini(struct net *net)
    {
        table = net->xfrm.sysctl_hdr->ctl_table_arg;
        unregister_net_sysctl_table(net->xfrm.sysctl_hdr);
        kfree(table);
        /* enable_xfrm_fragment is registered only for init_net, which never
         * exits; there is nothing per-net to release here. */
    }

Behaviour: /proc/sys/net/core/enable_xfrm_fragment now exists only in the root
netns (containers do not get it). get_xfrm_fragment() still reads the same
global, so the data path is unchanged.

## 4. Evidence

- reports/panic-20260926-xfrm/console-ramoops-0.txt  (full pstore console)
- reports/panic-20260926-xfrm/dmesg-crash-only.txt
- source bundle tca-logs-20260926-194905.tar.gz
  sha256 0dbd513c3c27018d7ec6a12f89bfc32507c1859d852cfc6a4a1be2e521c4aa5c

## 5. Verification

- [x] source located (net/xfrm/xfrm_sysctl.c)
- [ ] rebuild -> asm-offsets diff empty + verify-vendor-modules.py PASS
- [ ] on device: repeatedly droidspaces start/stop several containers and
      confirm cleanup_net no longer oopses
      (dmesg | grep -E 'xfrm_sysctl_fini|__slab_free|cleanup_net' should be empty)
