# 刷机包 — TCA Cycle Kernel (ReSukiSU) · iFLYTEK UMS9620

## 产物

| 文件 | 用途 | md5 |
|---|---|---|
| `artifacts/tca-cycle-kernel-5.4.210-tca-cycle-touch2-20260926.zip` | AnyKernel3 卡刷包（只换内核，保留设备现成 ramdisk） | `9d96949614a912e0472addae696a49de` |
| `artifacts/boot-tca-cycle-5.4.210-tca-cycle-touch2-20260926.img` | 成品 boot.img（原厂 ramdisk） | `dedacf53956a26c4a6037e4001b95a5b` |
| `artifacts/boot-tca-cycle-5.4.210-tca-cycle-magisk-20260926.img` | 成品 boot.img（**Magisk ramdisk**） | `2717a332764c441fe32f7e8189a2e9ec` |

三者都只把 `boot` 分区里的 **kernel 换成 TCA Cycle Kernel**；区别只在 ramdisk：

- `touch2` 的 boot.img / AK3 zip：用**原厂 ramdisk**（无 Magisk）。
- `magisk` 的 boot.img：用 **Magisk 打过补丁的 ramdisk**，内核仍是我们的。

## 关于 Magisk ramdisk 版

来源：`se50_t30l/dsu/boot_a.img`。它的内核 md5 与原厂 `stock/boot/unpack/kernel` **完全相同**
（`fe1fac8aa39fc19304ab5aa623a64da8`），说明它就是「原厂内核 + Magisk ramdisk」，
只是后来被人把内核换掉过。我们拿它当底座、只替换内核，就得到「我们的内核 + Magisk ramdisk」。

离线验证：

```
HEADER_VER [4]  KERNEL_SZ [31918592]  RAMDISK_SZ [1301998]  PAGESIZE [4096]
RAMDISK_FMT [lz4_legacy]  VBMETA
kernel banner : Project TCA cycle kernel Linux version 5.4.210-tca-cycle #22 ...
magiskboot cpio ramdisk.cpio test -> 1   (1 = Magisk 已打补丁)
ramdisk 条目 : .backup debug_ramdisk dev init metadata mnt overlay.d proc
               second_stage_resources sys system
```

刷法（与普通 boot.img 相同，TWRP 里选 Install Image → Boot，或 fastboot flash boot_a）。

> **Magisk 与 ReSukiSU 并存**：本内核已内建 ReSukiSU(KernelSU)，Magisk 的 `magiskinit`
> 又在 ramdisk 里，两套 root 方案会同时在。二者能共存，但 Zygisk/overlay 之类可能互抢；
> 若只想要其中一个，用 `touch2` 那版（无 Magisk）。
>
> **AK3 不再单独带 Magisk**：按需求，`touch2` 的 AK3 zip 保持「只换内核」，
> 它刷在**已装 Magisk** 的机器上会**自动保留** Magisk（AK3 只替换 kernel、ramdisk 用设备上现成的）。
> 想从纯净原厂状态装上 Magisk，就先刷 `magisk` 那版 boot.img。

## 包内容保证（已离线验证）

- boot.img 为 **Android boot header v4**，OS 13.0.0，page 4096，ramdisk lz4_legacy。
- 新 kernel = 31,521,280 B；`uname -r` = `5.4.210-tca-cycle`；banner `Project TCA cycle kernel Linux version 5.4.210-tca-cycle ...`；含 ReSukiSU。
- **ramdisk 解压后 md5 与原厂逐字节一致**（`e8e25cdef60aa27285a37fdeb5198431`）——只是重新压缩，内容未改。
- **DTB / DTBO 完全没碰**：本机 boot.img 内无 DTB，base DTB 在 `vendor_boot` 分区、板级 overlay 在 `dtbo` 分区，都不在刷写范围内。
- 原厂 160 个 vendor `.ko` 继续用（`tools/verify-vendor-modules.py` → 未解析符号 0，PASS）。

## 方式一：AnyKernel3 卡刷包（推荐）

1. 把 zip 拷进设备。
2. TWRP → Install → 选 zip → 刷入；或用内核管理器 App（EX Kernel Manager / Franco 等）刷 zip。
3. 重启。

`anykernel.sh` 行为：

- `do.devicecheck=1`，只接受 `ro.product.device` / `ro.build.product` / `ro.product.vendor.device` 为 `ums9620_se50`（或 `ums9620_se50_s30pro` / `s30pro`）。
- `BLOCK=boot` + `IS_SLOT_DEVICE=1`：自动读 `ro.boot.slot_suffix`，只刷当前槽。
- `do.modules=0`：不装任何模块，原厂 `.ko` 一个不动。
- `split_boot` + `flash_boot`：不解包 ramdisk，换 kernel 后直接重封。

## 方式二：fastboot 直刷 boot.img

```bash
fastboot getvar current-slot          # 确认当前槽，假设为 a
fastboot flash boot_a boot-tca-cycle-5.4.210-tca-cycle-20260925.img
fastboot reboot
```

若提示 unknown partition，改试 `fastboot flash boot ...`。

## 刷完自检

```bash
uname -r                          # 5.4.210-tca-cycle
cat /proc/version                 # Project TCA cycle kernel Linux version 5.4.210-tca-cycle ...
dmesg | grep -i kernelsu          # KernelSU 初始化日志
lsmod | wc -l                     # 应与原厂一致
su -c id                          # uid=0（装 ReSukiSU manager APK 并授权后）
```

## 回滚

重刷原厂整分区镜像 `stock/boot/boot.img`（64 MB）：

```bash
fastboot flash boot_a stock/boot/boot.img
```

## 注意

- AVB 已移除，重封后 boot 内的 AVB 签名不再有效属正常；不要再打开 AVB 校验，否则无法启动。
- 本包**不含触摸驱动**：原厂树缺少 `himax_ic_HX83102j.c`，vmlinux 里只有导出 `himax_resume_by_lcd` / `himax_int_enable` 的 shim。显示正常，触摸是否可用取决于该驱动（详见 `s5-ship/README.md` 的说明）。
- 不要混刷 `s5-ship/` 里那套重编模块：那是旧方案（替换原厂 `.ko`）。当前内核是 Plan A（全用原厂模块），混刷会破坏 ABI。
- 不要把 `boot` 换成 `init_boot` / `vendor_boot`：kernel 在 `boot`，DTB 在 `vendor_boot`。

## 重建方式

```bash
cd /home/misakapardola/leaking-iflytek/tcakernel
./build.sh                                    # 产出 build/se50-clang-out/arch/arm64/boot/Image
cp build/se50-clang-out/arch/arm64/boot/Image anykernel3/Image
cd anykernel3 && zip -r9 ../artifacts/tca-cycle-kernel-$(date +%Y%m%d).zip .
```
