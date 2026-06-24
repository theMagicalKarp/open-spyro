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
# the host gets the exact same formatter as CI. Scope is the hand-written matched
# C in src/c/ only — the Ghidra-seeded headers in include/ are generated, not
# hand-edited, so they are intentionally left out.
CLANG_FORMAT := $(UVRUN) -- clang-format
CFMT_SRC     := $(wildcard src/c/*.c)

.PHONY: help setup setup-iso docker shell verify-toolchain extract split build verify check wad iso play progress lint lint-py lint-c fmt fmt-py fmt-c clean

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
	@echo "  play       Boot the rebuilt iso in DuckStation"
	@echo "  progress   Regenerate the C-match progress report (PROGRESS.md + badge)"
	@echo "  lint       Lint + type-check everything: Python (ruff + ty) + C (clang-format), same as CI"
	@echo "  fmt        Auto-format everything: Python (ruff) + C (clang-format)"
	@echo "  clean      Remove build output + all regenerable/untracked artifacts"

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

play: ## Boot the rebuilt iso in DuckStation
	@bash tools/play.sh

progress: ## Regenerate the C-match progress report (PROGRESS.md + badge)
	$(OPEN_SPYRO) progress

lint: lint-py lint-c ## Lint + type-check everything (Python + C) — mirrors the CI lint job

lint-py: ## Lint + type-check the Python tooling (ruff + ty)
	cd tools/open-spyro && uv run --group split ruff check .
	cd tools/open-spyro && uv run --group split ruff format --check .
	cd tools/open-spyro && uv run --group split ty check

lint-c: ## Check matched C formatting (clang-format --dry-run --Werror)
	$(CLANG_FORMAT) --dry-run --Werror $(CFMT_SRC)

fmt: fmt-py fmt-c ## Auto-format everything (Python ruff + C clang-format)

fmt-py: ## Auto-format + autofix the Python tooling (ruff)
	cd tools/open-spyro && uv run --group split ruff format .
	cd tools/open-spyro && uv run --group split ruff check --fix .

fmt-c: ## Auto-format the matched C sources (clang-format -i)
	$(CLANG_FORMAT) -i $(CFMT_SRC)

clean: ## Remove all build output + regenerable/untracked artifacts
	rm -rf build/*
	rm -f config/spyro.main.d config/spyro.main.slots.ld config/spyro.main.syms.ld
	rm -f config/*.d
	rm -f src/asm/overrides.mk
	rm -f config/overlays/*.yaml config/overlays/*.d
	rm -f progress.json
	rm -rf tools/open-spyro/.ruff_cache
	find tools/open-spyro -name .venv -prune -o -name __pycache__ -type d -exec rm -rf {} +
