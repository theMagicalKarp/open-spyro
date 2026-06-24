# tools/Dockerfile — the single source of truth for the matching environment.
#
# Built by `make setup` / `make docker` as open-spyro/matchenv:latest.
# Run with the repo mounted at /work (see docker_env.sh). The gcc 2.7.2 cc1 is
# BUILT FROM SOURCE in the first stage below (a 32-bit i386 static ELF, same as
# the historical PSYQ-era community build), so the image targets linux/amd64
# (use --platform linux/amd64 on Apple Silicon; runs under emulation).
#
# Pin everything: an unpinned binutils/maspsx/pip/gcc-source bump can silently
# change emitted bytes.

# Debian Trixie pinned by digest for reproducibility (linux/amd64).
ARG BASE=debian:trixie-slim@sha256:4e401d95de7083948053197a9c3913343cd06b706bf15eb6a0c3ccd26f436a0e

# uv: the installer for all Python tooling. A global ARG + named stage (a tagged
# `--from=ghcr.io/...:${ARG}` is not a valid COPY source). Pinned to the host
# version for reproducibility.
ARG UV_VER=0.11.19
FROM ghcr.io/astral-sh/uv:${UV_VER} AS uv

# ============================================================================
# Stage 1 — build gcc 2.7.2 `cc1` from source.
#
# Source is decompals/mips-gcc-2.7.2: the GPL GNU gcc 2.7.2 tree with the
# generated lex/yacc files committed (so modern bison/flex never run) and the
# minimal patches to compile on a modern host. This produces a `cc1` whose
# *codegen* is byte-identical to the historical community build for this game
# (proven against the step-02 spike). Pinned to the v0.1 commit.
#
# Recipe mirrors the upstream linux CI job (.github/workflows/build.yml):
#   configure --target=mips-mips-gnu --host/--build=i386-pc-linux
#   CFLAGS="-static -std=gnu89 -m32"  (hence the 32-bit i386 static binary)
#   touch c-parse.c  (don't regenerate the committed parser with modern tools)
# We only need `cc1` — compile.sh calls it directly and never uses the gcc
# driver, so the driver is not built or shipped.
# ============================================================================
FROM ${BASE} AS gcc-builder

ARG GCC272_SRC_URL=https://github.com/decompals/mips-gcc-2.7.2.git
ARG GCC272_SRC_SHA=e6271f14b095d7d8b2638c2b5e07d0d5cd05bc14

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        gcc-multilib \
        bison \
        gperf \
        autoconf \
        file \
        git \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN git clone "${GCC272_SRC_URL}" /src \
    && git -C /src checkout "${GCC272_SRC_SHA}"

WORKDIR /src
RUN ./configure \
        --target=mips-mips-gnu --prefix=/opt/cross --with-gnu-as \
        --disable-gprof --disable-gdb --disable-werror \
        --host=i386-pc-linux --build=i386-pc-linux \
    && touch c-parse.c \
    && make CFLAGS="-static -std=gnu89 -m32" cc1 \
    && test -f cc1 \
    && file cc1

# ============================================================================
# Stage 2 — the matching environment.
# ============================================================================
FROM ${BASE}

WORKDIR /work

# The from-source gcc 2.7.2 code generator. Lives only in the image (it is a
# Linux binary; it never runs on the macOS host). compile.sh defaults CC1 here.
COPY --from=gcc-builder /src/cc1 /opt/gcc2.7.2/cc1

# Modern MIPS binutils + cpp/gcc driver used for as/ld/objcopy and preprocessing.
# (The actual code generator is the from-source gcc 2.7.2 cc1 above, not this gcc.)
RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mips-linux-gnu \
        gcc-mips-linux-gnu \
        cpp-mips-linux-gnu \
        build-essential \
        git \
        ca-certificates \
        python3 \
        python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN git config --global --add safe.directory /work

# uv is the installer for all Python tooling (same tool as the host venv), copied
# in from the named stage declared up top.
COPY --from=uv /uv /usr/local/bin/uv

# First-party build tooling: the `open-spyro` CLI plus its locked Python deps,
# installed FROM tools/open-spyro's uv.lock — the single source of truth shared
# with the host (so splat/spimdisasm/m2c/asm-differ versions can never drift
# between host and image). The build context is tools/, so the package source is
# `open-spyro`. We install --system (no venv) to match the image's PATH model:
# `uv export` flattens the lock to a pinned requirements set, then the project
# itself goes on PATH as the `open-spyro` console script. At runtime the build
# scripts prepend PYTHONPATH=tools/open-spyro/src (the mounted repo), so this baked
# copy only supplies the entrypoint — repo edits stay authoritative for the link.
#   split  — splat64[mips] + spimdisasm (disassembly + the verify-toolchain smoke)
#   match  — m2c + asm-differ + decomp-permuter libs (in-image match parity)
COPY open-spyro /opt/open-spyro
RUN cd /opt/open-spyro \
    && uv export --frozen --no-hashes --no-emit-project --no-default-groups \
         --group split --group match > /tmp/open-spyro-reqs.txt \
    && uv pip install --system --break-system-packages --no-cache -r /tmp/open-spyro-reqs.txt \
    && uv pip install --system --break-system-packages --no-cache /opt/open-spyro

# maspsx baked in at a pinned commit (compile.sh invokes it as /opt/maspsx/maspsx.py;
# it is not a project-managed Python dist). Also available via the tools/ mount.
ARG MASPSX_SHA=42b862c988fe7a13fe4e7ac0ebec90ed6b9fb763
RUN git clone https://github.com/mkst/maspsx.git /opt/maspsx \
    && git -C /opt/maspsx checkout "${MASPSX_SHA}"

CMD ["/bin/bash"]
