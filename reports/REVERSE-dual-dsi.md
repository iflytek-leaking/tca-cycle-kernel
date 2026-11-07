# 讯飞 sprd-drm.ko 双通道 DSI（dual-DSI）逆向报告

> 目标模块：`vendor_rd/lib/modules/sprd-drm.ko`（ELF64 aarch64, **未 strip**, ET_REL, BuildID 85d341e1ab1da700）
> 对照源码：`kernel_ums9620_se50/drivers/gpu/drm/sprd/`
> 方法：Ghidra 12.0 headless（JAVA_HOME=/usr/lib/java-21）导入 + 自动分析 + 反编译；配合 `aarch64-linux-gnu-objdump -dr` / `readelf` 校验重定位与调用目标。
> 全文 **【推测】** 表示由反汇编推断、无源码或字符串直接佐证。

---

## 0. 结论速览（TL;DR）

1. 原厂双通道不是“两个独立 DRM 通路”，而是 **一个 master 通路 + 一个 slave 从属通路**：
   - master = `dev-id = 0` 的 DSI/DPHY（DT 节点 `dsi@31300000` / `dphy`）；
   - slave = `dev-id = 1` 的 DSI/DPHY（DT 节点 `dsi1@31400000` / `dphy1`）；
   - 两者用 **DT phandle `sprd,dual-channel`** 相连（不是全局数组）。
2. slave 使能开关来自 **bootargs 的 `lcd_name=` → `/lcds/<name>` → `sprd,dual-dsi-enable`**，由 `sprd_dual_dsi_parse_dt()` 存入 `dsi->ctx.dual_dsi_en`。
3. **关键发现**：原厂 源码树其实**已经包含**了 slave 的底层 glb 实现（`global_dsi.c::dsi_s_*`、`global_dphy.c::dphy_s_*`），只是 **没有接线**：缺 ops 结构体、缺 match 表项、缺 probe/bind/encoder/host_attach 的 master↔slave 逻辑、缺结构体字段、缺 DT phandle。原厂 .ko 里对应符号为 `qogirn6pro_dsi1`/`qogirn6pro_dphy1`（**不是** `qogirn6pro_dsi_s_glb_ops` 直挂 master）。
4. master 驱动 slave 的方式：
   - **建立链接**：probe 阶段用 `of_parse_phandle(np,"sprd,dual-channel",0)` → `of_find_device_by_node()` → `platform_get_drvdata()` 拿到 slave 的 `struct sprd_dsi`，互指并存 `ctx.master/ctx.slave`；
   - **时钟/syscon**：`sprd_dsi_glb_init()` 与 `sprd_dsi_enable()` 会递归到 slave（power/enable/reset + core 初始化）；
   - **PHY**：`sprd_dphy_enable()` 递归到 slave PHY（`sprd_dphy_init`），失败打印 `sprd dphy slave init failed`；
   - **参数同步**：`sprd_dsi_host_attach()` 把 master 的 lanes/format/work_mode/burst_mode/nc_clk/byte_clk/esc_clk 复制给 slave ctx，并把 slave PHY 的 lanes/freq 赋值。

---

## 1. 地址与符号对照

ET_REL 下符号值 = 段内偏移；Ghidra 把 `.text` 放在 image base **0x129000**，故 `Ghidra VA = readelf st_value + 0x129000`。下表统一给 **readelf st_value（真值）**。

| 符号 | st_value | 大小 | 所在源码（推测） |
|---|---:|---:|---|
| `sprd_dual_dsi_parse_dt` | 0xcfbc | 376 | sprd_dsi.c |
| `sprd_dsi_probe` | 0xd134 | 672 | sprd_dsi.c |
| `sprd_dsi_bind` | 0xd418 | — | sprd_dsi.c |
| `sprd_dsi_encoder_enable` | 0xdfec | 2004 | sprd_dsi.c |
| `sprd_dsi_encoder_disable` | 0xdc14 | 984 | sprd_dsi.c |
| `sprd_dsi_encoder_disable_force` | 0xcb48 | 680 | sprd_dsi.c |
| `sprd_dsi_context_init` | 0xe940 | — | sprd_dsi.c |
| `sprd_dsi_host_attach` | 0xec1c | — | sprd_dsi.c |
| `sprd_dsi_enable`（static） | 0xe7f0 | 336 | sprd_dsi.c |
| `sprd_dsi_disable`（static） | 0xcdf0 | 460 | sprd_dsi.c |
| `sprd_dphy_probe` | 0xf4c8 | — | sprd_dphy.c |
| `sprd_dphy_enable` | 0xf050 | 492 | sprd_dphy.c |
| `sprd_dphy_disable` | 0xf23c | 652 | sprd_dphy.c |
| `sprd_dphy_context_init` | 0xf738 | — | sprd_dphy.c |
| `dsi_s_glb_parse_dt` | 0x16dc0 | 256 | qogirn6pro/global_dsi.c |
| `dsi_s_glb_enable` | 0x16ec0 | 96 | 同上 |
| `dsi_s_glb_disable` | 0x16f20 | 56 | 同上 |
| `dsi_s_reset` | 0x16f58 | 124 | 同上 |
| `dphy_s_glb_parse_dt` | 0x1732c | 256 | qogirn6pro/global_dphy.c |
| `dphy_s_glb_enable` | 0x1742c | 68 | 同上 |
| `dphy_s_glb_disable` | 0x17470 | 68 | 同上 |
| `dphy_s_power_domain` | 0x174b4 | 116 | 同上 |
| `qogirn6pro_dsi` | 0xcb18 | 16 | sprd_dsi.c 数据 |
| `qogirn6pro_dsi1` | 0xcb28 | 16 | sprd_dsi.c 数据 |
| `qogirn6pro_dsi_glb_ops` | 0xda48 | 40 | global_dsi.c |
| `qogirn6pro_dsi_s_glb_ops` | 0xda70 | 40 | global_dsi.c |
| `qogirn6pro_dphy` | 0xd3f8 | 24 | sprd_dphy.c 数据 |
| `qogirn6pro_dphy1` | 0xd410 | 24 | sprd_dphy.c 数据 |
| `qogirn6pro_dphy_glb_ops` | 0xda98 | 32 | global_dphy.c |
| `qogirn6pro_dphy_s_glb_ops` | 0xdab8 | 32 | global_dphy.c |

**ops 结构体内容（由 `readelf -rW` 重定位证实）**：

```
qogirn6pro_dsi   (0xcb18) = { dsi_ctrl_r1p0_ops, qogirn6pro_dsi_glb_ops   }
qogirn6pro_dsi1  (0xcb28) = { dsi_ctrl_r1p0_ops, qogirn6pro_dsi_s_glb_ops }   <-- slave
qogirn6pro_dphy  (0xd3f8) = { dsi_ctrl_ppi_ops, sharkl5_dphy_pll_ops, qogirn6pro_dphy_glb_ops   }
qogirn6pro_dphy1 (0xd410) = { dsi_ctrl_ppi_ops, sharkl5_dphy_pll_ops, qogirn6pro_dphy_s_glb_ops } <-- slave
```

> **注意**：原厂 树当前（含父 agent 的改动）把 `sprd,qogirn6pro-dsi1-phy` 映射到 **master** 的 `qogirn6pro_dphy`，这是错的，必须映射到新建的 slave ops。

**相关字符串**（文件偏移 / 内容）：

| 偏移 | 字符串 |
|---|---|
| 0xa864 | `sprd,dual-channel` |
| 0xcf89 | `dsi slave skip other action` |
| 0xda45 | `dual-dsi-enable is not found!` |
| 0xe780 | `[drm] find sprd,dual-channel` |
| 0xe029 | `[drm][%20s] %s enter` |
| 0xf36d | `parse dual dsi info failed` |
| 0xf8b2 | `sprd,dual-dsi-enable` |
| 0x11553 | `[drm][%20s] dsi_slave %s` |
| 0x1226a | `sprd dphy slave init failed` |
| 0x11ebd | `[drm] lcd name: %s` |

---

## 2. 还原的 C 代码

### 2.1 `sprd_dual_dsi_parse_dt()`（master probe 时调用）

语义：`/chosen` → `bootargs` → 找 `lcd_name=` → 拼 `/lcds/<name>` → 读 `sprd,dual-dsi-enable` → 写 `ctx->dual_dsi_en`（.ko 偏移 `ctx+0xA0`）。

```c
/* 还原自 sprd_dual_dsi_parse_dt @0xcfbc，原厂 树缺失此函数 */
int sprd_dual_dsi_parse_dt(struct sprd_dsi *dsi, struct device_node *np)
{
	struct device_node *chosen, *lcd_node;
	const char *bootargs = NULL;
	char name[64] = {0};
	char path[64];
	const char *p;
	u32 val = 0;
	int ret;

	chosen = of_find_node_opts_by_path("/chosen", NULL);
	ret = of_property_read_string(chosen, "bootargs", &bootargs);
	if (ret) {
		drm_err("can't not parse bootargs property\n");
		return ret;                 /* 原实现返回 of_property_read_string 的错误码 */
	}

	p = strstr(bootargs, "lcd_name=");
	if (p) {
		/* 原汇编：sscanf(p, "lcd_name=%s", name); pr_info("[drm] lcd name: %s\n", name); */
		sscanf(p, "lcd_name=%s", name);
		pr_info("lcd name: %s\n", name);
	}

	snprintf(path, sizeof(path), "/lcds/%s", name);
	lcd_node = of_find_node_opts_by_path(path, NULL);
	if (!lcd_node) {
		drm_err("%pOF: could not find %s node\n", np, name);
		return -ENODEV;
	}

	if (of_property_read_u32(lcd_node, "sprd,dual-dsi-enable", &val)) {
		drm_dbg(1, "dual-dsi-enable is not found!\n");
		return 0;                   /* 属性缺失：不算错误，dual_dsi_en 保持 0 */
	}

	dsi->ctx.dual_dsi_en = val;
	return 0;
}
```

反汇编要点：
- `of_find_node_opts_by_path("/chosen",0)` → `of_property_read_string(chosen,"bootargs",&p)`；
- `strstr(bootargs,"lcd_name=")` → `sscanf(...,"lcd_name=%s",buf)` → `sprintf(path,"/lcds/%s",buf)`；
- `of_find_node_opts_by_path(path,0)`；
- `of_property_read_variable_u32_array(lcd_node,"sprd,dual-dsi-enable",&val,1,0)`；
- 成功：`*(u32 *)(ctx + 0xC20) = val`（即 `ctx.dual_dsi_en`）。

### 2.2 `dphy_s_glb_parse_dt / enable / disable / power_domain`

**这四个函数与 原厂 源码 `qogirn6pro/global_dphy.c` 逐行一致**（反编译完全对应，含 `udelay(100)` 的 `__const_udelay(0x68dbc)`）。也就是说 原厂 树里已经实现好了，只是没接线。为便于阅读，按 .ko 语义重列：

```c
static struct dphy_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
} s_ctx_enable, s_ctx_power;          /* .ko 里就是 global_dphy.c 的这两个 */

static int dphy_s_glb_parse_dt(struct dphy_context *ctx, struct device_node *np)
{
	unsigned int syscon_args[2];

	s_ctx_enable.regmap = syscon_regmap_lookup_by_phandle_args(np,
				"enable-syscon", 2, syscon_args);
	if (IS_ERR(s_ctx_enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
	} else {
		s_ctx_enable.ctrl_reg  = syscon_args[0];
		s_ctx_enable.ctrl_mask = syscon_args[1];
	}

	s_ctx_power.regmap = syscon_regmap_lookup_by_phandle_args(np,
				"power-syscon", 2, syscon_args);
	if (IS_ERR(s_ctx_power.regmap)) {
		pr_warn("failed to map dphy glb reg: power\n");
	} else {
		s_ctx_power.ctrl_reg  = syscon_args[0];
		s_ctx_power.ctrl_mask = syscon_args[1];
	}
	return 0;
}

static void dphy_s_glb_enable(struct dphy_context *ctx)
{	/* regmap_update_bits(reg, mask, mask) —— 置位 */
	regmap_update_bits(s_ctx_enable.regmap, s_ctx_enable.ctrl_reg,
			   s_ctx_enable.ctrl_mask, s_ctx_enable.ctrl_mask);
}

static void dphy_s_glb_disable(struct dphy_context *ctx)
{	/* regmap_update_bits(reg, mask, ~mask) —— 清位 */
	regmap_update_bits(s_ctx_enable.regmap, s_ctx_enable.ctrl_reg,
			   s_ctx_enable.ctrl_mask, (unsigned int)(~s_ctx_enable.ctrl_mask));
}

static void dphy_s_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(s_ctx_power.regmap, s_ctx_power.ctrl_reg,
				   s_ctx_power.ctrl_mask, (unsigned int)(~s_ctx_power.ctrl_mask));
		udelay(100);   /* .ko: __const_udelay(0x68dbc) == udelay(100) */
	} else {
		regmap_update_bits(s_ctx_power.regmap, s_ctx_power.ctrl_reg,
				   s_ctx_power.ctrl_mask, s_ctx_power.ctrl_mask);
	}
}

/* .ko 中该 ops 被引用：global_dphy.c:189-194 */
const struct dphy_glb_ops qogirn6pro_dphy_s_glb_ops = {
	.parse_dt = dphy_s_glb_parse_dt,
	.enable   = dphy_s_glb_enable,
	.disable  = dphy_s_glb_disable,
	.power    = dphy_s_power_domain,
};
```

> 反编译证据：`dphy_s_glb_parse_dt` 里 `syscon_regmap_lookup_by_phandle_args(np,"enable-syscon",2,...)` / `"power-syscon"`；`dphy_s_glb_enable` 只有一次 `regmap_update_bits(mask,mask)`；`dphy_s_power_domain` 有 `udelay(100)`。

### 2.3 DSI 侧 slave glb（`dsi_s_*`）——原厂 也已有

```c
static struct clk *clk_dsi1_eb;

static int dsi_s_glb_parse_dt(struct dsi_context *ctx, struct device_node *np)
{
	unsigned int syscon_args[2];

	pr_info("%s enter\n", __func__);
	clk_dsi1_eb = of_clk_get_by_name(np, "clk_dsi1_eb");
	if (IS_ERR(clk_dsi1_eb)) {
		pr_warn("read clk_dsi1_eb failed\n");
		clk_dsi1_eb = NULL;
	}
	s_ctx_reset.regmap = syscon_regmap_lookup_by_phandle_args(np, "reset", 2, syscon_args);
	if (IS_ERR(s_ctx_reset.regmap))
		pr_warn("failed to map dsi glb reg\n");
	else {
		s_ctx_reset.ctrl_reg  = syscon_args[0];
		s_ctx_reset.ctrl_mask = syscon_args[1];
	}
	return 0;
}

static void dsi_s_glb_enable(struct dsi_context *ctx)
{
	if (!clk_prepare_enable(clk_dsi1_eb))
		return;
	pr_err("enable clk_dsi1_eb failed!\n");
}
static void dsi_s_glb_disable(struct dsi_context *ctx)
{	clk_disable_unprepare(clk_dsi1_eb); }

static void dsi_s_reset(struct dsi_context *ctx)
{
	regmap_update_bits(s_ctx_reset.regmap, s_ctx_reset.ctrl_reg,
			   s_ctx_reset.ctrl_mask, s_ctx_reset.ctrl_mask);
	udelay(10);                    /* .ko: __const_udelay(0xa7c6) */
	regmap_update_bits(s_ctx_reset.regmap, s_ctx_reset.ctrl_reg,
			   s_ctx_reset.ctrl_mask, (unsigned int)(~s_ctx_reset.ctrl_mask));
}

const struct dsi_glb_ops qogirn6pro_dsi_s_glb_ops = {
	.parse_dt = dsi_s_glb_parse_dt,
	.reset    = dsi_s_reset,
	.enable   = dsi_s_glb_enable,
	.disable  = dsi_s_glb_disable,
};
```

（注：`regmap_update_bits()` 在 .ko 中展开为 `regmap_update_bits_base(...,0,0,0)`。）

---

## 3. master 如何拿并驱动 slave

### 3.1 DSI：phandle + `dev-id`，**不是**全局数组

`sprd_dsi_probe()`（Ghidra 伪码提取）：

```c
platform_set_drvdata(pdev, dsi);
if (dsi->ctx.id == 0) {                                   /* dev-id == 0 -> master */
	ret = sprd_dual_dsi_parse_dt(dsi, np);            /* 读 bootargs -> ctx.dual_dsi_en */
	if (ret) { drm_err("parse dual dsi info failed\n"); return ret; }

	if (dsi->ctx.dual_dsi_en) {
		struct device_node *s_np =
			of_parse_phandle(dsi->dev.of_node, "sprd,dual-channel", 0);
		if (s_np) {
			struct platform_device *s_pdev = of_find_device_by_node(s_np);
			struct sprd_dsi *s = platform_get_drvdata(s_pdev); /* pdev->dev.drvdata(+0xA8) */
			pr_info("find sprd,dual-channel\n");
			dsi->ctx.slave = s;                        /* master->ctx+0x98 */
			if (!s)
				return -EPROBE_DEFER;             /* -517 */
			s->ctx.master = dsi;                       /* slave->ctx+0x90 */
			s->ctx.dual_dsi_en = 1;                    /* slave->ctx+0xA0 */
		}
	}
	/* ... host_init / mutex / pm_runtime / component_add ... */
} else {
	drm_err("dsi slave skip other action\n");          /* dev-id==1：不做 host/drm 注册 */
	return 0;
}
```

要点：
- phandle 取自 **master 自己的 dsi 节点**（`dsi->dev.of_node`）的 `sprd,dual-channel`，指向 **slave dsi 节点**；
- `of_find_device_by_node()` 返 device，`platform_get_drvdata()` 拿 slave 的 `struct sprd_dsi`；
- slave 的 probe 在 `sysfs_init` / `platform_set_drvdata` 之后 **提前 return 0**，因此 slave 只完成 `ctx`/glb/regmap 的初始化，不注册 DSI host、不挂 encoder/connector；
- master 用 `-EPROBE_DEFER` 等待 slave 先 probe 完成。

### 3.2 参数同步：`sprd_dsi_host_attach()`

master 拿到 panel/panel 参数后，把整组参数复制进 slave ctx（偏移为 .ko 值，见 §4）：

```c
if (dsi->ctx.slave) {
	struct sprd_dsi *s = dsi->ctx.slave;
	pr_info("dsi_slave %s\n", /*【推测】dev_name(&s->dev) 或 lcd 名*/);
	s->ctx.lanes      = dsi->ctx.lanes;      /* +0x3B */
	s->ctx.format     = dsi->ctx.format;     /* +0x3C */
	s->ctx.work_mode  = dsi->ctx.work_mode;  /* +0x40 */
	s->ctx.burst_mode = dsi->ctx.burst_mode; /* +0x41 */
	s->ctx.nc_clk_en  = dsi->ctx.nc_clk_en;  /* +0x73 */
	s->ctx.byte_clk   = dsi->ctx.byte_clk;   /* +0x54 */
	s->ctx.esc_clk    = dsi->ctx.esc_clk;    /* +0x58 */
}
/* 同时在 phy_attach 之后：把 slave PHY 的 lanes/freq 也补齐 */
if (dsi->ctx.slave && dsi->phy->slave) {
	dsi->phy->slave->ctx.lanes = dsi->ctx.lanes;                  /* phy+0x3E8 */
	dsi->phy->slave->ctx.freq  = dsi->phy->ctx.freq;              /* phy+0x3E4 */
}
```

### 3.3 时钟/复位递归：`sprd_dsi_enable()` / `sprd_dsi_disable()`

反汇编块尾自调用（`bl e7f0 <sprd_dsi_enable>` / `bl cdf0 <sprd_dsi_disable>`），`x0` 在调用前被置为 `[dsi+0xC18]`（即 `ctx.slave`）：

```c
static void sprd_dsi_enable(struct sprd_dsi *dsi)
{
	if (dsi->glb->power)   dsi->glb->power(&dsi->ctx, true);
	if (dsi->glb->enable)  dsi->glb->enable(&dsi->ctx);
	if (dsi->glb->reset)   dsi->glb->reset(&dsi->ctx);
	sprd_dsi_init(dsi);
	if (dsi->ctx.work_mode == DSI_MODE_VIDEO)
		sprd_dsi_dpi_video(dsi);
	else
		sprd_dsi_edpi_video(dsi);

	if (dsi->ctx.slave)                 /* NEW: 递归驱动 slave */
		sprd_dsi_enable(dsi->ctx.slave);
}

static void sprd_dsi_disable(struct sprd_dsi *dsi)
{
	sprd_dsi_fini(dsi);                 /* .ko 里被内联进本函数 */
	if (dsi->glb->disable) dsi->glb->disable(&dsi->ctx);
	if (dsi->glb->power)   dsi->glb->power(&dsi->ctx, false);

	if (dsi->ctx.slave)                 /* NEW */
		sprd_dsi_disable(dsi->ctx.slave);
}
```

### 3.4 bind 阶段初始化 slave glb：`sprd_dsi_glb_init()`

```c
static int sprd_dsi_glb_init(struct sprd_dsi *dsi)
{
	if (dsi->glb->power)  dsi->glb->power(&dsi->ctx, true);
	if (dsi->glb->enable) dsi->glb->enable(&dsi->ctx);

	if (dsi->ctx.slave) {                     /* NEW */
		struct sprd_dsi *s = dsi->ctx.slave;
		pr_info("dsi_slave %s\n", ...);
		if (s->glb->power)  s->glb->power(&s->ctx, true);
		if (s->glb->enable) s->glb->enable(&s->ctx);
	}
	return 0;
}
```

### 3.5 encoder enable/disable：slave 的 mode/lp_cmd/nc_clk

原厂 的 `sprd_dsi_lp_cmd_enable / set_work_mode / state_reset / nc_clk_en` 是普通全局函数，但原厂编译时被**内联**进 `encoder_enable`（`readelf` 证：`encoder_enable` 无这些函数的 CALL26 重定位），所以反编译里出现了 master/slave 两段展开。源码级还原：

```c
/* sprd_dsi_encoder_enable() 中，master 之后追加： */
sprd_dsi_enable(dsi);
sprd_dphy_enable(dsi->phy);
sprd_dsi_lp_cmd_enable(dsi, true);
if (dsi->ctx.slave)
	sprd_dsi_lp_cmd_enable(dsi->ctx.slave, true);

/* ... panel prepare/enable ... */

sprd_dsi_set_work_mode(dsi, dsi->ctx.work_mode);
sprd_dsi_state_reset(dsi);
if (dsi->ctx.work_mode == DSI_MODE_VIDEO && dsi->ctx.video_lp_cmd_en)
	sprd_dsi_lp_cmd_enable(dsi, true);
if (dsi->ctx.nc_clk_en)
	sprd_dsi_nc_clk_en(dsi, true);
else
	sprd_dphy_hs_clk_en(dsi->phy, true);

if (dsi->ctx.slave) {                       /* NEW */
	struct sprd_dsi *s = dsi->ctx.slave;
	sprd_dsi_set_work_mode(s, s->ctx.work_mode);
	sprd_dsi_state_reset(s);
	if (s->ctx.nc_clk_en)
		sprd_dsi_nc_clk_en(s, true);
	else
		sprd_dphy_hs_clk_en(s->phy, true);
}
```

`encoder_disable` 的 slave 处理（原厂在 master 之后镜像一份）：

```c
sprd_dsi_set_work_mode(dsi, DSI_MODE_CMD);
sprd_dsi_lp_cmd_enable(dsi, true);
if (dsi->ctx.slave) {                       /* NEW */
	sprd_dsi_set_work_mode(dsi->ctx.slave, DSI_MODE_CMD);
	sprd_dsi_lp_cmd_enable(dsi->ctx.slave, true);
}
/* ... panel disable/unprepare ... */
sprd_dphy_disable(dsi->phy);
sprd_dsi_disable(dsi);
```

### 3.6 DPHY：独立的一套 master/slave

`sprd_dphy_probe()` 同样读 `sprd,dual-channel`，但**触发条件是 master DSI 的 `ctx.dual_dsi_en`**（它通过 of_graph input 拿到 dsi 设备）：

```c
/* sprd_dphy_probe() 尾部，platform_set_drvdata 之后 */
if (dsi->ctx.dual_dsi_en) {
	struct device_node *s_np =
		of_parse_phandle(dphy->dev.of_node, "sprd,dual-channel", 0);
	if (s_np) {
		struct platform_device *s_pdev = of_find_device_by_node(s_np);
		struct sprd_dphy *s = platform_get_drvdata(s_pdev);
		pr_info("find sprd,dual-channel\n");
		dphy->slave = s;                     /* master_phy+0x410 */
		if (!s)
			return -EPROBE_DEFER;        /* -517 */
		s->master = dphy;                    /* slave_phy+0x408 */
	}
}
```

`sprd_dphy_enable()` 递归 enable slave：

```c
if (dphy->glb->power)  dphy->glb->power(&dphy->ctx, true);
if (dphy->glb->enable) dphy->glb->enable(&dphy->ctx);
ret = sprd_dphy_init(dphy);
if (ret) { drm_err("sprd dphy init failed\n"); return -EINVAL; }

if (dphy->slave) {                          /* NEW */
	struct sprd_dphy *s = dphy->slave;
	if (s->glb->power)  s->glb->power(&s->ctx, true);
	if (s->glb->enable) s->glb->enable(&s->ctx);
	ret = sprd_dphy_init(s);
	if (ret) { drm_err("sprd dphy slave init failed\n"); return -EINVAL; }
}
dphy->ctx.enabled = true;
```

`sprd_dphy_disable()` 对 slave 先做 PPI 复位序列（`rstz=0` → `shutdownz=0` → `rstz=1`），再 glb disable + power off：

```c
/* master: glb->disable(ctx); glb->power(ctx,0);  然后： */
if (dphy->slave) {                          /* NEW */
	struct sprd_dphy *s = dphy->slave;
	if (s->ppi->rstz)      s->ppi->rstz(&s->ctx, 0);
	if (s->ppi->shutdownz) s->ppi->shutdownz(&s->ctx, 0);
	if (s->ppi->rstz)      s->ppi->rstz(&s->ctx, 1);
	if (s->glb->disable)   s->glb->disable(&s->ctx);
	if (s->glb->power)     s->glb->power(&s->ctx, false);
}
```

### 3.7 端到端时序（master dsi0 / dphy0）

```
[DT]  dsi0 --sprd,dual-channel--> dsi1      dphy0 --sprd,dual-channel--> dphy1
      (dev-id=0)                   (dev-id=1)  (dev-id=0)                  (dev-id=1)

probe(dsi1)  : context_init(dsi_s_glb_parse_dt: clk_dsi1_eb, reset syscon)
               device_create, sysfs, platform_set_drvdata
               id!=0 -> "dsi slave skip other action" -> return 0
probe(dsi0)  : context_init(dsi_glb_parse_dt)
               sprd_dual_dsi_parse_dt(): bootargs lcd_name -> sprd,dual-dsi-enable -> dual_dsi_en
               of_parse_phandle(sprd,dual-channel) -> dsi1 -> ctx.slave
               dsi1.ctx.master=dsi0; dsi1.ctx.dual_dsi_en=1
               host_register / component_add
probe(dphy1) : context_init(dphy_s_glb_parse_dt: enable/power syscon)
probe(dphy0) : context_init(dphy_glb_parse_dt)
               dsi0.ctx.dual_dsi_en -> of_parse_phandle(sprd,dual-channel) -> dphy1
               phy0.slave=dphy1; dphy1.master=phy0
bind(dsi0)   : glb_init(master) + glb_init(slave dsi1)   [clk_dsi0_eb / clk_dsi1_eb]
host_attach  : 同步 lanes/format/work_mode/burst_mode/nc_clk/byte_clk/esc_clk -> dsi1
encoder_enable: sprd_dsi_enable(master->slave)  ->  dphy_enable(master->slave)
                lp_cmd / set_work_mode / state_reset / nc_clk(master+slave)
```

---

## 4. 结构体新增字段（字段 / 类型 / 偏移）

偏移取自 **原厂 .ko ABI**。改 原厂 源码重新编译时不要求偏移一致，按字段名加即可（会在编译时自动重排）。

### 4.1 `struct dsi_context`（`sprd_dsi.h`）

| 偏移 | 类型 | 字段 | 说明 |
|---:|---|---|---|
| +0x38 | `bool` | `enabled` | 原有 |
| **+0x39** | **`u8`** | **`id`** | **新增**；DT `dev-id`，0=master，1=slave |
| +0x3A | `u8` | `channel` | 原有 |
| +0x3B | `u8` | `lanes` | 原有 |
| +0x3C | `u32` | `format` | 原有 |
| ... | | | 原有 timing/int_mask 等（+0x44 irq0、+0x48 irq1、+0x4C int0_mask、+0x50 int1_mask、+0x54 byte_clk、+0x58 esc_clk、+0x5C~0x62 data/clk hs-lp、+0x68 max_rd_time、+0x73 nc_clk_en、+0x74 dpi_clk_div、+0x78 clk_dpi_384m、+0x79 video_lp_cmd_en、+0x7C dpms、+0x80 last_dpms） |
| +0x88 | `const char *` | `lcd_name` | 原有 |
| **+0x90** | **`struct sprd_dsi *`** | **`master`** | **新增**；slave 指回 master |
| **+0x98** | **`struct sprd_dsi *`** | **`slave`** | **新增**；master 指向 slave |
| **+0xA0** | **`u32`** | **`dual_dsi_en`** | **新增**；由 `sprd_dual_dsi_parse_dt` 写入，phy probe 读取 |
| +0xA4 | — | padding | —（`dsi_context` 总大小 0xB0） |

> 【推测】+0x7A/+0x7B 为 `hporch_lp_disable`/`surface_mode`/`is_esd_rst` 的排布，原厂相对 原厂 略有差异；重编译按 原厂 原名保留即可，不影响接线。

**`struct sprd_dsi`**（`sprd_dsi.h`）总大小 **0xC30**，内嵌 `ctx` 起始于 **+0xB80**；索引不变，无需新增字段（master/slave 指针放在 `ctx` 内）。

### 4.2 `struct sprd_dphy`（`sprd_dphy.h`）

| 偏移 | 类型 | 字段 | 说明 |
|---:|---|---|---|
| +0x000 | `struct device` | `dev` | 原有（大小 0x3A8） |
| +0x3A8 | `struct dphy_context` | `ctx` | 原有（大小 0x48，**无新增字段**） |
| +0x3F0 | `const struct dphy_ppi_ops *` | `ppi` | 原有 |
| +0x3F8 | `const struct dphy_pll_ops *` | `pll` | 原有 |
| +0x400 | `const struct dphy_glb_ops *` | `glb` | 原有 |
| **+0x408** | **`struct sprd_dphy *`** | **`master`** | **新增**；slave 指回 master |
| **+0x410** | **`struct sprd_dphy *`** | **`slave`** | **新增**；master 指向 slave |
| — | — | 总大小 **0x418** | — |

---

## 5. 原厂 源码 patch 方案

> 现状：`global_dsi.c` / `global_dphy.c` 的 `dsi_s_*` / `dphy_s_*` 及两个 `*_s_glb_ops` **已存在**（HEAD 未改动）；父 agent 已给 `sprd_dsi.c/.h` 加了 `qogirn6pro_dsi_s` + match、给 `sprd_dphy.c` 加了 `dsi1-phy` match（但映射错），并加了 DTS。下面给出**完整可编译**的改动。

### 5.1 `sprd_dsi.h`

```c
struct sprd_dsi;   /* 前置声明，放在 struct dsi_context 之前 */

struct dsi_context {
	...
	bool enabled;
	u8 id;                 /* NEW: dev-id */
	u8 channel;
	...
	const char *lcd_name;
	struct sprd_dsi *master;   /* NEW */
	struct sprd_dsi *slave;    /* NEW */
	u32 dual_dsi_en;           /* NEW */
};
...
extern const struct dsi_glb_ops qogirn6pro_dsi_s_glb_ops;   /* 已由父 agent 添加 */
/* 新增函数原型 */
int sprd_dual_dsi_parse_dt(struct sprd_dsi *dsi, struct device_node *np);
```

### 5.2 `sprd_dsi.c`

**(a) 新增 `sprd_dual_dsi_parse_dt()`**：见 §2.1（放在 `sprd_dsi_probe` 之前）。

**(b) `sprd_dsi_context_init()`**：在 `of_address_to_resource` 之后加

```c
if (!of_property_read_u32(np, "dev-id", &tmp))
	ctx->id = tmp;
```

**(c) `sprd_dsi_device_create()`**：建议把名字改成带 id

```c
dev_set_name(&dsi->dev, "dsi%d", dsi->ctx.id);
```

**(d) `sprd_dsi_probe()`**：在 `platform_set_drvdata(pdev, dsi);` 之后、`sprd_dsi_host_init()` 之前插入：

```c
if (dsi->ctx.id == 0) {                       /* master */
	ret = sprd_dual_dsi_parse_dt(dsi, np);
	if (ret) {
		DRM_ERROR("parse dual dsi info failed\n");
		return ret;
	}
	if (dsi->ctx.dual_dsi_en) {
		struct device_node *s_np =
			of_parse_phandle(dsi->dev.of_node, "sprd,dual-channel", 0);
		if (s_np) {
			struct platform_device *s_pdev = of_find_device_by_node(s_np);
			struct sprd_dsi *s = s_pdev ? platform_get_drvdata(s_pdev) : NULL;
			DRM_INFO("find sprd,dual-channel\n");
			dsi->ctx.slave = s;
			if (!s)
				return -EPROBE_DEFER;
			s->ctx.master = dsi;
			s->ctx.dual_dsi_en = 1;
		}
	}
} else {                                      /* slave */
	DRM_ERROR("dsi slave skip other action\n");
	return 0;
}
```

**(e) `sprd_dsi_glb_init()`**：加 slave 分支（见 §3.4）。

**(f) `sprd_dsi_enable()` / `sprd_dsi_disable()`**：末尾各加 slave 递归（见 §3.3）。

**(g) `sprd_dsi_host_attach()`**：在 `ctx->lcd_name = lcd_node->name;` 之后（或函数末尾）加 slave 参数同步（见 §3.2）。注意 phy 部分用 `dsi->phy->slave`。

**(h) `sprd_dsi_encoder_enable()`**：按 §3.5 在 master 的 lp_cmd / set_work_mode / state_reset / nc_clk 之后镜像一份给 slave。

**(i) `sprd_dsi_encoder_disable()`**：按 §3.5 在 master 的 set_work_mode + lp_cmd 之后镜像一份给 slave。

### 5.3 `sprd_dphy.h`

```c
/* 末尾新增 extern */
extern const struct dphy_glb_ops qogirn6pro_dphy_s_glb_ops;

struct sprd_dphy {
	struct device dev;
	struct dphy_context ctx;
	const struct dphy_ppi_ops *ppi;
	const struct dphy_pll_ops *pll;
	const struct dphy_glb_ops *glb;
	struct sprd_dphy *master;   /* NEW +0x408 */
	struct sprd_dphy *slave;    /* NEW +0x410 */
};
```

### 5.4 `sprd_dphy.c`

**(a) 新增 slave ops 结构体**（放在 `qogirn6pro_dphy` 之后）：

```c
static const struct sprd_dphy_ops qogirn6pro_dphy_s = {
	.ppi = &dsi_ctrl_ppi_ops,
	.pll = &sharkl5_dphy_pll_ops,
	.glb = &qogirn6pro_dphy_s_glb_ops,
};
```

**(b) 修正 match 表**：把父 agent 写的 `.data = &qogirn6pro_dphy` 改为 slave：

```c
{ .compatible = "sprd,qogirn6pro-dsi1-phy",
  .data = &qogirn6pro_dphy_s },     /* 不能复用 master 的 glb，否则覆盖 master 的 syscon 全局变量 */
```

**(c) `sprd_dphy_probe()`**：保留 `dsi_dev = sprd_disp_pipe_get_input(&pdev->dev);` 的指针，在 `platform_set_drvdata(pdev, dphy);` 之后加：

```c
{
	struct sprd_dsi *dsi = dev_get_drvdata(dsi_dev);   /* 需要 #include "sprd_dsi.h" */
	if (dsi && dsi->ctx.dual_dsi_en) {
		struct device_node *s_np =
			of_parse_phandle(pdev->dev.of_node, "sprd,dual-channel", 0);
		if (s_np) {
			struct platform_device *s_pdev = of_find_device_by_node(s_np);
			struct sprd_dphy *s = s_pdev ? platform_get_drvdata(s_pdev) : NULL;
			DRM_INFO("find sprd,dual-channel\n");
			dphy->slave = s;
			if (!s)
				return -EPROBE_DEFER;
			s->master = dphy;
		}
	}
}
```

> `sprd_dphy.c` 目前未 include `sprd_dsi.h`，需补上（或只前置声明 + 用 `dev_get_drvdata` 拿 `dsi->ctx.dual_dsi_en` 字段偏移不可移植，故建议 include）。

**(d) `sprd_dphy_enable()` / `sprd_dphy_disable()`**：见 §3.6。

### 5.5 `qogirn6pro/global_dsi.c` 与 `global_dphy.c`

**无需改动**：`dsi_s_glb_parse_dt/enable/disable/reset`、`dphy_s_glb_parse_dt/enable/disable`、`dphy_s_power_domain` 及两个 `*_s_glb_ops` 已在 HEAD 中完整存在，且与原厂 .ko 逐行一致。唯一要保证的是它们被 `sprd_dsi.c/sprd_dphy.c` 的 match 表引用到（§5.2/§5.4）。

### 5.6 DTS（父 agent 已加，这里给校验点）

master：
```
dsi: dsi@31300000  { compatible="sprd,qogirn6pro-dsi-host";  /* dev-id 缺省=0 */
                     sprd,dual-channel = <&dsi1>; };
dphy: dphy         { compatible="sprd,qogirn6pro-dsi-phy";   /* dev-id 缺省=0 */
                     sprd,dual-channel = <&dphy1>; };
```
slave：
```
dsi1: dsi1@31400000 { compatible="sprd,qogirn6pro-dsi1-host"; dev-id = <1>;
                      reset = <... DSI1_SOFT_RST>; syscon-names = "reset";
                      clocks=<&dpu_vsp_gate CLK_DSI1_EB>; clock-names="clk_dsi1_eb";
                      ports { port@0 { dsi1_out: endpoint { remote-endpoint=<&dphy1_in>; }; }; }; };
dphy1: dphy1        { compatible="sprd,qogirn6pro-dsi1-phy"; dev-id = <1>;
                      enable-syscon = <... DPHY1_REF_CKG_EN|DPHY1_CFG_CKG_EN>;
                      power-syscon  = <... DSI1_PD_REG>;
                      syscon-names = "enable","power";
                      port@1 { dphy1_in: endpoint { remote-endpoint=<&dsi1_out>; }; }; };
```

**待确认**：父 agent 的 `dphy1` 节点只有 `reg` 一项资源，而 `sprd_dphy_context_init()` 会尝试 `of_address_to_resource(np, 1, ...)` 作为 apbbase —— 失败仅打印 "this dphy has apb reg base" 前的判断会走 else（不致命，返回 0），slave PHY 无 APB 口是合理的。

---

## 6. 推测与不确定项

1. **【推测】** `encoder_enable` 中 slave 只做 `set_work_mode/state_reset/nc_clk`，**没有**重复 `video_lp_cmd_enable`（反编译块 0x1081–0x1123 未见该调用）；slave 的 lp_cmd 只在前面无条件 enable 一次。
2. **【推测】** `sprd_dphy_disable` 对 slave 的 PPI 复位序列顺序为 `rstz(0) → shutdownz(0) → rstz(1)`；反编译明确看到对 `slave+0x3F0`(ppi) 的 3 次调用，但成员名按 `struct dphy_ppi_ops` 前两个（rstz/shutdownz）推断。
3. **【推测】** `dsi_slave %s` 的第二个 `%s` 实参：Ghidra 显示传入的是 `__func__`（`sprd_dsi_glb_init`/`sprd_dsi_host_attach`），疑为寄存器传播误判；源码意图应为 slave 设备/LCD 名。
4. **probe 顺序依赖**：`sprd_dphy_probe()` 直接解引用 `dsi->ctx.dual_dsi_en`（未判空）。若 dphy 先于 dsi probe，会命中空指针。原厂依赖 probe 顺序；**建议**在 patch 时加 `if (dsi && ...)` 判空（本报告 §5.4 已加）。
5. `ctx+0x7A/+0x7B` 的 `surface_mode/is_esd_rst` 精确顺序未逐字节确认；重编译按 原厂 原名保留即可。
6. `.ko` 无 BTF/DWARF，结构体偏移均由反编译访存与 `readelf -rW` 重定位交叉验证得到；已在 §4 标注确认项。

---

## 7. 证据附录

### 7.1 关键反编译片段（原始）

```c
/* sprd_dsi_probe 双通道分支 */
iVar6 = sprd_dual_dsi_parse_dt(lVar7,uVar10);
if (iVar6 == 0) {
  if ((*(int *)(lVar7 + 0xc20) != 0) &&
      (lVar9 = of_parse_phandle(*(undefined8 *)(lVar7 + 0x300),"sprd,dual-channel",0), lVar9 != 0)) {
    printk(&DAT_0010f0a8);                       /* "[drm] find sprd,dual-channel" */
    lVar9 = of_find_device_by_node(lVar9);
    lVar9 = *(long *)(lVar9 + 0xa8);             /* platform drvdata = slave dsi */
    *(long *)(lVar7 + 0xc18) = lVar9;            /* master->ctx.slave */
    if (lVar9 == 0) return -0x205;               /* -EPROBE_DEFER */
    *(long *)(lVar9 + 0xc10) = lVar7;            /* slave->ctx.master */
    *(undefined4 *)(*(long *)(lVar7 + 0xc18) + 0xc20) = 1;  /* slave.dual_dsi_en=1 */
  }
  ...
} else {
  drm_err("parse dual dsi info failed\n");
}
```

```c
/* sprd_dphy_probe 双通道分支 */
*(long *)(param_1 + 0xa8) = lVar3;               /* platform_set_drvdata */
if ((*(int *)(*(long *)(lVar4 + 0x98) + 0xc20) != 0) &&     /* dsi->ctx.dual_dsi_en */
    (lVar7 = of_parse_phandle(*(undefined8 *)(lVar3 + 0x300),"sprd,dual-channel",0), lVar7 != 0)) {
  printk(&DAT_0010f0a8);
  lVar7 = of_find_device_by_node(lVar7);
  lVar7 = *(long *)(lVar7 + 0xa8);
  *(long *)(lVar3 + 0x410) = lVar7;              /* dphy->slave */
  if (lVar7 == 0) { uVar8 = 0xfffffdfb; goto ...; }   /* -EPROBE_DEFER */
  *(long *)(lVar7 + 0x408) = lVar3;              /* slave->master */
}
```

```c
/* sprd_dphy_enable slave 分支 */
if (iVar2 == 0) {
  lVar7 = *(long *)(param_1 + 0x410);            /* dphy->slave */
  if (lVar7 != 0) {
    lVar6 = *(long *)(lVar7 + 0x400);            /* slave->glb */
    if (lVar6 != 0) {
      pcVar5 = *(code **)(lVar6 + 0x18);         /* glb->power */
      if (pcVar5) { (*pcVar5)(lVar7 + 0x3a8,1); ... }
      pcVar5 = *(code **)(lVar6 + 8);            /* glb->enable */
      if (pcVar5) { (*pcVar5)(lVar7 + 0x3a8); }
    }
    iVar2 = sprd_dphy_init(lVar7);
    if (iVar2 != 0) { mutex_unlock(lVar1); pcVar3 = "sprd dphy slave init failed\n"; ... }
  }
  *(undefined1 *)(param_1 + 0x3e0) = 1;          /* dphy->ctx.enabled = true */
}
```

### 7.2 递归调用的反汇编证据

```
# sprd_dsi_enable @0xe7f0 末尾：
e888: mov  x0, x19                  ; x19 = dsi
e88c: bl   sprd_dsi_init
e890: ldrb w8, [x19, #3008]         ; ctx.work_mode (+0xBC0)
...
e8a4: ldr  x0, [x19, #3096]         ; x0 = ctx.slave (+0xC18)
e8a8: cbnz x0, e8bc
e8bc: bl   e7f0 <sprd_dsi_enable>   ; 递归 sprd_dsi_enable(dsi->ctx.slave)

# sprd_dsi_disable @0xcdf0 末尾：
cee8: ldr  x0, [x19, #3096]         ; ctx.slave
ceec: cbz  x0, cef4
cef0: bl   cdf0 <sprd_dsi_disable>  ; 递归 sprd_dsi_disable(dsi->ctx.slave)
```

### 7.3 复现命令

```bash
export JAVA_HOME=/usr/lib/java-21
cd /home/misakapardola/leaking-iflytek/tcakernel/re-ghidra
/usr/lib/ghidra/support/analyzeHeadless . proj \
  -import /home/misakapardola/leaking-iflytek/tcakernel/vendor_rd/lib/modules/sprd-drm.ko
/usr/lib/ghidra/support/analyzeHeadless . proj -process sprd-drm.ko -noanalysis \
  -scriptPath scripts -postScript DumpDecomp.java sprd_dual_dsi_parse_dt sprd_dsi_probe ...
```
产物：`re-ghidra/dump1.clean.txt`、`dump2.clean.txt`、`strref.txt`、`text.asm`。

---

*报告结束。所有偏移/字符串/调用目标均来自对未 strip 模块的静态逆向；带【推测】处请以在目标机上实测为准。*
