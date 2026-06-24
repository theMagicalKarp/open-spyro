#!/usr/bin/env bash
# tools/docker_env.sh — drop into the matching environment with the repo mounted.
# Requires `make setup` (or `make docker`) to have built the image first.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Allocate a TTY only when attached to one, so `make build` (non-interactive,
# e.g. CI) works while `make shell` stays interactive.
tty_flag=""
[ -t 0 ] && tty_flag="-t"
exec docker run --rm -i $tty_flag --platform linux/amd64 \
  -v "$ROOT:/work" -w /work \
  open-spyro/matchenv:latest "${@:-/bin/bash}"
