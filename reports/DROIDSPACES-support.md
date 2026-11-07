# Droidspaces 容器支持（kABI 安全路线）

**目标**：让 TCA Cycle Kernel（5.4.210, ums9620_se50）满足 [Droidspaces](https://github.com/ravindu644/Droidspaces-OSS) 的内核要求，
**同时不破坏原厂 160 个 vendor `.ko` 的 ABI**。

来源文档：https://github.com/ravindu644/Droidspaces-OSS/blob/main/Documentation/Kernel-Configuration.md

---

## 1. 决定性发现：原厂 SE50 的完整内核配置

原厂 boot Image 里嵌了 `/proc/config.gz`（`CONFIG_IKCONFIG=y`），可直接提取：

```bash
kernel_ums9620_se50/scripts/extract-ikconfig stock/boot/unpack/kernel > stock/stock.config
# → 6394 行，已存入 stock/stock.config
```

这让我们可以**逐项对照原厂**，而不是靠猜。关键几项：

| 选项 | 原厂 SE50 | 我们（改动前） |
|---|---|---|
| `CONFIG_SYSVIPC` | **n** | n |
| `CONFIG_POSIX_MQUEUE` | **n** | n |
| `CONFIG_PID_NS` | **n** | n |
| `CONFIG_DEVTMPFS` | **n** | n |
| `CONFIG_USER_NS` | **n** | n |
| `CONFIG_ANDROID_KABI_RESERVE` | **y** | y |
| `CONFIG_MODVERSIONS` | y | n（我们为复用原厂模块刻意关掉） |
| `CONFIG_TP_RESUME_BY_LCD` | **y** | 我们树里**无此符号** |
| `CONFIG_TOUCHSCREEN_HIMAX_CHIPSET` | y | 我们用的是 `_PAD` 变体 |

> 我们与原厂的 config 差异其实很小：**仅我们有 =y 的 11 项，仅原厂有 =y 的 21 项**。
> 后者多为原厂内建的杂项驱动（`DTOF_VI5300`、`MAG_AK09973`、`LCM_*`、`HW_AUDIO_SWITCH` …），
> 以及 `TP_RESUME_BY_LCD` / `HIMAX_CHIPSET` 这两个触摸相关项。

---

## 2. 为什么 SYSVIPC / POSIX_MQUEUE 是雷

Droidspaces 文档明确指出：

> Enabling standard Droidspaces features like `CONFIG_SYSVIPC` or `CONFIG_IPC_NS` would normally
> shift memory offsets in the core `task_struct`, causing pre-compiled vendor modules to crash.

具体影响两处结构体：

| 结构体 | 文件 | 字段 |
|---|---|---|
| `struct task_struct` | `include/linux/sched.h` | `struct sysv_sem sysvsem;` / `struct sysv_shm sysvshm;`（共 24 B） |
| `struct user_struct` | `include/linux/sched/user.h` | `unsigned long mq_bytes;`（8 B） |

原厂这两项都是 `n`，所以字段**不在**结构体里。直接开 `=y` 会让结构体长大 24 B / 8 B，
把后面所有字段偏移顶掉 → 原厂模块内联的偏移全错位。

**正解**：利用 Android 的 ABI padding。原厂 `CONFIG_ANDROID_KABI_RESERVE=y`，
`task_struct` 末尾有 `ANDROID_KABI_RESERVE(1..8)`（每个占一个 `u64`，共 64 B），
`user_struct` 末尾有 `RESERVE(1..2)`。把新字段搬进这些**本来就占空间**的坑里，
结构体尺寸和后续偏移**一个字节都不变**。

---

## 3. 改动

### 3.1 `include/linux/sched.h`

```diff
 #ifdef CONFIG_SYSVIPC
-	struct sysv_sem			sysvsem;
-	struct sysv_shm			sysvshm;
+	/* Droidspaces sysvipc kABI fix: 字段搬入下方 ANDROID_KABI_USE 预留位 */
+	// struct sysv_sem			sysvsem;
+	// struct sysv_shm			sysvshm;
 #endif

@@ ... @@ struct task_struct {
 	ANDROID_KABI_RESERVE(5);
+
+#ifdef CONFIG_SYSVIPC
+	ANDROID_KABI_USE(6, struct sysv_sem sysvsem);
+	_ANDROID_KABI_REPLACE(ANDROID_KABI_RESERVE(7); ANDROID_KABI_RESERVE(8), struct sysv_shm sysvshm);
+#else
 	ANDROID_KABI_RESERVE(6);
 	ANDROID_KABI_RESERVE(7);
 	ANDROID_KABI_RESERVE(8);
+#endif
```

(`sysv_sem` = 8 B 占坑 6；`sysv_shm` = 16 B 占坑 7+8。)

### 3.2 `include/linux/sched/user.h`

```diff
 #ifdef CONFIG_POSIX_MQUEUE
 	/* protected by mq_lock	*/
-	unsigned long mq_bytes;	/* How many bytes can be allocated to mqueue? */
+	// unsigned long mq_bytes;	/* 搬入 ANDROID_KABI_USE(1) */
 #endif
@@ ... @@ struct user_struct {
+#if defined(CONFIG_POSIX_MQUEUE)
+	ANDROID_KABI_USE(1, unsigned long mq_bytes);
+	ANDROID_KABI_RESERVE(2);
+#else
 	ANDROID_KABI_RESERVE(1);
 	ANDROID_KABI_RESERVE(2);
+#endif
```

补丁存档：`patches/droidspaces/001-sysvipc-kabi-padding.patch`、`patches/droidspaces/002-posix-mqueue-kabi-padding.patch`

上游参考（我们用脚本等价实现，未直接 git-am）：
- `001.GKI-below-6.12-fix_sysvipc_kabi_6_7_8.patch`（另有 `1_2_3` / `3_4_5` 备选，占坑不同）
- `002.5.10_or_lower_use_android_abi_padding_for_posix_mqueue.patch`

---

## 4. 启用的配置

```
CONFIG_SYSVIPC=y            CONFIG_POSIX_MQUEUE=y       CONFIG_IPC_NS=y
CONFIG_PID_NS=y             CONFIG_DEVTMPFS=y           CONFIG_USER_NS=y
CONFIG_CGROUP_DEVICE=y      CONFIG_CGROUP_PIDS=y        CONFIG_CGROUP_NET_PRIO=y
CONFIG_BRIDGE_NETFILTER=y   CONFIG_NF_TABLES=y          CONFIG_FW_LOADER_COMPRESS=y
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y   CONFIG_NETFILTER_XT_TARGET_REJECT=y
CONFIG_NETFILTER_XT_TARGET_LOG=y       CONFIG_NETFILTER_XT_MATCH_RECENT=y
CONFIG_IP_SET=y   CONFIG_IP_SET_HASH_IP=y   CONFIG_IP_SET_HASH_NET=y   CONFIG_NETFILTER_XT_SET=y
CONFIG_TMPFS_POSIX_ACL=y    CONFIG_TMPFS_XATTR=y
```

**ABI 影响面复核**（逐个查 `#ifdef CONFIG_X` 是否门控 `task_struct` / `user_struct` / `cred` / `nsproxy` / `file` / `inode` 的字段）：

* 只有 `SYSVIPC` 与 `POSIX_MQUEUE` 动结构体 → 已由上述 padding 抵消。
* `USER_NS`：`struct cred` 的 `user_ns` 是**无条件字段**，`#ifdef CONFIG_USER_NS` 只门控
  `current_user_ns()` 访问器 → 安全。
* 其余（PID_NS / IPC_NS / DEVTMPFS / cgroup / netfilter / tmpfs）不动上述任何结构体。
* `DEVTMPFS` **不**同时打开 `DEVTMPFS_MOUNT`，避免改变 Android 自己的 `/dev` 挂载方式。

---

## 5. ABI 验证方法（本地可做，不依赖上机）

`arch/arm64/kernel/asm-offsets.c` 会把 `task_struct` 的关键字段偏移生成到
`include/generated/asm-offsets.h`。**改动前后 diff 这个文件即可证明 ABI 是否守住**：

```bash
cp build/se50-clang-out/include/generated/asm-offsets.h /tmp/asm-offsets.before.h
# ... 打补丁 + olddefconfig + 重新编译 ...
diff /tmp/asm-offsets.before.h build/se50-clang-out/include/generated/asm-offsets.h
```

改动前的指纹（作为基线）：

```
#define TSK_ACTIVE_MM 1256
#define TSK_STACK 48
#define TSK_STACK_CANARY 1432
```

**期望结果：diff 为空**（结构体尺寸与偏移完全不变）。

---

## 6. 待办

- [x] 提取原厂 config → `stock/stock.config`
- [x] kABI padding 改动（`sched.h` / `user.h`）
- [x] 启用 Droidspaces 所需配置 + 固化进 `f50_defconfig`
- [x] 补丁存档 `patches/droidspaces/`
- [x] 编译 → **BUILD OK**，Image 32,320,000 B
- [x] `diff asm-offsets.h` → **完全一致，ABI 守住**（`TSK_STACK 48` / `TSK_ACTIVE_MM 1256` / `TSK_STACK_CANARY 1432` 均未变）
- [x] `tools/verify-vendor-modules.py` → **160 模块 / 0 未解析 / PASS**
- [x] 打包：`artifacts/tca-cycle-kernel-5.4.210-tca-cycle-resukisu35181-lcdresume-droidspaces-20260926.zip`
- [ ] **上机**：`su -c droidspaces check` 逐项确认

### 产物

| 文件 | md5 |
|---|---|
| zip | `0225237f296f64bbd7963337526d2578` |
| boot.img | `286d1129d53a1a50ae1eb94682790390` |
| Image | `4155025bc66ccfd00524a05b15b1fee6` |

---

## 7. 上机后发现的容器相关内核 panic（已定位并修复）

**症状**：停 Droidspaces 容器时 `Internal error: Oops - BUG: 0`，
`Workqueue: netns cleanup_net`，
`__slab_free → kfree → xfrm_sysctl_fini → xfrm_net_exit → cleanup_net` →
`Kernel panic - not syncing: Fatal exception`。

**根因**：原厂 `CONFIG_XFRM_FRAGMENT=y` 的 `sprd_xfrm_hdr` / `sprd_sys_ctl_table`
是**文件级全局变量**，却被放在**每 netns** 的 `xfrm_sysctl_init/fini` 里。
后建的 netns 覆盖全局；销毁任何非最后 init 的 netns 就会 `kfree` 别人的 /
已释放的 table → `__slab_free` 的 `BUG()`。Droidspaces 频繁建/毁 netns，稳定触发。
**原厂 Android 不 churn netns，所以这个雷长期潜伏；是我们打开容器配置把它引爆的。**

**修复**：`enable_xfrm_fragment` 本就是全局开关，改为只在 `init_net` 注册一次，
`xfrm_sysctl_fini()` 不再触碰这两个全局。只改 `net/xfrm/xfrm_sysctl.c`，**ABI 零影响**
（`asm-offsets.h` diff 为空；160 模块 0 未解析）。

详细分析 + 原始 pstore：`reports/PANIC-xfrm-netns-double-free.md`、
`reports/panic-20260926-xfrm/`。
