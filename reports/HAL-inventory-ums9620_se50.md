# ums9620_se50（iFLYTEK T30 Lite / s30pro）原厂 HAL 层解包清单

> 面向 **LineageOS 20（Android 13）移植** 的 HAL 层摸底文档。
> 数据来源：`spd_dump` 整分区备份 `spd_dump_it_main_238_…/260606_193212/`（`super` 10 GB，完整；另有截断的 4.35 GB 旧 dump，勿用）。
> 解包产物：`out/lp/{system,vendor,odm,system_ext,product,vendor_dlkm}_rd/`（约 10 GB，可复现脚本 `tools/extract-stock-hal.sh`）。
> 设备身份：`ro.board.platform=ums9620`、`ro.soc.model=T770`、A/B（虚拟 A/B，单物理槽）、A13 / SDK 33、GKI 5.4.210。

---

## 0. 结论速览

1. **设备侧共 67 个 VINTF HAL 条目**（43 AIDL + 24 HIDL），**全部在 `vendor` / `odm` 两个分区**；`system` 侧只有 2 个（`hidl.allocator`、`android.system.suspend`）。
2. **以标准 Treble 接口为主**：camera.provider 2.4、audio 7.1、composer 2.4、wifi 1.6、sensors AIDL、keymint 2.0、gatekeeper 1.0、health AIDL、power AIDL 3……展锐私有 HAL 都在 `vendor.sprd.*` / `vendor.unisoc.*` 命名空间，AOSP framework 不直接依赖。
3. **vendor 的 device compatibility matrix 只反向要求 system 提供 6 个标准 HIDL + `vendor-ndk 33` + `system-sdk 33`** —— LOS 20 默认全都带，理论上开箱满足（移植后用 `checkvintf` 复验）。
4. **内核是 Android 12 KMI**（5.4.210；`odm/etc/vintf/manifest/manifest_kernel.xml` 声明 `kernel target-level=6`）。A13 framework 接受这种组合（原厂就是这么跑的）；**只要保留原厂 `boot`/`vendor_boot`/`vendor_dlkm`，内核侧不需要动**。
5. **移植策略一句话**：原厂 `vendor + odm + vendor_dlkm + boot + vendor_boot + dtbo` 全留，只替换 `system`（先）→ `system_ext` / `product`（后）。
6. `system_ext` 里有**展锐 framework 扩展 + 讯飞定制 UI/相机/图库**；替换后 = 回到 AOSP/LOS 界面，学习机功能全没（硬件功能不受影响）。
7. 三个「没有」需要注意：**没有 Widevine**（只有 clearkey）、**没有标准 GNSS HAL**、**radio 与 SKU 相关**（本 dump 的 ODM 带 `sku_wifionly`，基本可判定 WiFi-only）。

---

## 1. 解包内容

| 分区 | 镜像 | 展开后 | 文件数 | 角色 |
|---|---|---|---|---|
| `super`（10 GB，virtual_ab_device，group_unisoc_a） | — | — | — | 动态分区容器 |
| `system_a` | 6.5 GB | 8.0 GB | 2683 | framework 本体、2 个 system HAL、framework manifest、FCM 3–7、plat sepolicy |
| `vendor_a` | 643 MB | 868 MB | 2035 | **HAL server + impl lib + VINTF + init rc + 固件 + 模型** |
| `odm_a` | 236 MB | 324 MB | 744 | **板级配置（audio/camera/sensor/isppipeline）、camera/fingerprint impl、odm manifest、precompiled sepolicy** |
| `system_ext_a` | 288 MB | 461 MB | 362 | 展锐 framework 扩展 jar + 讯飞/展锐定制 app + 展锐 native libs |
| `product_a` | 198 MB | 275 MB | 278 | 通用 AOSP 应用 + overlay |
| `vendor_dlkm_a` | 8 MB | 22 MB | 145 | 134 个 vendor 内核模块 `.ko` |
| `system_a-cow` | 2.38 GB（快照） | — | — | 虚拟 A/B OTA 快照；只有 system 受影响，其余分区干净 |

> `system_a` 的 `-cow` 快照说明这份 dump 是在 OTA 合并前抓的；`lpunpack` 抽到的是 base system。对移植无影响（system 本来就要整个换掉）。

---

## 2. HAL 的完整调用链（找问题时按这 5 层查）

```
 ① /vendor/etc/vintf/manifest.xml + manifest/*.xml     ← 接口声明（AIDL/HIDL、版本、实例名）
 ② /vendor/etc/init/*.rc                               ← 谁把 HAL 拉起来（service/class hal/lazy）
 ③ /vendor/bin/hw/<service>                            ← HAL server 可执行文件（45 个）
 ④ /vendor/lib64/hw/<impl>.so 或 /odm/lib64/hw/*.so    ← 真正干活的 impl 库（37 + 2 个）
 ⑤ /vendor/etc + /odm/etc + vendor_dlkm/*.ko + firmware ← 配置、参数、固件、内核驱动
```

关键数字：

| 类别 | 数量 |
|---|---|
| 设备 VINTF HAL 唯一条目 | **67**（AIDL 43 / HIDL 24） |
| `vendor/etc/vintf/manifest.xml` + 片段 | 1 + 35 个文件，70 条声明 |
| `odm/etc/vintf/` | `manifest.xml` + `manifest/manifest_kernel.xml`（kernel level 6）+ `manifest_wifionly.xml`（构建期 SKU 用） |
| `vendor/bin/hw` 二进制 | 45 个（38 个 init `class hal`，3 个 `early_hal`，其余为 wpa_supplicant / hostapd / urild / cnn_testbench） |
| HAL impl 库 | `vendor/lib64/hw` 37 个 + `odm/lib64/hw` 2 个（`camera.ums9620.so`、`microarray.fingerprint.default.so`） |
| init rc | `vendor` 92 + `odm` 2 + `system_ext` 18 = 112 个，126 个 service |
| `vendor/etc/permissions` | 41 个 feature xml（wifi/bt/指纹/人脸/camera/telephony/vulkan…） |
| SELinux | `vendor/etc/selinux/vendor_sepolicy.cil` 672 KB + `plat_pub_versioned.cil` 949 KB；`odm/etc/selinux/precompiled_sepolicy` 770 KB；`system_ext_sepolicy.cil` 95 KB |
| vendor 固件 | `vendor/firmware` 53 项；`odm/firmware` 12 项 |
| vendor 模型 | `vendor/etc/models` 88 MB、`vendor/etc/aiimg` 70 MB、`inceptionv3_mbs.bin` 17 MB |
| DLKM | 134 个 `.ko`，`modules.load` 133 行 + `modules.load.charger` |
| vendor/odm native 库 | `vendor/lib64` 269 个 `.so`、`odm/lib64` 273 个 `.so`（32 位目录各有一份） |

---

## 3. 设备 HAL 总表（按功能分组）

> 「LOS 20」列是移植时的预判：`OK` = A13 原生支持该接口版本、保留 vendor 即应工作；`注意` = 接口在但依赖 vendor/odm 配置或需要应用层配合；`风险` = 无标准接口或很可能残。

### 3.1 显示 / 图形

| HAL / 组件 | 版本 | 实例 | 服务 / 实现 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.graphics.composer` | HIDL 2.4 | default | `vendor.hwcomposer-2-4` → `hwcomposer.unisoc.so`、`dpu.unisoc.so`、`sprd-drm.ko` | 注意（A13 仍支持 HIDL composer 2.4；双 DSI/多屏/PQ 逻辑在原厂 dtbo+ko 里，保留即可） |
| `android.hardware.graphics.allocator` | HIDL 4.0 | default | `vendor.gralloc-4-0` → `allocator@4.0-impl-arm.so` | OK |
| `android.hardware.graphics.mapper` | HIDL 4.0 | default | `mapper@4.0-impl-arm.so`、`gralloc.default.so` | OK |
| GSP / DPU / Vulkan | — | — | `gsp.unisoc.so`、`dpu.unisoc.so`、`vulkan.ums9620.so` | 注意（标准 Vulkan，但 `ro.hardware.gpu/gsp` 属性照读 vendor） |

### 3.2 音频

| HAL | 版本 | 服务 | 实现 / 配置 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.audio` | HIDL 7.1 | `vendor.audio-hal`（`android.hardware.audio.service`） | `audio@7.1-impl.so`（`ro.hardware.audio.primary=whale`）、`snd-soc-sprd-*.ko`、`snd-soc-sprd-pa-aw883xx.ko` | OK（A13 的 audio HAL 就是 7.1） |
| `android.hardware.audio.effect` | HIDL 7.0 | 同上进程 | `audio.effect@7.0-impl.so`、`awinic.audio.effect.so`、`aw883xx_acf.bin` 等参数 | 注意（效果库和参数在 vendor/firmware，保留即可） |
| `android.hardware.bluetooth.audio` | AIDL 2 | 蓝牙进程 | `audio.bluetooth.ums9620.so` | OK |
| `android.hardware.soundtrigger` | HIDL 2.3 | `soundtrigger@2.3-impl.so` | — | OK |
| audio 子 HAL | — | — | `audio.primary.default.so`、`audio.usb.default.so`、`audio.r_submix.default.so`、`audio.dp.ums9620.so`、`parameter-framework` 配置、`odm/etc/audio/*`（10 个 policy xml）、`odm/etc/audio_params`（7.8 MB） | 注意（audio policy 全在 ODM，必须保留） |

### 3.3 相机 / 媒体

| HAL | 版本 | 服务 | 实现 / 配置 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.camera.provider` | HIDL 2.4 | `vendor.camera-provider-2-4`（实例 `legacy/0`） | `camera@2.4-impl-sprd.so` + `vendor/lib64/*camera*` + **`odm/lib64/hw/camera.ums9620.so`** + `odm/etc/camera/3rdpool` + `isppipeline` + sensor 固件 | 注意/风险（HAL 是标准的；出图依赖 ODM impl 与大量 vendor lib/固件，LOS 侧要实测） |
| `android.hardware.media.c2` | HIDL 1.1 | `media.unisoc.codec2` | `android.hardware.media.c2@1.1-unisoc-service` | 注意（硬编解码全走 vendor，A13 支持 HIDL C2 1.1/1.2） |
| `android.hardware.media.omx` | HIDL 1.0 | `vendor.media.omx` | `libstagefright_*`/OMX 库 | 注意 |
| media profiles | — | — | `vendor/etc/media_profiles_se50.xml`、`media_codecs_c2_secure.xml` | OK |
| 相机应用 | — | — | `system_ext` 的 `IflyDreamCamera2`、`IflyNewGallery2`（**会被 LOS 替换掉**） | 注意 |

### 3.4 传感器 / 生物识别

| HAL | 版本 | 服务 | 实现 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.sensors` | AIDL 1 | `vendor.sensors-hal-multihal` | `sensors.unisoc.so`、`sensors@2.X-sprd_subhal.so`、`sprd_sensor.ko`、`odm/etc/sensor_config.xml` | OK（A13 支持 AIDL sensors；配置在 ODM） |
| `android.hardware.biometrics.fingerprint` | HIDL 2.1 | `vendor.fps_hal`（lazy，disabled） | `odm/lib64/hw/microarray.fingerprint.default.so`、`microarray_fp.ko` | 注意（接口在；MMI/校准/信息安全链路依赖 vendor 服务） |
| `android.hardware.biometrics.face` | AIDL 2 | `vendor.face-default`（lazy） | `face.default.so`、`face_*.bin/elf` 模型 | 注意 |
| `vendor.sprd.hardware.fingerprintmmi` | AIDL | `vendor.fingerprintmmi-default`（lazy） | MMI 工厂测试 | 忽略（工厂测试用） |

### 3.5 安全 / TEE

| HAL | 版本 | 服务 | 说明 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.security.keymint` | AIDL 2 | `vendor.keymint-unisoc-2.0`（`early_hal`） | trusty TEE | OK（A13 用 keymint 2） |
| `android.hardware.security.secureclock` / `sharedsecret` | AIDL | 同上 | — | OK |
| `android.hardware.gatekeeper` | HIDL 1.0 | `vendor.gatekeeper-1-0` | trusty | OK（A13 仍支持 HIDL 1.0） |
| `android.hardware.secure_element` | HIDL 1.0 | SIM1/SIM2（通用 vendor 镜像里声明） | — | 忽略（WiFi-only SKU 无 SIM） |
| `android.hardware.oemlock` | AIDL | `vendor.oemlock-default` | 解锁状态 | OK |
| `vendor.sprd.hardware.soter` | AIDL | `vendor.soter-default`（lazy） | 微信 SOTER 指纹支付链 | 忽略/注意 |
| `android.hardware.rebootescrow` | AIDL | `vendor.rebootescrow-default` | — | OK |
| `vendor.sprd.hardware.trusty` | AIDL | `vendor.sprd.hardware.trusty-service` | trusty 桥 | 保留（vendor 内自洽） |

### 3.6 无线（Wi-Fi / BT / Radio）

| HAL | 版本 | 服务 | 实现 / 固件 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.wifi` | HIDL 1.6 | `vendor.wifi_hal_legacy` | `libwifi-hal-unisoc.so`、`liblowi_wifihal.so`、`wcn_bsp.ko`、`sprd_wlan_combo.ko`、`firmware/wcnmodem.bin`、`odm/firmware/wifi_board_config*.ini` | OK（A13 支持 HIDL wifi 1.6 + AIDL supplicant） |
| `android.hardware.wifi.supplicant` / `hostapd` | AIDL | `wpa_supplicant` / `hostapd`（lazy） | — | OK |
| `vendor.sprd.hardware.wcn` | HIDL 1.0 | `vendor.sprd.hardware.wcn@1.0-service` | 联通性控制 | 保留 |
| `vendor.sprd.hardware.connmgr` | HIDL 1.0 | `vendor.sprd.hardware.connmgr@1.0-service` | 网络连接管理 | 保留 |
| `android.hardware.bluetooth` | HIDL 1.1 | `vendor.bluetooth-1-1` | `sprdbt_tty.ko`、`bt_configure_rf.ini` | OK |
| radio（standard 7 套 + `vendor.unisoc.*` 扩展 7 套） | AIDL | `vendor.ril-daemon`（`urild` + `librilcore.so`） | 需要 modem | 忽略（**本 dump ODM 带 `sku_wifionly`，基本 WiFi-only**；4G 版本另算） |
| `vendor.sprd.hardware.broadcastradio` | HIDL 2.0 | `vendor.sprd.broadcastradio-hal2` | `sprd_fm.ko` | 忽略 |

### 3.7 电源 / 热 / 杂项

| HAL | 版本 | 服务 | 说明 | LOS 20 |
|---|---|---|---|---|
| `android.hardware.power` | AIDL 3 | `vendor.power-default`（`vendor.unisoc.hardware.power-service`） | unisoc 性能/功耗策略 | OK |
| `android.hardware.power.stats` | AIDL 1 | `vendor.power.stats-default` | — | OK |
| `android.hardware.thermal` | HIDL 1.0/2.0 | `vendor.thermal-hal-2-0` | `sprd_soc_thm.ko`、`thermal.default.so` | OK |
| `vendor.sprd.hardware.thermal` | HIDL 1.0/2.0 | `vendor.sprd.hardware.thermal@2.0-service` | 扩展温控 | 保留 |
| `android.hardware.light` | AIDL 2 | `vendor.sprd.hardware.lights-service` | 背光/灯 | OK |
| `android.hardware.vibrator` | AIDL 2 | `vendor.sprd.hardware.vibrator-service` | — | OK |
| `android.hardware.health` | AIDL 1 | `vendor.health-default` | `sprd_battery_info.ko`、`sprd-charger-manager.ko`、`sgm415xx-charger.ko`、`cw221x_battery.ko`、`sc27xx_typec.ko`、`bq2560x/bq2597x/sc8551-charger.ko` | OK（充电/电量在 kernel + health HAL，与 LOS 的 BatteryService 标准对接） |
| `android.hardware.usb` | AIDL 1 | `vendor.usb_default` | `dwc3-sprd.ko`、`musb_hdrc.ko`、`extcon-usb-gpio.ko` | OK |
| `android.hardware.boot` | HIDL 1.2 | `vendor.sprd.boot-hal-1-2`（early_hal） | A/B 槽控制 | OK |
| `android.hardware.memtrack` | AIDL | `vendor.sprd.hardware.memtrack-service` | — | OK |
| `vendor.sprd.hardware.enhance` | AIDL | `vendor.enhance-default` | 显示增强（PQ） | 保留 |
| `vendor.sprd.hardware.log` / `cplog_svc` / `network` | AIDL | 对应 service | 日志/网络调试 | 保留 |
| `vendor.unisoc.hardware.ai_engine` | AIDL | `vendor.ai_engine-default`（lazy） | NPU/AI 引擎；`vendor_dlkm` 里有 `npu_img_vha.ko` | 保留（AI 学习功能依赖它 + system_ext） |
| `android.hardware.neuralnetworks` | HIDL 1.3 + AIDL 4 | armnn-gpu / xtensa / unisoc-nn-shim | 加速推理 | OK/注意 |
| `android.hardware.drm` | AIDL 1 | `vendor.drm-clearkey-service` | **只有 clearkey，没有 Widevine** | 注意（流媒体 App 会挂） |
| `android.hardware.cas` | HIDL 1.2 | `vendor.cas-hal-1-2` | — | OK |
| `android.hardware.renderscript` | HIDL 1.0 | `renderscript@1.0-impl.so` | — | OK（A13 已弃用但兼容） |
| GNSS | **无 VINTF HAL** | `gpsd`（core）+ `gps.default.so` + `odm/firmware/gnssmodem.bin` | 不是标准 Treble GNSS | 风险（A13 用 HIDL/AIDL gnss；本机是 legacy 路径，可能无定位） |

---

## 4. system 侧（LOS 20）必须提供什么

### 4.1 vendor 的 device compatibility matrix（`vendor/etc/vintf/compatibility_matrix.xml`，type=device）

这是 **vendor 反向要求 system 提供**的东西：

| 要求 | 说明 | LOS 20 |
|---|---|---|
| `android.frameworks.sensorservice@1.0` | system 的 SensorService 老 HIDL 接口 | 有 |
| `android.hidl.allocator@1.0`（ashmem） | — | 有 |
| `android.hidl.manager@1.0` | — | 有 |
| `android.hidl.memory@1.0`（ashmem） | — | 有 |
| `android.hidl.token@1.0` | — | 有 |
| `android.system.wifi.keystore@1.0` | — | 有 |
| `vendor-ndk 33` / `system-sdk 33` | VNDK 33 / API 33 | 有（LOS 20 = A13） |

### 4.2 framework manifest（system 侧自带的 HAL，共 17 条）

`system/etc/vintf/manifest.xml` + 片段：`android.frameworks.{displayservice,schedulerservice,sensorservice,stats,cameraservice.service 2.2}`、`android.hidl.{manager 1.2, memory 1.0, token 1.0, allocator 1.0}`、`android.system.{net.netd 1.1, wifi.keystore 1.0, keystore2 2, suspend 1.0}`、`android.hardware.media.c2 1.2 (software)`、`netutils-wrapper`。
`system/bin/hw` 里实物只有 2 个：`android.hidl.allocator@1.0-service`、`android.system.suspend@1.0-service` —— **LOS 20 构建时要在 system 镜像里确认这两个在**，否则 vendor 的 matrix 校验会报错。

### 4.3 内核 / FCM

- `system/etc/vintf/compatibility_matrix.{3,4,5,6,7}.xml`：A13 的 framework matrix（本机 target-level **7**）。
- FCM 7 的 kernel 要求是 `5.10.107 / 5.15.41 (level 7)`，FCM 6 是 `5.4.86 (level 6)`；本机内核 5.4.210 通过 `odm/etc/vintf/manifest/manifest_kernel.xml` 声明 `kernel target-level=6` 走 level 6 校验 —— **原厂 A13 就是这么跑的，不要改它，也不要换掉 vendor_boot 里的 DTB/模块**。
- 移植后自检：`checkvintf`（LOS 自带）/ 开机 logcat 里看 `VintfObject` 报错。

---

## 5. ODM 板级配置（换 system 也必须留，HAL 的"参数"都在这）

| 路径 | 内容 | 大小 |
|---|---|---|
| `odm/etc/camera/3rdpool` | 相机第三方算法池 | 24 KB |
| `odm/etc/audio/*.xml` | audio policy（primary/bluetooth/dp/usb/r_submix/volume） | 60 KB |
| `odm/etc/audio_params/` | 音频 DSP 参数 | 7.8 MB |
| `odm/etc/audio_route*.xml` | 音频路由 | 132 KB |
| `odm/etc/sensor_config.xml` | 传感器配置 | 4 KB |
| `odm/etc/isppipeline/Pipeline_Linkage.json` | ISP pipeline | 16 KB |
| `odm/etc/unipnp/UniFeature.xml` | 展锐性能特性 | 28 KB |
| `odm/etc/zoom_config.json`、`lwp/`、`aiimg/` | 相机变焦、动态壁纸、AI 图像 | 376 KB + … |
| `odm/etc/permissions/` | camera flash/front/manual sensor、soter、`sku_wifionly/unavailable-telephony-feature.xml` | 28 KB |
| `odm/etc/sysconfig/`、`vintf/`、`selinux/` | 系统配置、VINTF（含 kernel level 6）、precompiled sepolicy | — |
| `odm/firmware/` | `wcnmodem.bin`、`gnssmodem.bin`、`wifi_board_config*.ini`、`bt_configure_*.ini`、`gatekeeper.elf`、`mafp.elf`、`novatek_ts_fw.bin` | 3 MB |

> **结论：ODM 是"板级 HAL 配置"的集中地，LOS 移植时原样保留。** 一旦丢了 ODM，相机/音频/传感器/温控会变成"接口在但参数错"的状态。

---

## 6. vendor 侧固件 / 模型 / 内核模块

- **`vendor/firmware`（53 项）**：
  - 相机 sensor 固件：`gc05a2/`、`hi846/`、`ov02b10/`、`ov08d10/`、`ov08d10_SU30/`、`ov16885/`、`ov16885_front/` …
  - 触摸：`Himax_firmware.bin`、`Himax_mpfw.bin`、`hx_criteria.csv`（himax 驱动在内核内建）
  - 音频：`aw883xx_acf.bin`、`awinic_params.bin`、`awinic_sinwave_params.bin`
  - AI/影像：`face*.bin/elf`、`hdcp.elf`、`Depth_NNA.mbs.bin`、`hdrnet_NNA.mbs.bin`、`gesture.mnn`
  - 其它：`cae.ini`、`fdr_cadence.bin`
- **`vendor/etc/models` 88 MB / `vendor/etc/aiimg` 70 MB / `inceptionv3_mbs.bin` 17 MB**：讯飞 AI 学习功能用的模型（换 system_ext 后这些还在，但没有 app 调用它们了）。
- **`vendor_dlkm/lib/modules`（134 个 `.ko`，`modules.load` 顺序加载）**：
  - 显示：`sprd-drm.ko`、`sprd-gsp.ko`、`dpu` 相关
  - 相机：`sprd_camera.ko`、`sprd_cpp.ko`、`jpg.ko`、`sprd_camsys_pw_domain.ko`、`mmdvfs.ko`
  - 音频：`snd-soc-sprd-*.ko`（card/codec-ump9620/dai/tdm/vbc/pdm/pa-aw883xx/unisoc-dp-codec）、`audio_mem.ko`、`audio-pipe.ko`、`audio_sipc.ko`
  - 无线：`wcn_bsp.ko`、`sprd_wlan_combo.ko`、`sprdbt_tty.ko`、`sprd_fm.ko`、`gnss_*.ko`
  - 充电/电源：`sprd-charger-manager.ko`、`sgm415xx-charger.ko`、`cw221x_battery.ko`、`sc27xx_typec.ko`、`bq2560x/bq2597x/sc8551-charger.ko`、`sy65153_wireless_charger.ko`
  - 触摸（非 himax 机型）：`focaltech_ft8756_spi_ts.ko`、`nvt_nt36xxx_spi_ts.ko`、`nvt_touch_host.ko`、`synaptics_td4320_spi_ts.ko`
  - 其它：`microarray_fp.ko`、`npu_img_vha.ko`、`mali_kbase.ko`、`trusty-tui.ko`、`dwc3-sprd.ko`、`musb_hdrc.ko`、`spi-sprd.ko`、`pwm-sprd.ko`、`ion_*.ko`、`unisoc-iommu.ko`
- `vendor_boot` 的 vendor ramdisk 里另有 **152 个 `.ko`** 和 first-stage fstab（见 `reports/TWRP-partition-choice.md`），正常启动第一阶段要用，**不要整包替换 vendor_boot**。

---

## 7. 会被 LOS 替换的部分（`system` / `system_ext` / `product`）

### 7.1 `system_ext`（461 MB）—— 换掉后变化最大

| 内容 | 说明 |
|---|---|
| 展锐 framework 扩展 jar | `unisoc-framework.jar`（320 KB）、`uni-telephony-common.jar`（576 KB）、`com.unisoc.sdk.common.jar`（61 KB）、`unipnp-framework.jar`（24 KB）、`smartlink_sdk.jar`（8 KB） |
| 讯飞定制 UI / 应用 | `IflySettings`、`IflySystemUI`、`IflyLauncher3QuickStep`、`IflyDreamCamera2`、`IflyNewGallery2`、`IflylibSprdImageFilterResource.so` 等 |
| 展锐应用 | `EngineerMode`、`CameraCalibration`、`CamTa`、`LinkTurbo`、`SGPS`、`SoterService`、`USCPhotosProvider`、`ValidationTools`、`WAPPushManager`、`ims`、`radio_interactor_service`、`Srmi`、`SprdDialer` 等（app 11 + priv-app 24） |
| 展锐 native libs | 74 个 `.so`：`libfaceverify.so`（26.9 MB）、`libSprdAppImageFilter.so`（8.1 MB）、`libmme_jrtc.so`、`libtflite_prebuilt.so`、`IflylibcamIP.so`、`libjni_sprd_fa/fd.so` 等 |
| 配置/模型 | `etc/models` 19 MB、`etc/vilte`、`etc/profiles`、`etc/permissions` 24 个 |

### 7.2 `system`（8.0 GB）

framework 本体 + 2 个 system HAL + framework manifest + FCM 3–7 + `plat_sepolicy.cil`（2.0 MB）。**LOS 20 会用自己的**，等价替换。

### 7.3 `product`（275 MB）

通用 AOSP 应用（`LatinIME`、`webview`、`PhotoTable`…）+ overlay；LOS 用自己的即可。

> ⚠️ 换了 `system_ext` 后：讯飞学习机功能（家长管控 / AI 学习 / 讯飞相机 / 讯飞桌面）全部消失；展锐 framework 扩展 jar 也消失。**但硬件 HAL 全在 vendor/odm，不受影响。**

---

## 8. LOS 20 移植时 HAL 层怎么处理

### 8.1 分区策略

| 分区 | 处理 | 原因 |
|---|---|---|
| `boot` | 保留原厂（或换 TCA Cycle Kernel） | GKI 5.4.210，HAL 的 kernel 侧依赖都在 |
| `vendor_boot` | **保留原厂** | DTB + 152 个 first-stage `.ko` + recovery 资源 |
| `dtbo` | 保留原厂 | 9 个机型 overlay（`reports/DTBO-models.md`） |
| `vendor` / `odm` / `vendor_dlkm` | **保留原厂** | 67 个 HAL 的 server/impl/配置/固件/ko 全在这 |
| `system` | 先只换这个 | framework 侧，等价"源码版 GSI" |
| `system_ext` / `product` | 跑通后再换 | 换了就丢讯飞定制 |

### 8.2 最小验证路径

1. 备份（已有 super/boot/vendor_boot dump + TWRP）。
2. 设备树起骨架（aospdtgen；输入 = `out/lp/{system,vendor,odm,system_ext,product}_rd`）。
3. 只构建 `m systemimage`，fastbootd 刷 `system_a`；`vendor/odm/system_ext/product` 全部原厂。
4. 开机验证（见 8.3），再决定要不要换 `system_ext/product`。

### 8.3 逐项验证清单

| 项 | 验证方法 | 期望 / 风险 |
|---|---|---|
| 显示/亮度/双屏 | 开机、旋转、HDMI/DP（若有）、`dumpsys SurfaceFlinger` | `composer 2.4` A13 支持；双 DSI 时序在 dtbo/ko，保留即应工作 |
| 触摸/手写笔 | 触摸、悬停、压感、休眠唤醒 | himax 驱动内建 + 固件在 vendor；`TP_RESUME_BY_LCD` 由 kernel/ko 负责 |
| 音频 | 外放/耳机/USB/DP/蓝牙、录音、音量 | audio 7.1 标准；ODM policy 保留即生效 |
| 相机 | 前后摄、拍照/录像、闪光灯 | provider 2.4 + odm impl；LOS 用 Camera2 应用，可能要修 metadata/权限 |
| 传感器 | 自动旋转、光线、霍尔 | sensors AIDL + `sensor_config.xml` |
| WiFi/BT | 扫描、连接、热点、蓝牙耳机 | wifi 1.6 + AIDL supplicant；RRO overlay 在 vendor |
| 指纹/人脸 | 录入、解锁 | 依赖 vendor 服务与校准数据 |
| 电源/充电/电量 | 插拔、关机充电、电量曲线 | health AIDL + 充电 ko |
| 媒体 | 本地播放、YouTube（无 Widevine 会挂） | c2 1.1 硬解 + clearkey |
| GNSS | 定位 | **无标准 HAL，大概率无定位** |
| 学习机功能 | — | **全无**（system_ext 被替换） |

### 8.4 风险分级

- **低**：audio 7.1、composer 2.4、allocator/mapper 4.0、wifi 1.6 + AIDL、sensors AIDL、keymint 2.0、gatekeeper 1.0、health AIDL、lights/vibrator、usb、power、thermal —— 接口版本与 A13 官方一致，保留 vendor 即应工作。
- **中**：相机（sprd provider 2.4 + ODM impl + 固件）、媒体 c2 1.1/omx 1.0、指纹/人脸（依赖 vendor 校准/服务）、双屏/多屏与 PQ（`ro.vendor.display.*` 在 vendor，但 SF 行为要实测）。
- **高/未知**：GNSS（无标准 HAL）、Widevine（没有）、4G 版本 radio（本机 WiFi-only）、**system_ext 换掉后的讯飞定制全丢**。
- **刷机注意**：虚拟 A/B 单物理槽 + 当前 `system_a-cow` 未合并快照 → 用 fastbootd 刷 `system_a`，别再走 stock OTA 逻辑。

---

## 9. 复现

```sh
# 1) 解包（默认读 /home/misakapardola/下载/spd_dump_.../260606_193212，输出到 out/lp）
tools/extract-stock-hal.sh

# 2) 只解某几个分区
HAL_PARTS="vendor_a odm_a" tools/extract-stock-hal.sh

# 3) 产物
out/lp/system_rd/      out/lp/vendor_rd/    out/lp/odm_rd/
out/lp/system_ext_rd/  out/lp/product_rd/   out/lp/vendor_dlkm_rd/
```

相关文档：`reports/DTBO-models.md`（9 机型 DTBO）、`reports/TWRP-partition-choice.md`（vendor_boot 里的 recovery/模块）、`reports/REVERSE-iflytek-delta.md`（讯飞对显示/触摸/充电的内核改动）。
