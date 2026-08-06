#
# pi-chocolate-doom — Chocolate Doom as a bootable bare-metal Raspberry Pi image.
#
#   make check-toolchain     report the cross compiler this build will use
#   make deps                the three circle-stdlib worlds and the shim
#                            archives built against them (long: the worlds
#                            build newlib and libc++ from source)
#   make deps-rpi4           the same for one board only, for a machine that
#                            cannot hold three worlds at once
#   make rpi5 | rpi4 | rpi3  one board's kernel image
#   make kernels             all three, built in parallel
#   make verify              truth-gate: every image exists and is non-empty
#   make media               download the freely redistributable game data
#                            into media/
#   make netboot             stage the Pi 5 image and its boot configuration
#                            into build/netboot-rpi5/
#   make card                stage the whole card into build/sd-card/, copying
#                            in whatever media/ holds and naming what it does
#                            not. It never downloads anything
#   make clean-boards        drop every board's build tree
#
# The three boards never share mutable state: each has its own circle-stdlib
# world, its own shim archive and its own object directory, so building them
# at the same time is safe and building one never disturbs another.
#
# The libc++ sources every world is built from are one immutable git tag, and
# CIRCLE_LLVM says where that checkout lives. The default puts it beside this
# repository, which is right for a plain clone and for a CI runner. Point
# several projects at one directory to fetch it once for all of them:
#
#   make deps CIRCLE_LLVM=/path/to/circle-llvm
#

include mk/toolchain.mk

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := kernels

BOARDS ?= rpi3 rpi4 rpi5

IMAGE_rpi3 = kernel8.img
IMAGE_rpi4 = kernel8-rpi4.img
IMAGE_rpi5 = kernel_2712.img

.PHONY: deps kernels verify media netboot card clean-boards $(BOARDS)
.PHONY: $(addprefix deps-,$(BOARDS))

deps:
	$(MAKE) -C circle-libsdl2 deps

# One board's dependencies: its own circle-stdlib world and the shim archive
# built against it. A machine with a small disk — a CI runner, most obviously
# — builds one board at a time and keeps only that board's world.
# Written as a static pattern rule over the board list rather than a plain
# pattern rule: these targets are phony, and make does not apply pattern rules
# to phony targets — it would quietly answer "nothing to be done" and leave
# the world unbuilt.
$(addprefix deps-,$(BOARDS)): deps-%:
	$(MAKE) -C circle-libsdl2 world BOARD=$*
	$(MAKE) -C circle-libsdl2 libSDL2-$*.a BOARD=$*

$(BOARDS): check-toolchain
	$(MAKE) -C host RAPI_BOARD=$@

# All three at once. Each sub-make owns a different world and a different
# output directory, so there is nothing for them to collide on.
#
# Each board is waited for BY PID, and its status kept. A bare `wait` reports
# only that the shell has no children left — it is success whatever the jobs
# did — so a board that failed to build would leave this target reporting
# success, and the truth-gate would then pass the board's PREVIOUS image,
# still on disk.
kernels: check-toolchain
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# Truth-gate: ask the filesystem, not the exit codes. An image that is
# missing or empty fails here even if the build claimed success.
verify:
	@fail=0; \
	for b in $(BOARDS); do \
		case $$b in \
			rpi3) img=host/build/rpi3/$(IMAGE_rpi3) ;; \
			rpi4) img=host/build/rpi4/$(IMAGE_rpi4) ;; \
			rpi5) img=host/build/rpi5/$(IMAGE_rpi5) ;; \
		esac; \
		if [ -s "$$img" ]; then \
			echo "  OK    $$img ($$(wc -c < $$img | tr -d ' ') bytes)"; \
		else \
			echo "  FAIL  $$img missing or empty"; fail=1; \
		fi; \
	done; \
	exit $$fail

# ---------------------------------------------------------------------------
# Game data
# ---------------------------------------------------------------------------
#
#   media/           what `make media` downloads. Gitignored, never shipped,
#                    and never part of a build.
#   build/sd-card/   what `make card` stages. It copies from media/ and
#                    fetches nothing.
#
# `card` does not depend on `media`, so a card built without it is complete
# except for the data and names the files that are absent.
#
# `make media` fetches one file: doom1.wad, the IWAD of id Software's 1993
# Doom shareware release, episode 1. No other IWAD — doom.wad, doom2.wad,
# tnt.wad and plutonia.wad are commercial products and are copied into media/
# by hand.
#
# What arrives is checked against both checksums. The MD5 is the published
# one for the v1.9 shareware IWAD, documented independently of this download;
# the SHA256 was computed from the file this project fetched, so a later
# fetch is known to be identical. Re-running re-verifies rather than
# re-downloading.
MEDIA_DIR = media

DOOM1_WAD    = $(MEDIA_DIR)/doom1.wad
DOOM1_URL    = https://archive.org/download/doom-shareware_1996/DOOM1.WAD
DOOM1_SHA256 = 1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771
DOOM1_MD5    = f0cefca49926d00903cf57551d901abe

# sha256sum and md5sum on Linux, shasum and md5 on macOS. Whichever exists;
# if either is missing the target stops rather than accepting a download it
# cannot check.
SHA256SUM := $(firstword $(shell command -v sha256sum 2>/dev/null) \
                         $(shell command -v shasum 2>/dev/null))
MD5SUM    := $(firstword $(shell command -v md5sum 2>/dev/null) \
                         $(shell command -v md5 2>/dev/null))

media:
	@if [ -z "$(SHA256SUM)" ] || [ -z "$(MD5SUM)" ]; then \
		echo "  MEDIA no checksum tool on this machine (sha256sum/shasum and"; \
		echo "        md5sum/md5 are both needed) — refusing to download"; \
		echo "        something that cannot be verified."; \
		exit 1; \
	fi
	@mkdir -p $(MEDIA_DIR)
	@if [ -f "$(DOOM1_WAD)" ]; then \
		echo "  MEDIA $(DOOM1_WAD) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(DOOM1_URL)"; \
		curl -fL --retry 3 -o "$(DOOM1_WAD).part" "$(DOOM1_URL)" || { \
			rm -f "$(DOOM1_WAD).part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		mv "$(DOOM1_WAD).part" "$(DOOM1_WAD)"; \
	fi
	@got=`$(SHA256SUM) -a 256 "$(DOOM1_WAD)" 2>/dev/null || $(SHA256SUM) "$(DOOM1_WAD)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(DOOM1_SHA256)" ]; then \
		echo "  MEDIA SHA256 MISMATCH for $(DOOM1_WAD)"; \
		echo "        expected $(DOOM1_SHA256)"; \
		echo "        got      $$got"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	got=`$(MD5SUM) -q "$(DOOM1_WAD)" 2>/dev/null || $(MD5SUM) "$(DOOM1_WAD)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(DOOM1_MD5)" ]; then \
		echo "  MEDIA MD5 MISMATCH for $(DOOM1_WAD)"; \
		echo "        expected $(DOOM1_MD5) — the published checksum of the"; \
		echo "        v1.9 shareware IWAD"; \
		echo "        got      $$got"; \
		exit 1; \
	fi; \
	head -c 4 "$(DOOM1_WAD)" | grep -q IWAD || { \
		echo "  MEDIA $(DOOM1_WAD) does not begin with the IWAD magic"; exit 1; }; \
	echo "  MEDIA $(DOOM1_WAD) verified ($$(wc -c < $(DOOM1_WAD) | tr -d ' ') bytes)"
	@printf '%s\n' \
		"doom1.wad — the Doom shareware IWAD" \
		"" \
		"Source:   $(DOOM1_URL)" \
		"Item:     https://archive.org/details/doom-shareware_1996" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"SHA256:   $(DOOM1_SHA256)" \
		"MD5:      $(DOOM1_MD5)" \
		"" \
		"What it is: the IWAD of id Software's 1993 Doom shareware release," \
		"version 1.9 — episode 1, \"Knee-Deep in the Dead\"." \
		"" \
		"Licence: id Software's original shareware terms. The shareware" \
		"episode may be copied and passed on freely, unmodified and without" \
		"charge." \
		"" \
		"Verification: the MD5 above is the published checksum of the v1.9" \
		"shareware IWAD, documented independently of this download. The" \
		"SHA256 was computed from the copy this project fetched." \
		"" \
		"Doom is a trademark of id Software LLC. This file is not" \
		"redistributed by this repository." \
		> $(MEDIA_DIR)/provenance.txt
	@echo "  MEDIA provenance written to $(MEDIA_DIR)/provenance.txt"

# The Pi 5 netboot bundle: the image the Pi 5 firmware looks for, plus the
# boot configuration it must be served alongside. Copy the contents into the
# TFTP root the board boots from (the Raspberry Pi firmware files themselves
# come from that root's existing installation, not from here).
NETBOOT_DIR = build/netboot-rpi5
netboot: rpi5
	@mkdir -p $(NETBOOT_DIR)
	@cp host/build/rpi5/$(IMAGE_rpi5) $(NETBOOT_DIR)/
	@cp host/config.txt host/cmdline.txt $(NETBOOT_DIR)/
	@echo "  STAGED $(NETBOOT_DIR)/"
	@ls -l $(NETBOOT_DIR)/

# The card, staged into a directory to copy onto media formatted elsewhere:
# the three kernels, boot configuration, the game's default settings and
# whatever game data media/ happens to hold.
#
# Everything belonging to this game lives in one directory on the card, named
# by RAPI_GAME_DIR in host/Makefile. A card carries several games, and two of
# them writing a `default.cfg` into the FAT root would each silently overwrite
# the other's. The two paths have to agree: the kernel enters this directory
# before the game starts, so a WAD staged anywhere else is a WAD the game
# never sees.
#
# This target downloads nothing. It copies what `make media` left and names
# what is absent.
CARD_DIR  = build/sd-card
CARD_GAME = $(CARD_DIR)/games/doom

card: kernels
	@rm -rf $(CARD_DIR)
	@mkdir -p $(CARD_GAME)
	@cp host/build/rpi3/$(IMAGE_rpi3) $(CARD_DIR)/
	@cp host/build/rpi4/$(IMAGE_rpi4) $(CARD_DIR)/
	@cp host/build/rpi5/$(IMAGE_rpi5) $(CARD_DIR)/
	@cp host/config.txt host/cmdline.txt $(CARD_DIR)/
	@cp host/default.cfg host/chocolate-doom.cfg $(CARD_GAME)/
	@echo "  STAGED $(CARD_DIR)/"
	@for f in doom1.wad doom.wad doom2.wad tnt.wad plutonia.wad \
	          freedoom1.wad freedoom2.wad chex.wad hacx.wad; do \
		if [ -f "$(MEDIA_DIR)/$$f" ]; then \
			cp "$(MEDIA_DIR)/$$f" $(CARD_GAME)/; \
			echo "  DATA   $$f"; \
		fi; \
	done
	@echo
	@if ls $(CARD_GAME)/*.wad >/dev/null 2>&1; then :; else \
		echo "  ABSENT no IWAD. The game cannot start without one. Either a"; \
		echo "         WAD copied from a version of Doom you own, or"; \
		echo "         doom1.wad, the free shareware episode — 'make media'"; \
		echo "         fetches that one."; \
	fi
	@echo "  NOTE   The Raspberry Pi firmware files are not staged here either."
	@echo "         See README.md."

# Board build trees and staged output only. media/ is not touched: it holds
# downloaded data, which no build target deletes.
clean-boards:
	@for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b clean-board; done
	rm -rf $(NETBOOT_DIR) $(CARD_DIR)
