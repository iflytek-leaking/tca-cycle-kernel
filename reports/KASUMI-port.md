# Kasumi 移植到 5.4.210（内核级路径隐藏）

## 1. Kasumi 是什么

`Rouyashiki/Kasumi`(前身 HymoFS)是一个 **out-of-tree LKM**(`kasumi_lkm.ko`),
做内核级**路径重定向 / 隐藏 / 目录合并**,并提供**伪造 /proc/<pid>/mountinfo、mounts、
SELinux 视角**等能力 —— 也就是给 KernelSU/ReSukiSU 做**反检测(root 隐藏)**。

- 控制面:root-only 的 `reboot` GET_FD 命令 + fd ioctl(`src/include/kasumi_uapi.h`)
- 上游 CI 目标:**android12-5.10 起**,我们内核是 **5.4.210**

## 2. 硬依赖:ftrace 函数钩子

它靠 ftrace 挂 4 个 VFS 函数:`vfs_getattr` / `d_path` / `iterate_dir` / `vfs_getxattr`
(`kasumi_ftrace_hooks.c`)。我们的内核原本 `CONFIG_FUNCTION_TRACER is not set`,所以先打开:

    CONFIG_FUNCTION_TRACER=y
    CONFIG_DYNAMIC_FTRACE=y

`-pg` + thin LTO + CFI 能过(gki 也这么开),重编 BUILD OK,ABI 不变。

## 3. 移植改动（原文件都在 tools/kasumi/src/）

| # | 文件 | 问题 | 处理 |
|---|---|---|---|
| 1 | internal/kasumi_base.h | `task_work_add()` 的 notify 参数 5.9 才变成 enum | <5.9 用 `bool` + `true` |
| 2 | core/kasumi_hide_events.c | `fsnotify_ops.handle_inode_event` 5.9 才有 | <5.9 实现 `handle_event`(从 `iter_info->marks[INODE]` 取 mark) |
| 3 | internal/kasumi_base.h | `cancel_work()` 5.8 才有 | <5.8 用 `cancel_work_sync()` |
| 4 | policy/kasumi_path_policy.c | `LOOKUP_BENEATH/NO_SYMLINKS/IN_ROOT/CACHED/...` 5.8-5.12 才有 | 按上游值补 0x10000+ 高位(与 5.4 位不冲突) |
| 5 | hooks/kasumi_ftrace_hooks.c | 缺 `#include kasumi_vfs_hooks.h` | 补上 |
| 6 | features/kasumi_fake_mountinfo.c | `seq_read_iter` 5.10 才有 | <5.10 用 `kernel_read()`(内部 set_fs) |
| 7 | kernel/trace/ftrace.c(内核) | `ftrace_set_filter_ips`(复数)5.4 没有,而 Kasumi 硬依赖 | **回移**该薄封装 + `EXPORT_SYMBOL_GPL` |

## 4. 编译结果

    tools/kasumi/src/kasumi_lkm.ko     5.0 MB
    vermagic: 5.4.210-tca-cycle SMP preempt mod_unload modversions aarch64
    MODPOST 1 modules,零 undefined 告警(168 个未解析符号全部由本内核导出)

运行时 kallsyms 依赖的符号全部就位:
`init_mm` / `proc_pid_attr_write` / `fuse_dentry_revalidate` /
`get_vfs_caps_from_disk` / `selinux_blob_sizes` / `show_mountinfo` /
`show_vfsmnt` / `vfs_readlink` / `__set_fixmap` —— **9/9 OK**。

## 5. 安装

1. **先刷带 ftrace 的内核**(必须):
   `boot-...-fas-ftrace-20260926.img` 或 `tca-cycle-kernel-...-ftrace-20260926.zip`
2. **装 Kasumi 模块**:
   `kasumi-0.1.0-5.4-port-magisk.zip`(Magisk/KSU 模块,post-fs-data 里 insmod)
   或手动:`su -c 'insmod /data/local/tmp/kasumi_lkm.ko'`
3. 内核会打印 `Kasumi: ...` 日志,用 `dmesg | grep -i kasumi` 确认。

## 6. 注意 / 风险

- **caveat 1:LOOKUP_* 标志**。5.4 的 `link_path_walk` 不认识 `LOOKUP_BENEATH/IN_ROOT/
  NO_SYMLINKS/CACHED`。Kasumi 自己的 walker 大部分会处理,但**内核侧的强制约束缺失**。
  如果上机发现重定向能越出 root,需要把这几个 flag 的语义回移到 `fs/namei.c`。
- **caveat 2:fsnotify 端口**。<5.9 用 `iter_info->marks[INODE]`,同一 inode 上多个 mark 时
  取第一个;Kasumi 每个路径组件一个 mark,正常情况够用。
- **caveat 3:需要用户态控制器**。Kasumi 只有内核机制,必须有一个用户态程序走
  `reboot GET_FD` + ioctl 下发隐藏规则。仓库里只有 UAPI 头(`src/include/kasumi_uapi.h`),
  没有现成 daemon。**只装 .ko 不会有任何效果。**
- **caveat 4:未上机验证**。编译/符号层面全绿,但 VFS hook 的实际行为必须上机测。

## 7. 验证

- [x] 内核开 ftrace → BUILD OK,`asm-offsets.h` diff 为空,160 原厂模块 0 未解析
- [x] Kasumi `kasumi_lkm.ko` 编译成功,vermagic 匹配,零 undefined
- [x] 运行时符号 9/9 就位
- [ ] 上机:insmod 成功 + `dmesg` 出 Kasumi 日志
- [ ] 上机:配合用户态控制器下发一条隐藏规则,验证 App 看不到目标路径

## 8. 2026-10-01 追加:缺少 android_vh_check_file_open(FATAL 阻塞)

上机后发现 kasumi_lkm.ko 初始化失败。根因是 kasumi_fop_bridge_init() 用
for_each_kernel_tracepoint() 找 ACK vendor hook android_vh_check_file_open,
找不到就 return -ENOENT;而该步骤在 kasumi_bootstrap_init() 里是 FATAL
(goto err_fop_bridge),所以整个 module_init 直接失败、insmod 返回错误。

证据(修复前):
- grep -rn android_vh_check_file_open kernel_ums9620_se50/ -> 0 处
- build/se50-clang-out/System.map -> 0 处(全内核只有 61 个 android_vh/rvh,没有它)
- git log -S android_vh_check_file_open --all -> 从未出现(不是清理时删的)

原因:上游 Kasumi 的 CI 目标是 android12-5.10 ~ android17-6.18 的 GKI 内核,
这个 hook 是 GKI/ACK 自带;我们这是 5.4 厂内核,必须自己补。

修复(本次已打):
- 新增 include/trace/hooks/fs.h:
  DECLARE_RESTRICTED_HOOK(android_vh_check_file_open, TP_PROTO(const struct file *file), TP_ARGS(file), 1)
- drivers/android/vendor_hooks.c: include trace/hooks/fs.h +
  EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_check_file_open)
- fs/open.c do_dentry_open(): 在 f->f_op = fops_get(...) 之后、
  security_file_open() 之前调用(满足 fops bridge 的"任何 file callback 之前"要求)

验证:
- 新 System.map: __tracepoint_android_vh_check_file_open @ ffffffc012009f90
- asm-offsets 关键项不变(TSK_STACK 48 / TSK_ACTIVE_MM 1256 / TSK_STACK_CANARY 1432)
- tools/verify-vendor-modules.py -> 160 模块 / 1753 符号 / 0 未解析 PASS
- boot.img: kernel md5 97b0afc5(=新 Image), ramdisk md5 e8e25cde(=原厂逐字节)

待办:上机抓 panic 日志(若仍崩)。模块此前根本进不了 init 尾段,所以之前的 crash
要么在 init 前段/清理路径,要么实际跑的包与工作区不一致。