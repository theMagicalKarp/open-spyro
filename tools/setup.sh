#!/usr/bin/env bash
# tools/setup.sh — provision the matching toolchain (host side).
#
# Driven by `make setup`. Idempotent: re-running skips work already done.
# Everything provisioned here is gitignored (see .gitignore) — we never commit
# the compiler or the cloned tool repos. gcc 2.7.2 is built FROM SOURCE inside
# the Docker image (tools/Dockerfile), not fetched as a prebuilt binary.
#
# Phases (run all, or one by name):
#   tools      clone maspsx / decomp-permuter at pinned commits
#   iso        clone + cmake-build mkpsxiso (host-native dumpsxiso + mkpsxiso)
#   python     create the uv venv (splat/spimdisasm + open-spyro CLI) and sync
#              the tools/open-spyro `match` group (m2c, asm-differ, permuter deps)
#   docker     build the matching Docker image (tools/Dockerfile); this is where
#              gcc 2.7.2 cc1 is compiled from source
#
#   tools/setup.sh            # tools + iso + python + docker
#   tools/setup.sh docker     # just one phase

set -euo pipefail

# ============================ LOCKED PINS ====================================
# Bump deliberately — a toolchain change can break every prior byte-match.
# gcc 2.7.2 cc1 is built from source in the Docker image (tools/Dockerfile) from
# this pinned commit of the decompals/mips-gcc-2.7.2 tree. Proven codegen-
# equivalent to the historical community build via the step-02 spike.
GCC272_SRC_URL="https://github.com/decompals/mips-gcc-2.7.2.git"
GCC272_SRC_SHA="e6271f14b095d7d8b2638c2b5e07d0d5cd05bc14"

MASPSX_URL="https://github.com/mkst/maspsx.git"
MASPSX_SHA="42b862c988fe7a13fe4e7ac0ebec90ed6b9fb763"

PERMUTER_URL="https://github.com/simonlindholm/decomp-permuter.git"
PERMUTER_SHA="efc5c5e7d9850f7267323b7dca6b41bc30a42d1f"

# The Python matching tooling — splat/spimdisasm and m2c/asm-differ/permuter libs —
# is pinned solely in tools/open-spyro (pyproject.toml `split`/`match` groups,
# locked in uv.lock). Both the host (`uv run --project tools/open-spyro`) and the
# Docker image (`uv export | uv pip install --system`) install from that one lock,
# so there are no version pins to keep in sync here.

# mkpsxiso provides the host-native disc tools: dumpsxiso (extract) and
# mkpsxiso (repack). Built from source on the host (it never runs in the
# MIPS Docker image). Uses git submodules, so the clone must be recursive.
MKPSXISO_URL="https://github.com/Lameguy64/mkpsxiso.git"
MKPSXISO_SHA="633adc6778b7a7c677eecaebc7da41bd19068048"

DOCKER_IMAGE="open-spyro/matchenv:latest"
# =============================================================================

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

log()  { printf '\033[1;34m[setup]\033[0m %s\n' "$*" >&2; }
warn() { printf '\033[1;33m[setup] warn:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[setup] error:\033[0m %s\n' "$*" >&2; exit 1; }

sha1_of() { shasum -a1 "$1" 2>/dev/null | awk '{print $1}'; }

# ---- pinned tool clones ------------------------------------------------------
clone_pinned() {
  local url="$1" sha="$2" dir="$3"
  if [ -d "$dir/.git" ]; then
    log "$dir present; checking out $sha"
    git -C "$dir" fetch --quiet origin "$sha" 2>/dev/null || git -C "$dir" fetch --quiet --all
  else
    log "cloning $url -> $dir"
    git clone --quiet "$url" "$dir"
  fi
  git -C "$dir" checkout --quiet "$sha" || die "could not checkout $sha in $dir"
}
phase_tools() {
  clone_pinned "$MASPSX_URL"    "$MASPSX_SHA"    tools/maspsx
  # m2c + asm-differ are uv git deps now (tools/open-spyro `match` group); only
  # decomp-permuter still needs a clone (it ships no installable package).
  clone_pinned "$PERMUTER_URL"  "$PERMUTER_SHA"  tools/decomp-permuter
}

# ---- mkpsxiso disc tools (host-native cmake build) ---------------------------
phase_iso() {
  command -v cmake >/dev/null 2>&1 || die "cmake not found — install Xcode CLT or 'brew install cmake'"
  local dir=tools/mkpsxiso
  if [ -x "$dir/build/dumpsxiso" ] && [ -x "$dir/build/mkpsxiso" ]; then
    log "$dir/build present (dumpsxiso + mkpsxiso); skipping"
    return
  fi
  if [ -d "$dir/.git" ]; then
    log "$dir present; checking out $MKPSXISO_SHA"
    git -C "$dir" fetch --quiet origin "$MKPSXISO_SHA" 2>/dev/null || git -C "$dir" fetch --quiet --all
    git -C "$dir" checkout --quiet "$MKPSXISO_SHA" || die "could not checkout $MKPSXISO_SHA in $dir"
    git -C "$dir" submodule update --init --recursive --quiet
  else
    log "cloning $MKPSXISO_URL -> $dir (recursive)"
    git clone --quiet --recurse-submodules "$MKPSXISO_URL" "$dir"
    git -C "$dir" checkout --quiet "$MKPSXISO_SHA" || die "could not checkout $MKPSXISO_SHA in $dir"
    git -C "$dir" submodule update --init --recursive --quiet
  fi
  log "building mkpsxiso (cmake release)"
  # Plain Release build (not --preset release): the release preset redirects
  # output into a build/Release/ subdir; a plain build lands the binaries
  # directly in build/, the path extract.sh and the step docs expect.
  cmake -S "$dir" -B "$dir/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$dir/build" -j
  [ -x "$dir/build/dumpsxiso" ] || die "build finished but $dir/build/dumpsxiso missing"
  [ -x "$dir/build/mkpsxiso" ]  || die "build finished but $dir/build/mkpsxiso missing"
  log "mkpsxiso built: $dir/build/{dumpsxiso,mkpsxiso}"
}

# ---- python tooling (uv project) ---------------------------------------------
# All host Python tooling lives in the tools/open-spyro uv project, locked in its
# uv.lock and materialised into tools/open-spyro/.venv:
#   project (open-spyro CLI) + dev (ruff/ty) + split (splat/spimdisasm) + match
#   (m2c, asm-differ, decomp-permuter deps). The Makefile and CI invoke it via
#   `uv run --project tools/open-spyro …`; there is no bare root .venv anymore.
# The Docker image installs the same groups from the same uv.lock (via
# `uv export | uv pip install --system`, see tools/Dockerfile), so host and image
# share one set of pins — there are no versions to keep in sync here.
phase_python() {
  command -v uv >/dev/null 2>&1 || die "uv not found — install from https://docs.astral.sh/uv/"
  log "syncing tools/open-spyro (split + match groups: splat/spimdisasm, m2c/asm-differ, permuter deps)"
  uv sync --group split --group match --project tools/open-spyro
}

# ---- docker matching image ---------------------------------------------------
phase_docker() {
  command -v docker >/dev/null 2>&1 || { warn "docker not installed — skipping image build"; return; }
  if ! docker info >/dev/null 2>&1; then
    warn "docker daemon not running — skipping image build."
    warn "start Docker Desktop, then: make docker"
    return
  fi
  log "building $DOCKER_IMAGE (linux/amd64) from tools/Dockerfile"
  log "  (this compiles gcc 2.7.2 cc1 from source — first build takes a few minutes)"
  docker build --platform linux/amd64 \
    --build-arg GCC272_SRC_URL="$GCC272_SRC_URL" \
    --build-arg GCC272_SRC_SHA="$GCC272_SRC_SHA" \
    --build-arg MASPSX_SHA="$MASPSX_SHA" \
    -t "$DOCKER_IMAGE" -f tools/Dockerfile tools/
  log "image built: $DOCKER_IMAGE"
}

# ---- in-image verification (toolchain exit criteria) -------------------------
# Runs the checks that require the cc1, which only runs inside the image.
phase_verify() {
  command -v docker >/dev/null 2>&1 || die "docker not installed"
  docker info >/dev/null 2>&1 || die "docker daemon not running — start it, then: make verify-toolchain"
  docker image inspect "$DOCKER_IMAGE" >/dev/null 2>&1 || die "$DOCKER_IMAGE not built — run: make docker"
  log "running exit-criteria checks inside $DOCKER_IMAGE"
  docker run --rm --platform linux/amd64 -v "$ROOT:/work" -w /work "$DOCKER_IMAGE" bash -euo pipefail -c '
    echo "== cc1 -version ==";         /opt/gcc2.7.2/cc1 -version < /dev/null 2>&1 | grep -i "version" | head -1
    echo "== maspsx --help ==";        python3 /opt/maspsx/maspsx.py --help >/dev/null && echo ok
    echo "== splat --version ==";       splat --version
    echo "== m2c --help ==";           m2c --help >/dev/null && echo ok
    echo "== trivial compile chain =="; tmp=$(mktemp -d)
    printf "int f(void){return 0;}\n" > "$tmp/t.c"
    bash config/compile.sh main "$tmp/t.c" "$tmp/t.o"
    mips-linux-gnu-ld -EL -r -o "$tmp/t.elf" "$tmp/t.o" && echo "cc1->maspsx->as->ld OK"
  '
}

case "${1:-all}" in
  tools)  phase_tools ;;
  iso)    phase_iso ;;
  python) phase_python ;;
  docker) phase_docker ;;
  verify) phase_verify ;;
  all)    phase_tools; phase_iso; phase_python; phase_docker ;;
  *) die "unknown phase: $1 (try: tools | iso | python | docker | verify | all)" ;;
esac
log "done: ${1:-all}"
