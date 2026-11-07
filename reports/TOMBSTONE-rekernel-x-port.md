# 墓碑模式（ReKernel-X）内核移植报告

## 1. 这是什么

"墓碑模式"在这台机器上对应 **Sakion Team 的 Re-Kernel / ReKernel-X** 体系：
内核侧监听「被冻结进程收到 Binder 调用 / 关键信号 / 入站网络包」，
经 **Generic Netlink (family `rekernel_x2`)** 上报用户态，让冻结器按需解冻。

| 组件 | 形态 | 作用 |
|---|---|---|
| Re-Kernel | 内核补丁 | 往内核加 hook（含 ACK 没有的自定义 hook）|
| **ReKernel-X** | LKM | 监听冻结进程事件并上报（我们采用这个）|

---

## 2. 决定性发现：4 个 hook 全是 ACK 标准 hook

上一次评估说 `android_vh_do_send_sig_info` "ACK 没有、需自加" —— **这是错的**。
逐字核对 AOSP `kernel/common android12-5.10` 后确认：

| hook | ACK 5.10 出处 |
|---|---|
| `android_vh_binder_alloc_new_buf_locked` | `include/trace/hooks/binder.h` ✅ |
| `android_vh_binder_reply` | `include/trace/hooks/binder.h` ✅ |
| `android_vh_binder_trans` | `include/trace/hooks/binder.h` ✅ |
| `android_vh_do_send_sig_info` | `include/trace/hooks/signal.h` ✅ |

所以全部从 ACK 5.10 原样回移，没有自造 hook。

---

## 3. 已完成的改动

### 3.1 新增 / 修改的头文件

```
include/trace/hooks/binder.h      +3 DECLARE_HOOK + 前向声明
include/trace/hooks/signal.h      新建，+1 DECLARE_HOOK（照抄 ACK 5.10）
drivers/android/vendor_hooks.c    #include <trace/hooks/signal.h>
                                  +4 EXPORT_TRACEPOINT_SYMBOL_GPL
```

### 3.2 调用点（与 ACK 5.10 位置一致）

| 文件 | 位置 | 调用 |
|---|---|---|
| `drivers/android/binder.c` | reply 分支 `binder_inner_proc_unlock()` 后 | `trace_android_vh_binder_reply` |
| `drivers/android/binder.c` | `e->to_node = ...` 后 | `trace_android_vh_binder_trans` |
| `drivers/android/binder_alloc.c` | `is_async` 空间检查前 | `trace_android_vh_binder_alloc_new_buf_locked` |
| `kernel/signal.c` | `do_send_sig_info()` | `trace_android_vh_do_send_sig_info(sig, current, p)` |

> `binder_alloc.c` 我们的 vendor 树把 `size = max(size, sizeof(void *))` 放在
> `is_async` 检查**之后**（ACK 5.10 在之前）。为**不改动既有行为**，hook 放在
> `is_async` 检查之前、用未 padding 的 size（阈值语义不受影响）。

### 3.3 ReKernel-X LKM（`drivers/rekernel_x/`，10 个 .o）

内建（`obj-$(CONFIG_REKERNEL_X)`）而非外部 LKM —— 我们 vermagic 是
`"modversions "`（为复用原厂模块刻意改的），外挂 LKM 反而麻烦。

**5.4 移植踩到的 4 个坑，均已修复：**

1. **版本分支缺口**：`rkx_binder.c` 的 `alloc_new_buf_locked` 分支只写到 `>=5.10`，
   5.4 会剩一个孤立 `{ }`。→ 改为 `#else`（覆盖 5.4–5.14，签名同 5.10）。
2. **`DEFINE_SHOW_ATTRIBUTE` 未定义**：5.4 里它在 `<linux/seq_file.h>`，
   而 vendor `binder_internal.h` 直接用它。→ 在 `rkx_binder.c` / `rkx_binder_alloc.h`
   里于 `binder_internal.h` **之前**补 `#include <linux/seq_file.h>` + `<linux/debugfs.h>`。
3. **`str__signal__trace_system_name` 重定义**：`kernel/signal.c` 定义了
   `CREATE_TRACE_POINTS` 并 include `trace/events/signal.h`；此时再 include
   `trace/hooks/signal.h`（TRACE_SYSTEM 同为 `signal`）会重复生成该符号。
   → **照 ACK 的写法**：hooks 头之前加 `#undef CREATE_TRACE_POINTS`。
4. `rkx.h` 补 `linux/sched/signal.h`、`linux/sched/task.h`、`linux/cred.h`（隐式声明）。

---

## 4. 配置

```
CONFIG_REKERNEL_X=y        # Kconfig 默认 n，已在 f50_defconfig 固化
```

必需的既有基础设施（都在）：

| | |
|---|---|
| `CONFIG_ANDROID_VENDOR_HOOKS=y` | vendor hook 框架 ✅ |
| `CONFIG_ANDROID_VENDOR_OEM_DATA=y` | task_struct vendor padding ✅ |
| `CONFIG_KPROBES` / `KALLSYMS_ALL` / `TRACEPOINTS` | `kallsyms_lookup_name` 靠 kprobe 偷地址 ✅ |
| `include/trace/hooks/vendor_hooks.h` / `drivers/android/binder_internal.h` | ✅ |

---

## 5. ABI 安全性

4 个 hook 与 ReKernel-X 都**不修改任何结构体布局**：
不碰 `task_struct` / `cred` / `file` / `binder_*` 的字段增删。
验证手段仍是 `asm-offsets.h` diff：

```bash
cp build/se50-clang-out/include/generated/asm-offsets.h /tmp/asm-offsets.before.h
# 编译后
diff /tmp/asm-offsets.before.h build/se50-clang-out/include/generated/asm-offsets.h
# 期望：为空（TSK_STACK 48 / TSK_ACTIVE_MM 1256 / TSK_STACK_CANARY 1432 不变）
```

外加 `tools/verify-vendor-modules.py`：160 模块 / 0 未解析。

---

## 6. 运行时注意

* 内核侧只负责**上报事件**；真正的冻结/解冻由用户态 ReKernel-X 管理器
  （监听 `rekernel_x2` genl family）完成。**没有装管理器 = 内核侧静默无事发生**。
* `rkx_binder_trans` 会在同步 binder 路径里 `copy_from_user` 读 RPC 名；
  该调用点**未持有 binder 锁**、进程上下文，可睡眠 —— 安全。
* 卸载（`rmmod`）不适用（内建）。要关掉：`CONFIG_REKERNEL_X` 置 n 重编。

---

## 7. 状态

* [x] 4 个 ACK hook 回移（声明 + 导出 + 4 处调用点）
* [x] ReKernel-X 10 个目标全部编译通过并链入 vmlinux
* [x] `CONFIG_REKERNEL_X=y` 固化进 `f50_defconfig`
* [x] 全量 Image 构建 **BUILD OK**（32,455,168 B）
* [x] ABI diff 为空；`verify-vendor-modules.py` → 160 模块 / 1753 符号 / **0 未解析**
* [x] 4 个 `__tracepoint_android_vh_*` 都在 vmlinux 且已导出（`__ksymtab_`）
* [x] `rekernel_x2` genl family 字符串在 vmlinux 中
* [ ] 上机：ReKernel-X 管理器连通 + 冻结进程事件上报验证

### 产物（本次）

| 文件 | md5 |
|---|---|
| `artifacts/tca-cycle-kernel-...-droidspaces-bfq-bbr-tombstone-20260926.zip` | `76a5c8c1add75a334a6e371046efb125` |
| `artifacts/boot-tca-cycle-...-droidspaces-bfq-bbr-tombstone-20260926.img` | `fd6acfda97818b77afbbefd34f493a76` |
| `Image` | `7c0e41bb873f7def5ff723fad08d1882` |

git: `1f84ce71e`（未推送）

### 上机验证要点

```bash
# 1) genl family 是否注册
su -c 'cat /proc/net/netlink' | grep -i rekernel   # 或
su -c 'genl-ctrl-list 2>/dev/null | grep rekernel_x2'  # 有 netlink 工具时
# 2) dmesg 里 ReKernel-X 的日志
su -c 'dmesg | grep -iE "ReKernel|rkx|rekernel_x2"'
#    期望看到 "starting..." / "hooked!" 之类
# 3) 用 ReKernel-X 管理器（用户态）连上并冻结几个后台 App 试收通知
```

