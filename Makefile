# open-spyro — byte-for-byte matching decompilation of Spyro the Dragon (PS1, SCUS_942.28)
#
# Run `make` with no target to print the full target list.

.DEFAULT_GOAL := help

# Host Python tooling runs through the tools/open-spyro uv project (locked in its
# uv.lock). `--group split` carries splat/spimdisasm; it is passed on every call
# so the project venv stays at one consistent group set (no per-invocation resync).
UVRUN      := uv run --project tools/open-spyro --group split
SPLAT      := $(UVRUN) -- python -m splat
OPEN_SPYRO := $(UVRUN) -- open-spyro
# clang-format ships from the tools/open-spyro `dev` group (pinned in uv.lock), so
# the host gets the exact same formatter as CI. Scope is every C source and
# header in the repo (src/, include/, native/); the generated hand-asm
# translation (src/native/handasm) is formatted by its generator.
CLANG_FORMAT := $(UVRUN) -- clang-format
CFMT_SRC     := $(shell find src include native -name '*.[ch]')
CFMT_WIP     := $(shell find src -name '*.c.wip')

.PHONY: help setup setup-iso docker shell verify-toolchain extract split build verify check wad iso progress lint lint-py lint-c fmt fmt-py fmt-c clean native native-run native-gate native-test

help: ## Print this help (default target)
	@echo "open-spyro — make targets:"
	@echo ""
	@echo "  setup      Build the matching toolchain (gcc 2.7.2 from source + binutils + maspsx + splat)"
	@echo "  setup-iso  Build just the host-native disc tools (mkpsxiso + dumpsxiso)"
	@echo "  docker     Build just the matching Docker image (tools/Dockerfile)"
	@echo "  shell      Open a shell in the matching Docker image (repo mounted at /work)"
	@echo "  verify-toolchain  Run the toolchain exit-criteria checks inside the Docker image"
	@echo "  extract    Dump the original files from the disc image into disc/orig/ (gitignored)"
	@echo "  split      Run splat to disassemble into asm/ + config/"
	@echo "  build      Compile + link src/ and asm/ into a byte-identical SCUS_942.28 and overlays"
	@echo "  verify     Check rebuilt artifacts against the original SHA-1s"
	@echo "  check      Rebuild then verify (build + verify) — avoids verifying a stale EXE"
	@echo "  wad        Stage build/WAD.WAD from current parts (repack if overlays built, else pass)"
	@echo "  iso        Repack the rebuilt files into a runnable .bin/.cue"
	@echo "  progress   Regenerate the C-match progress report (PROGRESS.md + badge + map)"
	@echo "  lint       Lint + type-check everything: Python (ruff + ty) + C (clang-format), same as CI"
	@echo "  fmt        Auto-format everything: Python (ruff) + C (clang-format)"
	@echo "  clean      Remove build output + all regenerable/untracked artifacts"
	@echo ""
	@echo "  native       Build the native executable: macOS arm64, Linux arm64/x86_64 (build/native/spyro; needs tools/native_setup.sh)"
	@echo "  native-run   Run it with spyro.toml (NATIVE_ARGS=\"--frames 300 --screenshot out.bmp\" to pass options)"
	@echo "  native-test  Build + run the native host unit tests (build/native-test; needs only a C compiler + cmake)"
	@echo "  native-gate  ILP32 gate: native level load vs a DuckStation savestate (SAVESTATE=path.sav)"

setup: ## Build the matching toolchain
	@bash tools/setup.sh all

setup-iso: ## Build just the host-native disc tools (mkpsxiso + dumpsxiso)
	@bash tools/setup.sh iso

docker: ## Build just the matching Docker image
	@bash tools/setup.sh docker

shell: ## Open a shell in the matching Docker image (repo mounted at /work)
	@bash tools/docker_env.sh

verify-toolchain: ## Run the toolchain exit-criteria checks inside the Docker image
	@bash tools/setup.sh verify

extract: ## Dump original files from the disc image
	@bash tools/extract.sh

split: ## Disassemble into asm/ + config/ via splat
	$(SPLAT) split config/spyro.main.yaml
	$(OPEN_SPYRO) fixup-hasm
	@# Per-function flip infra: wrap each function in asm/text.s in its own fixed-VMA section
	@# (+ HAVE_C guard) and refresh config/text_layout.json so functions can be
	@# flipped to C individually. Idempotent; safe to re-run after every split.
	$(OPEN_SPYRO) sectionize
	@# Overlays: regen the per-overlay splat configs from the frozen
	@# overlay table (config/overlays.json), then split each into asm/overlays/.
	$(OPEN_SPYRO) gen-overlay-yaml
	@for y in config/overlays/*.yaml; do \
		$(SPLAT) split "$$y" >/dev/null || exit 1; \
	done
	@# Function/data-split overlays (those with a committed .symbols.txt seed) get the
	@# same per-function flip infra as the main EXE; also re-inserts the slots INCLUDE
	@# splat just wiped from their generated .ld. Idempotent.
	$(OPEN_SPYRO) sectionize-overlays
	@echo "split: disassembled main + $$(ls config/overlays/*.yaml | wc -l | tr -d ' ') overlays"

build: ## Compile + link into a byte-identical SCUS_942.28 + all 37 overlays
	@bash tools/docker_env.sh bash tools/build_main.sh
	@bash tools/docker_env.sh bash tools/build_overlays.sh

verify: ## Check rebuilt artifacts (main EXE + 37 overlays + WAD.WAD) against original SHA-1s
	@bash tools/verify_main.sh

check: build verify ## Rebuild then verify — guards against verifying a stale EXE

wad: ## Stage build/WAD.WAD from current parts (byte-faithful repack via open-spyro wad)
	@bash tools/wad.sh

iso: ## Repack rebuilt files into a runnable disc
	@bash tools/make_iso.sh

progress: ## Regenerate the C-match progress report (PROGRESS.md + badge + map)
	$(OPEN_SPYRO) progress
	$(OPEN_SPYRO) progress-treemap

lint: lint-py lint-c ## Lint + type-check everything (Python + C) — mirrors the CI lint job

lint-py: ## Lint + type-check the Python tooling (ruff + ty)
	cd tools/open-spyro && uv run --group split ruff check .
	cd tools/open-spyro && uv run --group split ruff format --check .
	cd tools/open-spyro && uv run --group split ty check

lint-c: ## Check all C + parked .c.wip formatting (clang-format --dry-run --Werror)
	$(CLANG_FORMAT) --dry-run --Werror $(CFMT_SRC) $(CFMT_WIP)

fmt: fmt-py fmt-c ## Auto-format everything (Python ruff + C clang-format)

fmt-py: ## Auto-format + autofix the Python tooling (ruff)
	cd tools/open-spyro && uv run --group split ruff format .
	cd tools/open-spyro && uv run --group split ruff check --fix .

fmt-c: ## Auto-format all C sources + headers + parked .c.wip attempts (clang-format -i)
	$(CLANG_FORMAT) -i $(CFMT_SRC) $(CFMT_WIP)

# ---- native port ------------------------------------------------------------
# Everything lives under build/native/; the PS1 build is untouched. PsyZ is the
# external/psyz submodule (its own submodules are not needed); the config-file
# parser is the external/tomlc17 submodule.
NATIVE_BUILD := build/native
NATIVE_ARGS  ?=
# Bounded: a bare -j is unlimited under cmake's Makefile generator.
NATIVE_JOBS  ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
# The config file native-run passes (--config): spyro.toml, a gitignored copy
# of spyro.example.toml; empty for the user's own (config.toml in their data
# directory).
NATIVE_CONFIG ?= spyro.toml

native: ## Build the native executable (macOS arm64, Linux arm64/x86_64) (build/native/spyro)
	@test -f external/psyz/psyz/CMakeLists.txt || git submodule update --init external/psyz
	@test -f external/tomlc17/src/tomlc17.c || git submodule update --init external/tomlc17
	cmake -S native -B $(NATIVE_BUILD) -DCMAKE_BUILD_TYPE=Release >/dev/null
	cmake --build $(NATIVE_BUILD) -j $(NATIVE_JOBS)

native-run: native ## Run the native executable with spyro.toml
	@test -z "$(NATIVE_CONFIG)" -o -f "$(NATIVE_CONFIG)" || { echo "native-run: no $(NATIVE_CONFIG) (cp spyro.example.toml spyro.toml, then set its disc)"; exit 2; }
	$(NATIVE_BUILD)/spyro $(if $(NATIVE_CONFIG),--config "$(NATIVE_CONFIG)") $(NATIVE_ARGS)

native-test: ## Build + run the native host unit tests (native/tests; no wasm toolchain needed)
	@test -f external/tomlc17/src/tomlc17.c || git submodule update --init external/tomlc17
	cmake -S native/tests -B build/native-test -DCMAKE_BUILD_TYPE=Debug >/dev/null
	cmake --build build/native-test -j $(NATIVE_JOBS)
	ctest --test-dir build/native-test --output-on-failure

native-gate: native ## ILP32 gate: native level load vs PS1 RAM in SAVESTATE (a DuckStation .sav)
	@test -n "$(SAVESTATE)" || { echo "usage: make native-gate SAVESTATE=path/to/state.sav (taken in a level)"; exit 2; }
	$(OPEN_SPYRO) native gate "$(SAVESTATE)" --binary $(NATIVE_BUILD)/gate

clean: ## Remove all build output + regenerable/untracked artifacts
	rm -rf build/*
	rm -f config/spyro.main.d config/spyro.main.slots.ld config/spyro.main.syms.ld
	rm -f config/*.d
	rm -f config/overlays/*.yaml config/overlays/*.d
	rm -f progress.json
	rm -rf tools/open-spyro/.ruff_cache
	find tools/open-spyro -name .venv -prune -o -name __pycache__ -type d -exec rm -rf {} +
