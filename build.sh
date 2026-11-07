#!/usr/bin/env bash
#
# build.sh — iFlytek UMS9620 (se50 / 2H10) 内核编译脚本
#
# 功能:
#   * 自动生成配置 (Kconfig -> .config)
#   * 默认【只编内核 Image】——配合「全原厂模块 + 原厂 DT」方案，不编自编模块/DT
#   * 全量写入 logs/build-<时间戳>.log，logs/build-latest.log 指向最新
#   * 任一步报错/超时，自动打印「错误摘要 + 末尾 80 行」
#
# 用法:
#   ./build.sh                       # 默认: olddefconfig + Image（不编模块/DT）
#   DO_MODULES=1 ./build.sh          # 额外编译内核模块
#   DO_DT=1 DO_DTBO=1 ./build.sh     # 额外编 se50 base DTB + overlay DTBO
#   DO_KERNEL=0 DO_DT=1 ./build.sh   # 只编设备树
#   DTB=... DTO=... ./build.sh       # 换设备树目标
#   CLEAN=1 ./build.sh               # 先 make clean 再编（保留 .config）
#   CLEAN=mrproper ./build.sh        # 全清（含 .config，会重新生成）
#   NO_CONFIG=1 ./build.sh           # 跳过配置步骤
#   MAKE_TIMEOUT=3600 JOBS=8 ./build.sh
#
set -uo pipefail

# ---------------------------------------------------------------- 配置
WORKDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${SRC:-$WORKDIR/kernel_ums9620_se50}"
OBJ="${OBJ:-$WORKDIR/build/se50-clang-out}"
# 需要 Android Clang r416183b（LLVM 12.0.5）。放到任意位置后用 CLANG_BIN 指定：
#   CLANG_BIN=/opt/clang-r416183b/bin ./build.sh
CLANG_BIN="${CLANG_BIN:-$HOME/clang-r416183b/bin}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
JOBS="${JOBS:-$(nproc)}"
MAKE_TIMEOUT="${MAKE_TIMEOUT:-7200}"
BSP_BUILD_FAMILY="${BSP_BUILD_FAMILY:-qogirn6pro}"
DEFCONFIG="${DEFCONFIG:-f50_defconfig}"
LOG_DIR="${LOG_DIR:-$WORKDIR/logs}"
NO_CONFIG="${NO_CONFIG:-0}"
# LTO 模式（默认 thin）: thin(ThinLTO,快/省内存,保留CFI) | full(完整LTO,慢/吃内存) | keep(不改)
# 警告: 不要用 none(关 LTO/CFI)！74 个原厂模块 import __cfi_slowpath，关了会加载失败。
LTO="${LTO:-thin}"
# 清理: 0(不清理) | clean(make clean,保留 .config) | mrproper(全清,含 .config)
CLEAN="${CLEAN:-0}"

DO_KERNEL="${DO_KERNEL:-1}"     # 内核 Image（默认编）
DO_MODULES="${DO_MODULES:-0}"   # 自编模块（默认不编，用全原厂模块）
DO_DT="${DO_DT:-0}"             # base DTB（默认不编，用原厂 DTB）
DO_DTBO="${DO_DTBO:-0}"         # overlay DTBO（默认不编，用原厂 DTBO）

# se50 (=UMS9620 2H10) 的单个设备树：base + overlay
# DT 目标是相对 arch/arm64/boot/dts 的路径（顶层 %.dtb 规则会自动再拼 $(dtstree)/）
DTB="${DTB:-sprd/ums9620.dtb}"
DTO="${DTO:-sprd/ums9620-2h10-overlay.dtbo}"
OUT_DT="${OUT_DT:-$WORKDIR/out/dt}"

# ---------------------------------------------------------------- 日志
mkdir -p "$LOG_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/build-$STAMP.log"
ln -sfn "$(basename "$LOG")" "$LOG_DIR/build-latest.log"

# ---------------------------------------------------------------- 工具链 / make
export PATH="$CLANG_BIN:$PATH"

if [ ! -x "$CLANG_BIN/clang" ]; then
    echo "错误: 找不到 $CLANG_BIN/clang" >&2
    echo "本内核需要 Android Clang r416183b (LLVM 12.0.5)，下载后：" >&2
    echo "  CLANG_BIN=/path/to/clang-r416183b/bin ./build.sh" >&2
    exit 1
fi

# scripts/setlocalversion 只在 LOCALVERSION 未定义时给脏树追加 '+'.
# 设定成空值可保证 uname -r 精确为 5.4.210-tca-cycle (无尾部 +).
export LOCALVERSION=

MAKE=(make -C "$SRC" O="$OBJ" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
      CROSS_COMPILE="$CROSS_COMPILE" KCFLAGS="-Wno-error" \
      BSP_BUILD_FAMILY="$BSP_BUILD_FAMILY" -j"$JOBS")
MAKE_OVL=(make -C "$SRC" O="$OBJ" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
      CROSS_COMPILE="$CROSS_COMPILE" KCFLAGS="-Wno-error" \
      BSP_BUILD_FAMILY="$BSP_BUILD_FAMILY" BSP_BUILD_DT_OVERLAY=y BSP_BUILD_ANDROID_OS=y -j"$JOBS")

# ---------------------------------------------------------------- 失败处理
fail() {
    local reason="$1"
    {
        echo ""
        echo "======================= BUILD FAILED ======================="
        echo "原因 : $reason"
        echo "日志 : $LOG"
        echo "----------------------- 错误摘要 ---------------------------"
        grep -inE "error:|fatal error|undefined reference|No rule to make target|recipe for target .* failed|错误|Error [0-9]+" "$LOG" | tail -50
        echo "----------------------- 末尾 80 行 -------------------------"
        tail -80 "$LOG"
        echo "------------------------------------------------------------"
        echo "提示 : 完整日志见 $LOG"
        echo "============================================================"
    } | tee -a "$LOG"
    exit 1
}

run() {
    local name="$1"; shift
    echo "==> $name" | tee -a "$LOG"
    if [ "$MAKE_TIMEOUT" != "0" ]; then
        timeout "$MAKE_TIMEOUT" stdbuf -oL -eL "$@" 2>&1 | tee -a "$LOG" || fail "$name (exit/timeout)"
    else
        stdbuf -oL -eL "$@" 2>&1 | tee -a "$LOG" || fail "$name"
    fi
}

# ---------------------------------------------------------------- 开始
{
    echo "============================================================"
    echo " se50 kernel build   $(date)"
    echo " 源码     : $SRC"
    echo " 输出     : $OBJ"
    echo " 工具链   : $CLANG_BIN"
    echo " 内核     : $([ "$DO_KERNEL"  = 1 ] && echo Image || echo '-')"
    echo " 模块     : $([ "$DO_MODULES" = 1 ] && echo 'yes' || echo 'no (用原厂模块)')"
    echo " base DT  : $([ "$DO_DT"   = 1 ] && echo "$DTB" || echo '- (用原厂 DTB)')"
    echo " overlay  : $([ "$DO_DTBO" = 1 ] && echo "$DTO" || echo '- (用原厂 DTBO)')"
    echo " LTO      : $LTO"
    echo " 清理     : $CLEAN"
    echo " 并行     : $JOBS     超时: $MAKE_TIMEOUT s"
    echo "============================================================"
} | tee -a "$LOG"

[ -d "$SRC" ] || fail "源码目录不存在: $SRC"

if [ "$CLEAN" != "0" ]; then
    case "$CLEAN" in
      1|clean)  CLEAN_TARGET="clean" ;;
      mrproper) CLEAN_TARGET="mrproper" ;;
      *) fail "CLEAN 取值无效: $CLEAN (0|1|clean|mrproper)" ;;
    esac
    run "[0] make $CLEAN_TARGET" "${MAKE[@]}" "$CLEAN_TARGET"
fi

if [ "$NO_CONFIG" != "1" ]; then
    if [ ! -f "$OBJ/.config" ]; then
        run "[1] 生成配置: $DEFCONFIG" "${MAKE[@]}" "$DEFCONFIG"
    fi
    if [ "$LTO" != "keep" ]; then
        case "$LTO" in
          thin) "${SRC}/scripts/config" --file "$OBJ/.config" --disable LTO_NONE --enable LTO_CLANG  --enable  THINLTO ;;
          none) echo "!! 警告: none 会关掉 CFI，74 个原厂模块将因缺 __cfi_slowpath 而加载失败 !!" ; "${SRC}/scripts/config" --file "$OBJ/.config" --enable  LTO_NONE --disable LTO_CLANG --disable THINLTO ;;
          full) "${SRC}/scripts/config" --file "$OBJ/.config" --disable LTO_NONE --enable  LTO_CLANG --disable THINLTO ;;
          *) fail "LTO 取值无效: $LTO (keep|thin|none|full)" ;;
        esac
    fi
    run "[1] olddefconfig (LTO=$LTO)" "${MAKE[@]}" olddefconfig
fi

if [ "$#" -gt 0 ]; then
    run "[*] make $*" "${MAKE[@]}" "$@"
else
    [ "$DO_KERNEL"  = "1" ] && run "[2] 内核 Image"        "${MAKE[@]}" Image
    [ "$DO_MODULES" = "1" ] && run "[3] 内核模块 (自编)"     "${MAKE[@]}" modules
    [ "$DO_DT"      = "1" ] && run "[4] 设备树 base: $DTB"  "${MAKE[@]}" "$DTB"
    if [ "$DO_DTBO" = "1" ]; then
        # 内核树没有 %.dtbo 规则：先 cpp 预处理，再用内核自带 dtc 编 overlay
        run "[5a] 构建 dtc" "${MAKE[@]}" scripts_dtc
        mkdir -p "$OUT_DT"
        ov_dts="$SRC/arch/arm64/boot/dts/${DTO%.dtbo}.dts"
        ov_tmp="$(mktemp)"
        "${CLANG_BIN}/clang" -E -nostdinc \
            -I"$SRC/scripts/dtc/include-prefixes" \
            -undef -D__DTS__ -x assembler-with-cpp \
            -o "$ov_tmp" "$ov_dts" || fail "overlay 预处理失败: $ov_dts"
        "$OBJ/scripts/dtc/dtc" -@ -I dts -O dtb -q \
            -o "$OUT_DT/$(basename "$DTO")" "$ov_tmp" \
            || fail "overlay 编译失败: $DTO"
        rm -f "$ov_tmp"
        echo "  overlay -> $OUT_DT/$(basename "$DTO")"
    fi
fi

if [ "$DO_DT" = "1" ] || [ "$DO_DTBO" = "1" ]; then
    mkdir -p "$OUT_DT"
    if [ "$DO_DT" = "1" ] && [ -f "$OBJ/arch/arm64/boot/dts/$DTB" ]; then
        cp -f "$OBJ/arch/arm64/boot/dts/$DTB" "$OUT_DT/"
    fi
fi

{
    echo ""
    echo "======================= BUILD OK ==========================="
    ls -la "$OBJ/arch/arm64/boot/Image" 2>/dev/null | awk '{printf "Image : %s  %s %s %s\n", $5, $6, $7, $8}'
    if [ "$DO_DT" = "1" ] || [ "$DO_DTBO" = "1" ]; then
        echo "DT    : $OUT_DT"
        ls -la "$OUT_DT" 2>/dev/null | tail -n +2 | sed 's/^/        /'
    fi
    if [ "$DO_MODULES" = "1" ]; then
        echo "模块  : $(find "$OBJ" -name '*.ko' -newer "$LOG" 2>/dev/null | wc -l) 个新编译"
    fi
    echo "日志  : $LOG"
    echo "============================================================"
} | tee -a "$LOG"
