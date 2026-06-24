#!/usr/bin/env bash
# tools/ci.sh — the CI build+verify+progress pipeline, run INSIDE the matching
# Docker image (open-spyro/matchenv:latest), repo mounted at /work.
#
#   docker run --rm -v "$PWD:/work" -w /work open-spyro/matchenv:latest \
#     bash tools/ci.sh
#
# This is the disc-free path: it relinks the main EXE + all 37 overlays from the
# COMMITTED asm/ + config/ (config/spyro.main.ld, config/overlays/*.ld, the
# undefined_*.auto.txt symbol lists), then SHA-1-verifies the outputs against the
# recorded baselines (config/sha1sums.txt, config/overlays.json). No `make split`
# (that needs the original disc, which is never present in CI) and no copyrighted
# game data of any kind.
#
# Unlike the Makefile `build` target, this does NOT wrap the build scripts in
# `docker_env.sh` — we are already inside the image, so we call them directly
# (no docker-in-docker).
set -euo pipefail

export PYTHON="${PYTHON:-python3}"
# First-party build tooling is the `open-spyro` CLI (installed in the matching
# image); PYTHONPATH points it at the mounted package source so repo edits win.
export PYTHONPATH="tools/open-spyro/src${PYTHONPATH:+:$PYTHONPATH}"
OPEN_SPYRO="${OPEN_SPYRO:-open-spyro}"

echo "::group::env"
echo "pwd=$(pwd)"
mips-linux-gnu-as --version | head -1 || true
"$PYTHON" --version
test -x /opt/gcc2.7.2/cc1 && echo "cc1: present" || echo "cc1: MISSING (image incomplete)"
echo "::endgroup::"

echo "::group::build main EXE"
bash tools/build_main.sh
echo "::endgroup::"

echo "::group::build 37 overlays"
bash tools/build_overlays.sh
echo "::endgroup::"

echo "::group::verify (SHA mode — no disc)"
# verify_main.sh auto-detects the absence of disc/orig and asserts SHA-1s only.
# Non-zero exit here fails the CI job: a newly-matched C function whose bytes
# don't reproduce the original turns the build red.
bash tools/verify_main.sh
echo "::endgroup::"

echo "::group::progress report"
$OPEN_SPYRO progress
echo "::endgroup::"

echo "ci: OK — build byte-reproduces recorded SHA-1s; progress regenerated"
