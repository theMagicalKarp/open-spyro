# tools/Dockerfile — the single source of truth for the matching environment.
#
# Built by `make setup` / `make docker` as open-spyro/matchenv:latest.
# Run with the repo mounted at /work (see docker_env.sh). The gcc 2.7.2 cc1 is
# the canonical decompals/old-gcc PREBUILT release binary (a 32-bit i386 static
# ELF — the historical PSYQ-era community build), so the image targets
# linux/amd64 (use --platform linux/amd64 on Apple Silicon; runs under
# emulation).
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
# Stage 1 — fetch the canonical prebuilt gcc 2.7.2 `cc1`.
#
# The decompals/old-gcc RELEASE binary (i386 static ELF), not a from-source
# build. This is the binary the PSX decomp community matches against. A
# from-source build of the same 2.7.2 tree on a modern host is NOT
# bit-behavior-identical: we found a stack-temp slot allocation divergence
# (host-compiler-dependent behavior in the 2.7.2 source) that flips spill
# slot offsets in large functions (found via func_titlescreen_8007CEE4:
# `sw $t0,0x24($sp)` vs the original 0x20). The release binary reproduces
# the original bytes; the from-source one does not. Pinned by tag + sha256.
# ============================================================================
FROM ${BASE} AS gcc-builder

ARG GCC272_REL_URL=https://github.com/decompals/old-gcc/releases/download/0.17/gcc-2.7.2.tar.gz
ARG GCC272_CC1_SHA256=fee572bf052e83f7610b77ca776aa8c30d797a534464391b77852a27137805b0

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl \
        file \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "${GCC272_REL_URL}" -o /tmp/gcc-2.7.2.tar.gz \
    && mkdir -p /src \
    && tar xzf /tmp/gcc-2.7.2.tar.gz -C /src \
    && echo "${GCC272_CC1_SHA256}  /src/cc1" | sha256sum -c - \
    && file /src/cc1

# ============================================================================
# Stage 2 — the matching environment.
# ============================================================================
FROM ${BASE}

WORKDIR /work

# The canonical prebuilt gcc 2.7.2 code generator (decompals/old-gcc release).
# Lives only in the image (it is a Linux binary; it never runs on the macOS
# host). compile.sh defaults CC1 here.
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
