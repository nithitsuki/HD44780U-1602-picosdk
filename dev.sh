#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------------
# dev.sh — Build & upload helper for HD44780 pico-sdk project
#
# Usage:
#   ./dev.sh                     Build all + upload hello_lcd.
#   ./dev.sh build               Build all targets.
#   ./dev.sh upload [target]     Flash target.elf (default: hello_lcd).
#   ./dev.sh clean               Remove build directory.
#   ./dev.sh rebuild [target]    clean + build + upload [target].
#
# OpenOCD configuration (set these env vars or edit defaults below):
#   OPENOCD_INTERFACE     Path to the adapter interface config
#                         (default: interface/cmsis-dap.cfg)
#   OPENOCD_TARGET        Path to the target config
#                         (default: target/rp2040.cfg)
#   OPENOCD               OpenOCD binary path
#                         (default: openocd)
# ------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
ELF_FILE="$BUILD_DIR/hello_lcd.elf"

# ── OpenOCD defaults (override via environment) ───────────────────
: "${OPENOCD:=openocd}"
: "${OPENOCD_INTERFACE:=interface/cmsis-dap.cfg}"
: "${OPENOCD_TARGET:=target/rp2040.cfg}"

# ── Build ──────────────────────────────────────────────────────────
cmd_build() {
    echo "── Building ──────────────────────────────────────────"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    # Build all targets so any of them can be uploaded later.
    make -C "$BUILD_DIR" -j"$(nproc)"
    echo "✔  Build OK — all targets in $BUILD_DIR"
}

# ── Upload via OpenOCD ────────────────────────────────────────────
cmd_upload() {
    if [ ! -f "$ELF_FILE" ]; then
        echo "✖  No .elf found — run '${0##*/} build' first." >&2
        return 1
    fi

    echo "── Flashing via OpenOCD ──────────────────────────────"
    echo "  Interface : $OPENOCD_INTERFACE"
    echo "  Target    : $OPENOCD_TARGET"
    echo "  ELF       : $ELF_FILE"
    echo

    echo "  Running: $OPENOCD \\"
    echo "    -f $OPENOCD_INTERFACE \\"
    echo "    -f $OPENOCD_TARGET \\"
    echo "    -c \"program ${ELF_FILE} verify reset exit\""
    echo

    # Split $OPENOCD into words (e.g. "doas openocd" → two words)
    # so that execve receives them as separate argv entries.
    openocd_cmd=($OPENOCD)
    "${openocd_cmd[@]}" \
        -f "$OPENOCD_INTERFACE" \
        -f "$OPENOCD_TARGET" \
        -c "program ${ELF_FILE} verify reset exit"

    echo "✔  Flash complete."
}

# ── Clean ──────────────────────────────────────────────────────────
cmd_clean() {
    echo "── Cleaning build directory ──────────────────────────"
    rm -rf "$BUILD_DIR"
    echo "✔  Removed $BUILD_DIR"
}

# ── Help ───────────────────────────────────────────────────────────
cmd_help() {
    sed -n '/^# Usage:/,/^$/p' "$0"
}

# ── Dispatch ───────────────────────────────────────────────────────
case "${1:-all}" in
    build)   cmd_build ;;
    upload)  target="${2:-hello_lcd}"; ELF_FILE="$BUILD_DIR/$target.elf"; cmd_upload ;;
    clean)   cmd_clean ;;
    rebuild) cmd_clean; cmd_build; ELF_FILE="$BUILD_DIR/${2:-hello_lcd}.elf"; cmd_upload ;;
    all)     cmd_build; ELF_FILE="$BUILD_DIR/hello_lcd.elf"; cmd_upload ;;
    *)       cmd_help; exit 1 ;;
esac
