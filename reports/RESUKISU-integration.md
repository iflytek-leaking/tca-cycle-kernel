# ReSukiSU Integration — TCA Cycle Kernel (ums9620, Linux 5.4.210)

> **已更新到 ReSukiSU `v4.2.0-rc3`（`main` @ `20756bfb`）。**
> 旧的 `v4.1.0` 记录保留在文末「附录 A」，其兼容 hack 已全部作废。

## 1. 目标

把 ReSukiSU 内建进 ZTE UMS9620 (iFLYTEK SE50 / ums9620_se50) 的 5.4.210 GKI 源码树，
只刷 boot Image；原厂 160 个 vendor `.ko` 一个不改、一个不少。

## 2. 来源

| 项 | 值 |
|---|---|
| 项目 | ReSukiSU (https://github.com/ReSukiSU/ReSukiSU) |
| 分支 / 标签 | `main` / `v4.2.0-rc3` |
| 提交 | `20756bfb` |
| KSU_VERSION | `35181` = 30000 + git rev-list --count HEAD(4481) + 700（Kbuild 自动计算） |
| KSU_VERSION_FULL | `v4.2.0-rc3-20756bfb@ReSukiSU` |
| 本地路径 | `kernel_ums9620_se50/KernelSU/`（**完整** clone；Kbuild 需要真实 commit count，不能 shallow） |
| 接入方式 | 符号链接 `drivers/kernelsu -> ../KernelSU/kernel` |
| 指南 | https://resukisu.org/guide/manual-integrate.html |

## 3. 为什么必须升级 v4.1.0 → v4.2.0

v4.1.0 是旧架构：`kernel/` 下平铺文件，hook 只能靠 **kprobe / tracepoint**。
v4.2.0 重构成 `compat / core / feature / hook / infra / policy / runtime / selinux / sulog / supercall` 子目录，
hook 方式改成三选一：

| 选项 | 适用 |
|---|---|
| `KSU_TRACEPOINT_HOOK` | 仅 GKI 2.0（5.10+）；Kbuild 对 GKI 1.0 直接 `$(error)` |
| `KSU_MANUAL_HOOK` | 3.4+，**本内核采用** |
| `KSU_SUSFS` | 需自行整合 simonpunk/susfs4ksu |

新 Kbuild 明确把 5.4 识别为 **`KERNEL_TYPE := GKI 1.0`**，Manual Hook 是官方支持路径。

### 3.1 这如何修掉「未检测到内核驱动」

v4.1.0 的管理器发现链路**完全依赖 kprobe 注册成功**：
`ksu_supercalls_init()` 在 `__arm64_sys_reboot` 上注册 kprobe，管理器的握手

```
syscall(SYS_reboot, 0xDEADBEEF, 0xCAFEBABE, 0, &fd)
```

才可能被截获并下发 `[ksu_driver]` fd。**只要该 kprobe 注册失败，握手永远不会发生**，
管理器拿不到 fd → 判定「内核上未检测到 KernelSU 驱动程序」。而 kprobe 注册失败是静默的。

v4.2.0 的 Manual Hook 把调用点**直接编译进源码**（第 4 节），不存在「注册失败」这一失效模式。

> **澄清**：v4.1.0 的 `is_manager_apk()` 调用 `check_v2_signature(path, false, NULL)`，
> 走的是 `check_multi_manager == false` 分支 —— 源码注释即 **"Common manager check: any valid signature will do"**
> （`v4.1.0 kernel/apk_sign.c:373-376`）。所以 v4.1.0 时期**签名不是**拦路虎。
> 签名严格化是 v4.2.0 才引入的新行为（见第 6 节）。

## 4. 本树需要打的 Manual Hook（4 个文件 / 7 处）

| 文件 | 插入内容 |
|---|---|
| `fs/stat.c` | `ksu_handle_stat()` → `newfstatat` + `fstatat64`；`ksu_handle_newfstat_ret()` → `newfstat`；`ksu_handle_fstat64_ret()` → `fstat64`；文件级 extern 声明 |
| `fs/exec.c` | `ksu_handle_execveat()` + `ksu_handle_post_execveat()` 包住 `do_execveat_common()` |
| `fs/open.c` | `ksu_handle_faccessat()` → `SYSCALL_DEFINE3(faccessat)` |
| `kernel/reboot.c` | `ksu_handle_sys_reboot()` → `SYSCALL_DEFINE4(reboot)`（**握手关键**） |

setresuid / initrc(读) / input 三项**不需要**手打：`CONFIG_KSU_MANUAL_HOOK_AUTO_SETUID_HOOK`、
`_AUTO_INITRC_HOOK`、`_AUTO_INPUT_HOOK` 默认 `y`，分别走 LSM / LSM / input_handler。

构建期 `kernel/tools/manual_hook_check.mk` 会 grep 校验上述全部符号，缺一个直接 `$(error)` 编译失败（本次全绿）。

补丁存档：`patches/resukisu-manual-hook-5.4.patch`

## 5. 配置

```diff
# arch/arm64/configs/f50_defconfig
-CONFIG_KSU_MANUAL_SU=y          # v4.1.0 的旧选项名，已废弃
+CONFIG_KSU=y
+# CONFIG_KSU_TRACEPOINT_HOOK is not set
+CONFIG_KSU_MANUAL_HOOK=y
+CONFIG_KSU_MULTI_MANAGER_SUPPORT=y
+CONFIG_KSU_MANUAL_HOOK_AUTO_SETUID_HOOK=y
+CONFIG_KSU_MANUAL_HOOK_AUTO_INITRC_HOOK=y
+CONFIG_KSU_MANUAL_HOOK_AUTO_INPUT_HOOK=y
```

## 6. 管理器 APK 签名要求（重要）

v4.2.0 的 `manager/apk_sign.c` 改为**严格比对证书长度 + SHA-256**，不再「任意 V2 签名都放行」。
`CONFIG_KSU_MULTI_MANAGER_SUPPORT=y` 时接受下列任一：

| 来源 | 证书长度 | SHA-256（前缀） |
|---|---|---|
| **官方 ReSukiSU** | `0x377` | `d3469712b6214462764a1d8d3e5cbe1d6819a0b629791b9f4101867821f1df64` |
| tiann/KernelSU | `0x33b` | `c371061b19d8c7d7...` |
| 5ec1cff/KernelSU | 384 | `7e0c6d7278a3bb8e...` |
| rsuntk/KernelSU | `0x396` | `f415f4ed9435427e...` |
| SukiSU-Ultra | `0x35c` | `947ae944f3de4ed4...` |
| KOWX712/KernelSU | `0x375` | `484fcba6e6c43b1f...` |

👉 **请安装官方 ReSukiSU 管理器 APK**（GitHub Release 页的正式版）。
自编译或重签名的 manager APK 不在表内，`is_manager()` 会恒为 `false`，
管理器随即显示「内核上未检测到 KernelSU 驱动程序」。

管理器侧另有版本下限 `Natives.MINIMAL_SUPPORTED_KERNEL = 35002`；本内核上报 `35181`，满足。

## 7. 构建结果

```
-- ReSukiSU version code: 35181
-- ReSukiSU version name: v4.2.0-rc3-20756bfb@ReSukiSU
-- Supported Unofficial Manager: MKSU, RKSU, KOWSU, SukiSU-Ultra, ReSukiSU
-- KERNEL_TYPE: GKI 1.0
-- ReSukiSU: using Manual Hook
-- ReSukiSU/manual_hook: ksu_handle_execveat found
-- ReSukiSU/manual_hook: ksu_handle_faccessat found
-- ReSukiSU/manual_hook: ksu_handle_stat found
-- ReSukiSU/manual_hook: ksu_handle_newfstat_ret found
-- ReSukiSU/manual_hook: ksu_handle_fstat64_ret found
-- ReSukiSU/manual_hook: ksu_handle_sys_reboot found
...
======================= BUILD OK ===========================
Image : 31906304   md5 8b72ff58ed3809e11d5e3b37d9c997c6
text=18738711  data=13034083  bss=2905248  dec=34678042
```

`include/generated/utsrelease.h` → `UTS_RELEASE "5.4.210-tca-cycle"`
banner → `Project TCA cycle kernel Linux version 5.4.210-tca-cycle ...`

### 7.1 相比 v4.1.0：9 处 hack → 0 处

v4.1.0 时代为让 5.4 编过而打了 9 处补丁（`patches/resukisu-5.4-compat.patch`，现已归档作废）。
v4.2.0 的 `compat/kernel_compat.h` + `tools/kernel_compat.mk` 已**原生自动探测**并处理：

`TWA_RESUME` / `put_seccomp_filter` / `strncpy_from_user_nofault` / `<linux/pgtable.h>` /
`selinux_state` / `filename_trans_key` / `handle_inode_event` / `current_sid` / `kernel_read` /
`anon_inode_getfd_secure` / `security_inode_init_security_anon` / `iterate_dir` / `static_key` …

本次编译的 compat 探测日志全部命中，**无需任何手工 5.4 补丁**。

`fs/namespace.c` 的 `path_umount()` 回移仍保留：v4.2.0 自带 `__weak path_umount()` 兜底，
但本树已回移的强符号版本会覆盖它，行为更贴近原生。

## 8. 原厂模块加载验证（PASS）

`tools/verify-vendor-modules.py` 对 `stock/vendor_rd/lib/modules` 的 160 个原厂 `.ko` 逐个检查 undefined 符号：

```
vendor 模块总数 : 160
modules.load    : 61
被依赖符号总数  : 1753   由 vendor 模块提供: 290
未解析符号      : 0
RESULT: PASS
```

**不要用 `CLEAN=mrproper`**：`build.sh` 只在 `.config` 不存在时才套 `f50_defconfig`，
而当前 `.config` 有若干刻意设置（`CONFIG_MODULE_SIG=n` 等）与 defconfig 不同源。
`CLEAN=1`（`make clean`）会保留 `.config`，安全。

## 9. 产物

| 文件 | 大小 | md5 |
|---|---|---|
| `artifacts/tca-cycle-kernel-5.4.210-tca-cycle-resukisu35181-20260926.zip` | 18,873,436 | `5c415e13c1649d182c84c65af773b608` |
| `artifacts/boot-tca-cycle-5.4.210-tca-cycle-resukisu35181-20260926.img` | 67,108,864 | `f4e990d7f9761ca27356f79565119d50` |

`boot.img` 由**原厂 `stock/boot/boot.img`** 用 x86_64 `magiskboot` 解包、仅替换 kernel 后重打包：
header v4 / pagesize 4096 / ramdisk lz4_legacy 1,472,331 B（原厂未动）/ os 13.0.0+2023-03 / signature size 0。
解包回读 kernel 大小 31,906,304、md5 与新 Image 一致。

## 10. 刷机后验证

```bash
uname -r                          # 期望 5.4.210-tca-cycle
cat /proc/version                 # 期望 "Project TCA cycle kernel Linux version ..."
lsmod | wc -l                     # vendor 模块数量应与原厂一致
dmesg | grep -iE "kernelsu|ReSukiSU"   # 期望 "Initialized with driver version: 35181 ... Built-in"
```

装上**官方 ReSukiSU 管理器 APK** 后：

```bash
su -c id                          # uid=0
```

若仍显示未检测到驱动，请抓：
```bash
dmesg | grep -iE "kernelsu|ksu|sha256|manager|install ksu fd"
```

---

## 附录 A：v4.1.0 集成（已废弃）

**来源**：`v4.1.0` / `0d27e685` — "Bump API version to 4.1.0"。

**接入**：`CONFIG_KSU=y` + `CONFIG_KSU_MANUAL_SU=y`；`CONFIG_KSU_MANUAL_HOOK` 关闭，
走 **kprobe + syscall tracepoint**（依赖 `CONFIG_KPROBES` / `CONFIG_FTRACE` / `CONFIG_KALLSYMS_ALL`）。

**当时为 5.4 打的 9 处兼容补丁**（`patches/resukisu-5.4-compat.patch`，已归档到 `patches/obsolete-v4.1.0/`）：

| 文件 | 问题 | 处理 |
|---|---|---|
| `kernel/Kbuild` | 版本探测写死 fork 名 → `KSU_VERSION` 为空 → 语法错 | 兜底 `KSU_VERSION := 41000` |
| `kernel/Kbuild` | 5.8 才有 `TWA_RESUME` | `-DTWA_RESUME=true` |
| `kernel/Kbuild` | `put_task_struct` 隐式声明 | `-include <linux/sched/task.h>` |
| `kernel/Kbuild` | 5.4 无 `put_seccomp_filter()` | `-Dput_seccomp_filter=seccomp_filter_release` |
| `kernel/Kbuild` | 5.8 才改名 `strncpy_from_user_nofault` | `-D...=strncpy_from_unsafe_user` |
| `sucompat.c`, `util.c` | `<linux/pgtable.h>` 5.8 才有 | <5.8 用 `<asm/pgtable.h>` |
| `manual_su.c` | `get_random_bytes` 隐式声明 | 补 `<linux/random.h>` |
| `pkg_observer.c` | 5.9 才拆出 `.handle_inode_event` | 回退 `.handle_event` |
| `selinux/rules.c` | `selinux_state.policy` 是 6.4+ | 用本树 `selinux_state.ss->policydb` |
| `selinux/sepolicy.c` | `filename_trans_key` 是 5.9+ | `add_filename_trans()` 返回 false 告警 |
| `fs/namespace.c` | `path_umount()` 5.9 才有 | 回移 `can_umount()` + `path_umount()` |

**v4.1.0 构建结果**：`Image 31,918,592 B`，`KSU_VERSION 41000 / v4.1.0-ReSukiSU`。

**已知取舍**：`type_transition` 规则注入在 5.4 不可用（`add_filename_trans` stub），
仅影响 manager 下发 `file` 类型转换规则的少数场景，`apply_kernelsu_rules()` 默认规则集不受影响。
