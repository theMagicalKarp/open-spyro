#!/usr/bin/env bash
# tools/native_setup.sh — provision the native-port toolchain (macOS arm64,
# Linux arm64, Linux x86_64).
#
# Native build = game C + PsyZ core compiled for wasm32 (ILP32, PS1 RAM at
# 0x80000000), wasm2c'd to C, then built for the host by its C compiler
# (docs/native-spike.md). Idempotent: re-running skips work already done.
# Nothing here touches the PS1 matching toolchain.
#
# Installs:
#   wasi-sdk   wasm32 clang + wasm-ld + wasi-libc sysroot, pinned, unpacked into
#              build/native-tools/ (gitignored via build/)
#   wabt       wasm2c, built from the pinned wabt source release into
#              build/native-tools/wabt-<ver>/bin, with its runtime sources
#              (wasm-rt-impl.c etc.) in build/native-tools/wasm2c-runtime; one
#              source tarball for both keeps them at the same version
#   sdl3       host video/audio/input backend for PsyZ: Homebrew on macOS; on
#              Linux the system's if pkg-config finds one, else built from the
#              pinned source release into build/native-tools/sdl3
#   cmake      PsyZ / native build driver: Homebrew on macOS; on Linux the
#              system's (see LINUX_PACKAGES)
#
#   tools/native_setup.sh          # install everything, then run the checks
#   tools/native_setup.sh check    # only run the checks

set -euo pipefail

# ============================ PINS ===========================================
WASI_SDK_VERSION="34.0"
WASI_SDK_TAG="wasi-sdk-34"
# Per host: <asset suffix> <sha256>.
WASI_SDK_PIN_arm64_macos="9c59398106b417f8f14913380fdf0097a8cc0ff4af9eb3ce0065a859e88d49e9"
WASI_SDK_PIN_arm64_linux="f7e243dff54d60bcc576e94d6166b69f410f2500ae4a9ceef34315be10e77971"
WASI_SDK_PIN_x86_64_linux="b761e3a0721dbae9c09a0059e5fdb2bf917d1b4a8a7b430fb3b5aafb0984b2c4"

# wasm2c and its runtime must be the same version: both come from this tarball.
WABT_VERSION="1.0.42"
WABT_SRC_ASSET="wabt-${WABT_VERSION}.tar.xz"
WABT_SRC_SHA256="a76cda3c174a43097863a07fc0b0c202f770f53e21806ea2636f167d1ffb1e30"
WABT_SRC_URL="https://github.com/WebAssembly/wabt/releases/download/${WABT_VERSION}/${WABT_SRC_ASSET}"

# Linux only, when no system SDL3 is found (the version Homebrew ships).
SDL3_VERSION="3.4.16"
SDL3_SRC_ASSET="SDL3-${SDL3_VERSION}.tar.gz"
SDL3_SRC_SHA256="7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68"
SDL3_SRC_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/${SDL3_SRC_ASSET}"

BREW_FORMULAE=(sdl3 cmake)
# Debian/Ubuntu packages the Linux build needs (compiler, cmake, and SDL3's
# X11/Wayland/audio development headers for the source build).
LINUX_PACKAGES="build-essential clang cmake curl xz-utils pkg-config \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libegl-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev \
  libpipewire-0.3-dev libudev-dev libdbus-1-dev"
# =============================================================================

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="$ROOT/build/native-tools"
WASI_SDK_DIR="$TOOLS_DIR/wasi-sdk-${WASI_SDK_VERSION}"
SDL3_DIR="$TOOLS_DIR/sdl3"
WASM2C_RT_DIR="$TOOLS_DIR/wasm2c-runtime"
WABT_DIR="$TOOLS_DIR/wabt-${WABT_VERSION}"
WASM2C="$WABT_DIR/bin/wasm2c"
# Bounded: a bare -j is unlimited under the Makefile generator.
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

log() { printf '\033[1m==> %s\033[0m\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

sha256_check() { # <sha256> <file>
  if command -v sha256sum >/dev/null; then
    echo "$1  $2" | sha256sum -c - >/dev/null
  else
    echo "$1  $2" | shasum -a 256 -c - >/dev/null
  fi
}

preflight() {
  OS="$(uname -s)"
  ARCH="$(uname -m)"
  case "$OS/$ARCH" in
    Darwin/arm64) HOST=arm64-macos ;;
    Linux/aarch64 | Linux/arm64) HOST=arm64-linux ;;
    Linux/x86_64) HOST=x86_64-linux ;;
    *) die "unsupported host $OS/$ARCH (macOS arm64, Linux arm64, Linux x86_64)" ;;
  esac
  local pin="WASI_SDK_PIN_${HOST//-/_}"
  WASI_SDK_ASSET="wasi-sdk-${WASI_SDK_VERSION}-${HOST}.tar.gz"
  WASI_SDK_SHA256="${!pin}"
  WASI_SDK_URL="https://github.com/WebAssembly/wasi-sdk/releases/download/${WASI_SDK_TAG}/${WASI_SDK_ASSET}"

  if [ "$OS" = Darwin ]; then
    command -v brew >/dev/null || die "Homebrew not found (https://brew.sh)"
    command -v clang >/dev/null || die "Apple clang not found (xcode-select --install)"
  else
    local missing=()
    for t in cc c++ cmake curl xz pkg-config; do
      command -v "$t" >/dev/null || missing+=("$t")
    done
    [ ${#missing[@]} -eq 0 ] ||
      die "missing ${missing[*]}; on Debian/Ubuntu: sudo apt install $LINUX_PACKAGES"
  fi
}

install_brew() {
  for f in "${BREW_FORMULAE[@]}"; do
    if brew list --formula "$f" >/dev/null 2>&1; then
      log "brew: $f already installed"
    else
      log "brew: installing $f"
      brew install "$f"
    fi
  done
}

install_sdl3_linux() {
  if pkg-config --exists sdl3 2>/dev/null; then
    log "sdl3: system $(pkg-config --modversion sdl3)"
    return
  fi
  if [ "$(cat "$SDL3_DIR/VERSION" 2>/dev/null)" = "$SDL3_VERSION" ]; then
    log "sdl3 $SDL3_VERSION already at $SDL3_DIR"
    return
  fi
  mkdir -p "$TOOLS_DIR"
  local tarball="$TOOLS_DIR/$SDL3_SRC_ASSET"
  local src="$TOOLS_DIR/sdl3-src"
  log "downloading $SDL3_SRC_ASSET"
  curl -fL --retry 3 -o "$tarball" "$SDL3_SRC_URL"
  sha256_check "$SDL3_SRC_SHA256" "$tarball" \
    || { rm -f "$tarball"; die "SDL3 source checksum mismatch (tarball removed; re-run)"; }
  rm -rf "$src" "$SDL3_DIR"
  mkdir -p "$src"
  tar -xzf "$tarball" -C "$src" --strip-components=1
  log "building SDL3 $SDL3_VERSION (needs: $LINUX_PACKAGES)"
  cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDL3_DIR" -DSDL_SHARED=ON -DSDL_STATIC=OFF \
    -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >/dev/null
  cmake --build "$src/build" -j "$JOBS" >/dev/null
  cmake --install "$src/build" >/dev/null
  echo "$SDL3_VERSION" > "$SDL3_DIR/VERSION"
  rm -rf "$src" "$tarball"
}

install_wasi_sdk() {
  if [ -x "$WASI_SDK_DIR/bin/clang" ]; then
    log "wasi-sdk $WASI_SDK_VERSION already at $WASI_SDK_DIR"
    return
  fi
  mkdir -p "$TOOLS_DIR"
  local tarball="$TOOLS_DIR/$WASI_SDK_ASSET"
  if [ ! -f "$tarball" ]; then
    log "downloading $WASI_SDK_ASSET"
    curl -fL --retry 3 -o "$tarball.part" "$WASI_SDK_URL"
    mv "$tarball.part" "$tarball"
  fi
  log "verifying sha256"
  sha256_check "$WASI_SDK_SHA256" "$tarball" \
    || { rm -f "$tarball"; die "wasi-sdk checksum mismatch (tarball removed; re-run)"; }
  log "unpacking into $TOOLS_DIR"
  tar -xzf "$tarball" -C "$TOOLS_DIR"
  # The tarball unpacks to wasi-sdk-<ver>-<host>; normalize the name.
  if [ -d "$TOOLS_DIR/wasi-sdk-${WASI_SDK_VERSION}-${HOST}" ]; then
    mv "$TOOLS_DIR/wasi-sdk-${WASI_SDK_VERSION}-${HOST}" "$WASI_SDK_DIR"
  fi
  # macOS: downloaded binaries carry the quarantine bit; Gatekeeper would
  # block them.
  [ "$OS" != Darwin ] || xattr -dr com.apple.quarantine "$WASI_SDK_DIR" 2>/dev/null || true
  rm -f "$tarball"
}

install_wabt() {
  if [ -x "$WASM2C" ] && [ -f "$WASM2C_RT_DIR/wasm-rt-impl.c" ] &&
     [ "$(cat "$WASM2C_RT_DIR/VERSION" 2>/dev/null)" = "$WABT_VERSION" ]; then
    log "wabt $WABT_VERSION (wasm2c + runtime) already in $TOOLS_DIR"
    return
  fi
  mkdir -p "$TOOLS_DIR"
  local tarball="$TOOLS_DIR/$WABT_SRC_ASSET"
  local src="$TOOLS_DIR/wabt-src"
  log "downloading $WABT_SRC_ASSET"
  curl -fL --retry 3 -o "$tarball" "$WABT_SRC_URL"
  sha256_check "$WABT_SRC_SHA256" "$tarball" \
    || { rm -f "$tarball"; die "wabt source checksum mismatch (tarball removed; re-run)"; }
  rm -rf "$src" "$WASM2C_RT_DIR"
  mkdir -p "$src" "$WASM2C_RT_DIR" "$WABT_DIR/bin"
  tar -xJf "$tarball" -C "$src" --strip-components=1
  log "building wasm2c $WABT_VERSION"
  cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    -DBUILD_LIBWASM=OFF -DWITH_WASI=OFF >/dev/null
  cmake --build "$src/build" --target wasm2c -j "$JOBS" >/dev/null
  cp "$src/build/wasm2c" "$WASM2C"
  cp -R "$src/wasm2c/." "$WASM2C_RT_DIR/"
  echo "$WABT_VERSION" > "$WASM2C_RT_DIR/VERSION"
  rm -rf "$src" "$tarball"
}

check() {
  log "checks"
  local tmp; tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  "$WASI_SDK_DIR/bin/clang" --version | head -1
  [ -x "$WASM2C" ] || die "wasm2c missing; run tools/native_setup.sh"
  echo "wasm2c $("$WASM2C" --version)"
  local rt="$WASM2C_RT_DIR"
  [ -f "$rt/wasm-rt-impl.c" ] || die "wasm2c runtime missing; run tools/native_setup.sh"
  echo "wasm2c runtime: $rt"

  # End-to-end: an ILP32 C file storing data at a PS1 address -> wasm32 ->
  # wasm2c -> host executable.
  cat > "$tmp/t.c" <<'EOF'
typedef struct Actor { void *state; struct Actor *next; int pad[20]; } Actor;
typedef char ActorSizeCheck[sizeof(Actor) == 0x58 ? 1 : -1];
__attribute__((export_name("probe")))
int probe(void) {
  volatile int *ram = (volatile int *)0x80010000;
  ram[0] = (int)(ram + 1);            /* a pointer stored in an int */
  return ram[0] < 0 ? sizeof(void *) : -1; /* KSEG0 sign test */
}
EOF
  "$WASI_SDK_DIR/bin/clang" --target=wasm32 -O2 -nostdlib -Wl,--no-entry \
    -Wl,--initial-memory=2155872256 -Wl,--max-memory=2155872256 \
    -o "$tmp/t.wasm" "$tmp/t.c"
  "$WASM2C" "$tmp/t.wasm" -n t -o "$tmp/t_w.c"
  cat > "$tmp/main.c" <<'EOF'
#include <stdio.h>
#include "t_w.h"
int main(void) {
  w2c_t inst;
  wasm_rt_init();
  wasm2c_t_instantiate(&inst);
  int r = w2c_t_probe(&inst);
  printf("probe -> %d (want 4: 32-bit pointers, KSEG0 address < 0)\n", r);
  wasm2c_t_free(&inst);
  wasm_rt_free();
  return r == 4 ? 0 : 1;
}
EOF
  cc -O1 -I"$tmp" -I"$rt" \
    "$tmp/main.c" "$tmp/t_w.c" "$rt/wasm-rt-impl.c" "$rt/wasm-rt-mem-impl.c" \
    -o "$tmp/probe"
  "$tmp/probe"
  log "native toolchain OK"
  echo "WASI_SDK=$WASI_SDK_DIR"
  echo "WASM2C=$WASM2C"
}

preflight
case "${1:-all}" in
  all)
    if [ "$OS" = Darwin ]; then install_brew; else install_sdl3_linux; fi
    install_wasi_sdk; install_wabt; check ;;
  check) check ;;
  *) die "usage: tools/native_setup.sh [all|check]" ;;
esac
