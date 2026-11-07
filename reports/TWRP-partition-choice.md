# TWRP 该刷到哪个分区 —— `vendor_boot`（已修正）

> **修正说明**：本文档第一版结论是「刷 `boot`，vendor_boot 不行」，**那是错的**。
> 我当时没展开 `vendor_boot` 里的 ramdisk 就看体积，漏掉了决定性证据：
> 原厂 `vendor_boot` 的 vendor ramdisk 里**就装着 recovery**。

**设备**：iFLYTEK ums9620_se50（UMS9620 / 2H10，A/B，Android 13）

---

## 1. 结论

**刷 `vendor_boot`。`boot` 一个字节都不用动 —— 安卓照常启动，recovery 模式进 TWRP。**

```bash
fastboot flash vendor_boot artifacts/vendor_boot-twrp-recovery-ums9620_se50-20260926.img
```

---

## 2. 决定性证据：recovery 实体在 vendor_boot 里

把原厂 `vendor_boot_a.img` 拆开，它的 vendor ramdisk（662 项）中含有：

```
system/bin/recovery                       ← recovery 主程序
system/etc/recovery.fstab
system/etc/recovery.tmpfsdata.fstab
system/etc/init/servicemanager.recovery.rc
system/lib64/librecovery_ui.so
system/lib64/librecovery_ui_ext.so
lib/modules/modules.load.recovery
first_stage_ramdisk/fstab.ums9620_se50    ← 正常启动第一阶段用
lib/modules/*.ko  × 152                   ← vendor 内核模块
```

而原厂 `boot` 的 ramdisk **只有 11 项**，且**没有** `system/bin/recovery`：

```
debug_ramdisk  dev  init  metadata  mnt  proc
second_stage_resources  sys  system/etc/ramdisk/build.prop
```

=> 这台机器走的是 AOSP 的 **`BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT`** 方案：

| 分区 | 角色 |
|---|---|
| `boot` | kernel + 极简 generic ramdisk（`/init`）—— 正常启动真正跑的那个 |
| `vendor_boot` | vendor ramdisk = **recovery 实体**（`system/bin/recovery` 等）+ 152 个 vendor 模块 + first-stage fstab + DTB |

启动时 bootloader 把两者拼成一个 initramfs；`boot` 的 generic `/init` 跑起来，
**正常模式**走 Android，**recovery 模式**（`/misc` 里的 BCB）则去 exec `/system/bin/recovery` ——
那个二进制来自 `vendor_boot`。

**所以换掉 vendor ramdisk 里的 recovery 资源 = 换 recovery，而完全不碰正常启动链。**

（TWRP 设备树里那句 `BOARD_USES_RECOVERY_AS_BOOT := true` 是生成器的错误判断，
本机既没有独立 recovery 分区，recovery 也不在 boot 里。）

---

## 3. 不能简单替换，必须叠加

原厂 vendor ramdisk 里那 152 个 `lib/modules/*.ko` 和 `first_stage_ramdisk/fstab.*`
是**正常启动第一阶段要用的**。直接拿 TWRP 的 ramdisk 替换整个 vendor ramdisk，
会把这些全丢掉，安卓直接起不来。

TWRP ramdisk 侧的情况正好互补：

| | 原厂 vendor ramdisk | TWRP ramdisk |
|---|---|---|
| 条目数 | 662 | 3662 |
| `lib/modules/*.ko` | **152** | **0** |
| `first_stage_ramdisk/fstab.*` | 有 | 有（去掉了 avb） |
| `system/bin/recovery` | 有 | 有 |
| `res/`（TWRP 主题）、`sbin/` | 无 | 有 |

=> **合并**：以原厂 vendor ramdisk 为底，把 TWRP 的资源叠上去。

第一阶段的 fstab 刻意**保留原厂的**（带 `avb=vbmeta_*` 的那 7 行），
这样正常启动行为与改动前**逐字节一致**；TWRP 自己用 `system/etc/recovery.fstab`。

> TWRP 那版 `first_stage_ramdisk/fstab` 只是把 `avb=` / `avb_keys=` 删掉（共 10 行，条目齐全）。
> 如果你之后刷了 TWRP 重建的 vbmeta（`--set_hashtree_disabled_flag`），
> 那第一阶段就得换成 TWRP 那版（否则 `avb=vbmeta_system` 会因为 hashtree 被禁而挂不上）。
> 需要的话说一声，我 10 秒给你重出一个。

---

## 4. 体积：这次非常宽裕

`vendor_boot` 分区 **100 MiB**（spd_dump 整分区 dump 实测 104,857,600）。

| | 大小 |
|---|---|
| 原厂 vendor ramdisk | 29,094,608（lz4） |
| **合并后 vendor ramdisk** | **36,870,225（gzip）** |
| DTB | 161,140 |
| 成品镜像 | **37,044,224（35.3 MiB）**，补零到 100 MiB |

对比一下上一版往 **64 MiB 的 `boot`** 里硬塞 62.6 MiB 的窘境（只剩 4.3 MiB 余量）——
走 `vendor_boot` 才是这台机器本来的设计。

> 内核 initramfs 解压器只开了 `CONFIG_RD_GZIP` / `CONFIG_RD_LZ4`，**没有 `CONFIG_RD_XZ`**。
> 这里用 gzip（lz4 也行，只是大一点），xz 刷了会黑屏。

---

## 5. 已打好的包

```
artifacts/vendor_boot-twrp-recovery-ums9620_se50-20260926.img
  104,857,600 B   md5 eabb5055e871c27cb90aa116b3380fd8
```

头字段与原厂 vendor_boot 逐项一致：

| 项 | 值 |
|---|---|
| header | v4 |
| page size | 4096 |
| kernel load addr | 0x00008000 |
| ramdisk load addr | 0x05400000 |
| tags load addr | 0x00000100 |
| dtb addr / size | 0x1f00000 / 161,140 |
| vendor cmdline | `console=ttyS1,115200n8 buildvariant=user` |
| vendor ramdisk | 36,870,225（gzip），3856 项 |

双向复核：`unpack_bootimg` 与 `magiskboot` 都能正确解析（`VENDOR_BOOT_HDR`），
解包回读确认 `system/bin/recovery`（TWRP 2,438,680）、152 个 vendor `.ko`、
原厂 first-stage fstab（7 行 avb）、TWRP `res/` 109 个文件都在。

---

## 6. 刷机与回滚

```bash
# 进 TWRP（recovery 模式：音量键组合，或 adb reboot recovery）
fastboot flash vendor_boot artifacts/vendor_boot-twrp-recovery-ums9620_se50-20260926.img

# 回原厂 recovery（boot 一直没动，安卓全程可用）
fastboot flash vendor_boot stock/vendor_boot_a.img
```

**安卓完全不受影响** —— 这个包不碰 `boot`，也就是不碰正常启动链。
刷坏了也只需要把原厂 `vendor_boot_a.img` 刷回去。

### 另一个包（不推荐，留作备用）

`artifacts/twrp-recovery-tca-kernel-ums9620_se50-20260926.img`（md5 `6a639b2f…`）
是「recovery-as-boot」版：把 TWRP 塞进 `boot`，**会顶掉安卓的正常启动 ramdisk**，
刷了就进不了安卓（要刷回 `boot-...-droidspaces...img`）。
只有在 `vendor_boot` 这条路线走不通时才考虑它。

---

## 7. 顺带记录

* 原厂 `vendor_boot` 的 vendor ramdisk 是 **lz4** 压缩，我们重打成了 gzip（内核两种都认）。
* `se50_t30l/initboot/init_boot_a.img`（8 MiB）内容全 0，这台机器**不用 init_boot**。
* `stock/dtb_a.img` 也是空的，设备树在 `vendor_boot` 里。
* 原厂 `boot.img` 的 cmdline 为空；`console=ttyS1,115200n8 buildvariant=user` 来自 `vendor_boot`。
