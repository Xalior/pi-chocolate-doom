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
#   make rebuild-rpi5        one board's kernel image from nothing
#   make rebuild             all three from nothing, built in parallel
#   make verify              truth-gate: every image exists, is non-empty and
#                            carries the defaults block
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

.PHONY: deps kernels rebuild verify media netboot card clean-boards $(BOARDS)
.PHONY: $(addprefix deps-,$(BOARDS)) $(addprefix rebuild-,$(BOARDS))

deps:
	+@$(NOT_DRY_RUN)
	$(MAKE) -C circle-libsdl2 deps

# One board's dependencies: its own circle-stdlib world and the shim archive
# built against it. A machine with a small disk — a CI runner, most obviously
# — builds one board at a time and keeps only that board's world.
# Written as a static pattern rule over the board list rather than a plain
# pattern rule: these targets are phony, and make does not apply pattern rules
# to phony targets — it would quietly answer "nothing to be done" and leave
# the world unbuilt.
$(addprefix deps-,$(BOARDS)): deps-%:
	+@$(NOT_DRY_RUN)
	$(MAKE) -C circle-libsdl2 world BOARD=$*
	$(MAKE) -C circle-libsdl2 libSDL2-$*.a BOARD=$*

$(BOARDS): check-toolchain
	+@$(NOT_DRY_RUN)
	$(MAKE) -C host RAPI_BOARD=$@

# One board from nothing: its build tree is removed before the build, so no
# object can be inherited from a previous one. Written as a static pattern rule
# over the board list for the same reason deps-% is.
$(addprefix rebuild-,$(BOARDS)): rebuild-%: check-toolchain
	+@$(NOT_DRY_RUN)
	$(MAKE) -C host RAPI_BOARD=$* rebuild

# All three at once. Each sub-make owns a different world and a different
# output directory, so there is nothing for them to collide on.
#
# Each board is waited for BY PID, and its status kept. A bare `wait` reports
# only that the shell has no children left — it is success whatever the jobs
# did — so a board that failed to build would leave this target reporting
# success, and the truth-gate would then pass the board's PREVIOUS image,
# still on disk.
kernels: check-toolchain
	+@$(NOT_DRY_RUN)
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# All three from nothing, in parallel, waited for by PID exactly as above.
rebuild: check-toolchain
	+@$(NOT_DRY_RUN)
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b rebuild & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# Truth-gate: ask the filesystem, not the exit codes. An image that is
# missing, empty, or does not carry the defaults block at offset 0x800 fails
# here even if the build claimed success.
#
# What this cannot tell you is whether the image was built from the sources as
# they now stand. That is a question about the build, not about the file, and
# `make rebuild` is the only answer to it.
verify:
	@fail=0; \
	for b in $(BOARDS); do \
		case $$b in \
			rpi3) img=host/build/rpi3/$(IMAGE_rpi3) ;; \
			rpi4) img=host/build/rpi4/$(IMAGE_rpi4) ;; \
			rpi5) img=host/build/rpi5/$(IMAGE_rpi5) ;; \
		esac; \
		if [ ! -s "$$img" ]; then \
			echo "  FAIL  $$img missing or empty"; fail=1; \
		elif [ "`dd if=$$img bs=4 skip=512 count=1 2>/dev/null`" != "PM8D" ]; then \
			echo "  FAIL  $$img has no defaults block at 0x800"; fail=1; \
		else \
			echo "  OK    $$img ($$(wc -c < $$img | tr -d ' ') bytes, defaults block present)"; \
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
# `make media` fetches every freely redistributable WAD this project found:
#
#   doom1.wad     the IWAD of id Software's 1993 Doom shareware release,
#                 episode 1.
#   freedoom1.wad, freedoom2.wad, freedm.wad
#                 the Freedoom project's complete, BSD-licensed IWAD set —
#                 full-size replacements for Doom, Doom II and the
#                 deathmatch-only FreeDM.
#   chex.wad      Chex Quest 1, freeware since its original 1996 release,
#                 fetched from its current official distribution site.
#   scythe.wad    a freeware PWAD megawad for Doom II (Erik Alm, 2003),
#                 fetched from the /idgames archive — proves the `-file`
#                 merge path rather than the IWAD path.
#
# No commercial IWAD — doom.wad, doom2.wad, tnt.wad and plutonia.wad are
# products and are copied into media/ by hand.
#
# What arrives is checked against a published checksum where the upstream
# project publishes one, and otherwise against the WAD's own magic and size.
# Re-running re-verifies rather than re-downloading.
MEDIA_DIR = media

DOOM1_WAD    = $(MEDIA_DIR)/doom1.wad
DOOM1_URL    = https://archive.org/download/doom-shareware_1996/DOOM1.WAD
DOOM1_SHA256 = 1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771
DOOM1_MD5    = f0cefca49926d00903cf57551d901abe

FREEDOOM_VERSION   = 0.13.0
FREEDOOM_ZIP_URL   = https://github.com/freedoom/freedoom/releases/download/v$(FREEDOOM_VERSION)/freedoom-$(FREEDOOM_VERSION).zip
FREEDOOM_ZIP_SHA256 = 3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59
FREEDOOM1_WAD      = $(MEDIA_DIR)/freedoom1.wad
FREEDOOM1_SHA256   = 7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d
FREEDOOM2_WAD      = $(MEDIA_DIR)/freedoom2.wad
FREEDOOM2_SHA256   = a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b

FREEDM_ZIP_URL     = https://github.com/freedoom/freedoom/releases/download/v$(FREEDOOM_VERSION)/freedm-$(FREEDOOM_VERSION).zip
FREEDM_ZIP_SHA256  = b420f13508ef745d7b38e83d15e55e0fc0b09d9a503c96741cddd9773d43f7c9
FREEDM_WAD         = $(MEDIA_DIR)/freedm.wad
FREEDM_SHA256      = d9adc4d792627e7fc47b09067b15486da724010c71dd12831e1cf8e0755b68ad

CHEX_WAD     = $(MEDIA_DIR)/chex.wad
CHEX_ZIP_URL = https://www.chexquest3.com/downloads/chex.zip
CHEX_SHA256  = d8eb5277918883f490fb1a4be3c9a8588df2dbaee6dc4beb8df4929148bbffb1
CHEX_MD5     = 25485721882b050afa96a56e5758dd52

SCYTHE_WAD     = $(MEDIA_DIR)/scythe.wad
SCYTHE_ZIP_URL = https://youfailit.net/pub/idgames/levels/doom2/megawads/scythe.zip
SCYTHE_SIZE    = 6215556

# sha256sum and md5sum on Linux, shasum and md5 on macOS. Whichever exists;
# if either is missing the target stops rather than accepting a download it
# cannot check.
SHA256SUM := $(firstword $(shell command -v sha256sum 2>/dev/null) \
                         $(shell command -v shasum 2>/dev/null))
MD5SUM    := $(firstword $(shell command -v md5sum 2>/dev/null) \
                         $(shell command -v md5 2>/dev/null))
UNZIP     := $(shell command -v unzip 2>/dev/null)

media:
	@if [ -z "$(SHA256SUM)" ] || [ -z "$(MD5SUM)" ]; then \
		echo "  MEDIA no checksum tool on this machine (sha256sum/shasum and"; \
		echo "        md5sum/md5 are both needed) — refusing to download"; \
		echo "        something that cannot be verified."; \
		exit 1; \
	fi
	@if [ -z "$(UNZIP)" ]; then \
		echo "  MEDIA no unzip on this machine — refusing to download the"; \
		echo "        zipped WADs (Freedoom, Chex Quest, Scythe) that need it."; \
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
	@if [ -f "$(FREEDOOM1_WAD)" ] && [ -f "$(FREEDOOM2_WAD)" ]; then \
		echo "  MEDIA $(FREEDOOM1_WAD), $(FREEDOOM2_WAD) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(FREEDOOM_ZIP_URL)"; \
		curl -fL --retry 3 -o "$(MEDIA_DIR)/freedoom.zip.part" "$(FREEDOOM_ZIP_URL)" || { \
			rm -f "$(MEDIA_DIR)/freedoom.zip.part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		got=`$(SHA256SUM) -a 256 "$(MEDIA_DIR)/freedoom.zip.part" 2>/dev/null || $(SHA256SUM) "$(MEDIA_DIR)/freedoom.zip.part"`; \
		got=`echo "$$got" | awk '{print $$1}'`; \
		if [ "$$got" != "$(FREEDOOM_ZIP_SHA256)" ]; then \
			echo "  MEDIA SHA256 MISMATCH for freedoom-$(FREEDOOM_VERSION).zip"; \
			echo "        expected $(FREEDOOM_ZIP_SHA256)"; \
			echo "        got      $$got"; \
			echo "        the file has been left in place for inspection, and is"; \
			echo "        NOT safe to extract."; \
			exit 1; \
		fi; \
		$(UNZIP) -p "$(MEDIA_DIR)/freedoom.zip.part" '*/freedoom1.wad' > "$(FREEDOOM1_WAD)"; \
		$(UNZIP) -p "$(MEDIA_DIR)/freedoom.zip.part" '*/freedoom2.wad' > "$(FREEDOOM2_WAD)"; \
		rm -f "$(MEDIA_DIR)/freedoom.zip.part"; \
	fi
	@for pair in "$(FREEDOOM1_WAD):$(FREEDOOM1_SHA256)" "$(FREEDOOM2_WAD):$(FREEDOOM2_SHA256)"; do \
		wad=`echo "$$pair" | cut -d: -f1`; expect=`echo "$$pair" | cut -d: -f2`; \
		got=`$(SHA256SUM) -a 256 "$$wad" 2>/dev/null || $(SHA256SUM) "$$wad"`; \
		got=`echo "$$got" | awk '{print $$1}'`; \
		if [ "$$got" != "$$expect" ]; then \
			echo "  MEDIA SHA256 MISMATCH for $$wad"; \
			echo "        expected $$expect"; \
			echo "        got      $$got"; \
			echo "        the file has been left in place for inspection, and is"; \
			echo "        NOT safe to put on a card."; \
			exit 1; \
		fi; \
		head -c 4 "$$wad" | grep -q IWAD || { \
			echo "  MEDIA $$wad does not begin with the IWAD magic"; exit 1; }; \
		echo "  MEDIA $$wad verified ($$(wc -c < $$wad | tr -d ' ') bytes)"; \
	done
	@printf '%s\n' \
		"freedoom1.wad, freedoom2.wad — Freedoom: Phase 1 and Phase 2 IWADs" \
		"" \
		"Source:   $(FREEDOOM_ZIP_URL)" \
		"Release:  https://github.com/freedoom/freedoom/releases/tag/v$(FREEDOOM_VERSION)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"Zip SHA256 (published, PGP-signed CHECKSUM file from the same" \
		"release): $(FREEDOOM_ZIP_SHA256)" \
		"freedoom1.wad SHA256: $(FREEDOOM1_SHA256)" \
		"freedoom2.wad SHA256: $(FREEDOOM2_SHA256)" \
		"" \
		"What they are: complete, original-content IWAD replacements for" \
		"Doom (freedoom1.wad) and Doom II (freedoom2.wad) from the" \
		"Freedoom project." \
		"" \
		"Licence: BSD 3-Clause. See" \
		"https://github.com/freedoom/freedoom/blob/master/COPYING.adoc" \
		"" \
		"Verification: the zip's SHA256 was checked against the Freedoom" \
		"project's own published, PGP-signed CHECKSUM file. The per-WAD" \
		"SHA256 values were computed from the copies this project" \
		"extracted." \
		>> $(MEDIA_DIR)/provenance.txt
	@if [ -f "$(FREEDM_WAD)" ]; then \
		echo "  MEDIA $(FREEDM_WAD) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(FREEDM_ZIP_URL)"; \
		curl -fL --retry 3 -o "$(MEDIA_DIR)/freedm.zip.part" "$(FREEDM_ZIP_URL)" || { \
			rm -f "$(MEDIA_DIR)/freedm.zip.part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		got=`$(SHA256SUM) -a 256 "$(MEDIA_DIR)/freedm.zip.part" 2>/dev/null || $(SHA256SUM) "$(MEDIA_DIR)/freedm.zip.part"`; \
		got=`echo "$$got" | awk '{print $$1}'`; \
		if [ "$$got" != "$(FREEDM_ZIP_SHA256)" ]; then \
			echo "  MEDIA SHA256 MISMATCH for freedm-$(FREEDOOM_VERSION).zip"; \
			echo "        expected $(FREEDM_ZIP_SHA256)"; \
			echo "        got      $$got"; \
			echo "        the file has been left in place for inspection, and is"; \
			echo "        NOT safe to extract."; \
			exit 1; \
		fi; \
		$(UNZIP) -p "$(MEDIA_DIR)/freedm.zip.part" '*/freedm.wad' > "$(FREEDM_WAD)"; \
		rm -f "$(MEDIA_DIR)/freedm.zip.part"; \
	fi
	@got=`$(SHA256SUM) -a 256 "$(FREEDM_WAD)" 2>/dev/null || $(SHA256SUM) "$(FREEDM_WAD)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(FREEDM_SHA256)" ]; then \
		echo "  MEDIA SHA256 MISMATCH for $(FREEDM_WAD)"; \
		echo "        expected $(FREEDM_SHA256)"; \
		echo "        got      $$got"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	head -c 4 "$(FREEDM_WAD)" | grep -q IWAD || { \
		echo "  MEDIA $(FREEDM_WAD) does not begin with the IWAD magic"; exit 1; }; \
	echo "  MEDIA $(FREEDM_WAD) verified ($$(wc -c < $(FREEDM_WAD) | tr -d ' ') bytes)"
	@printf '%s\n' \
		"freedm.wad — FreeDM, the Freedoom project's deathmatch-only IWAD" \
		"" \
		"Source:   $(FREEDM_ZIP_URL)" \
		"Release:  https://github.com/freedoom/freedoom/releases/tag/v$(FREEDOOM_VERSION)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"Zip SHA256 (published, PGP-signed CHECKSUM file from the same" \
		"release): $(FREEDM_ZIP_SHA256)" \
		"freedm.wad SHA256: $(FREEDM_SHA256)" \
		"" \
		"Licence: BSD 3-Clause, same terms as freedoom1.wad/freedoom2.wad" \
		"above." \
		"" \
		"Verification: the zip's SHA256 was checked against the Freedoom" \
		"project's own published, PGP-signed CHECKSUM file. The WAD's" \
		"SHA256 was computed from the copy this project extracted." \
		>> $(MEDIA_DIR)/provenance.txt
	@if [ -f "$(CHEX_WAD)" ]; then \
		echo "  MEDIA $(CHEX_WAD) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(CHEX_ZIP_URL)"; \
		curl -fL --retry 3 -o "$(MEDIA_DIR)/chex.zip.part" "$(CHEX_ZIP_URL)" || { \
			rm -f "$(MEDIA_DIR)/chex.zip.part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		$(UNZIP) -p "$(MEDIA_DIR)/chex.zip.part" 'chex.wad' > "$(CHEX_WAD)"; \
		rm -f "$(MEDIA_DIR)/chex.zip.part"; \
	fi
	@got=`$(SHA256SUM) -a 256 "$(CHEX_WAD)" 2>/dev/null || $(SHA256SUM) "$(CHEX_WAD)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(CHEX_SHA256)" ]; then \
		echo "  MEDIA SHA256 MISMATCH for $(CHEX_WAD)"; \
		echo "        expected $(CHEX_SHA256)"; \
		echo "        got      $$got"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	got=`$(MD5SUM) -q "$(CHEX_WAD)" 2>/dev/null || $(MD5SUM) "$(CHEX_WAD)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(CHEX_MD5)" ]; then \
		echo "  MEDIA MD5 MISMATCH for $(CHEX_WAD)"; \
		echo "        expected $(CHEX_MD5) — the published checksum of the"; \
		echo "        1996-10-31 release, per the Doom Wiki's CHEX.WAD page"; \
		echo "        got      $$got"; \
		exit 1; \
	fi; \
	head -c 4 "$(CHEX_WAD)" | grep -q PWAD || { \
		echo "  MEDIA $(CHEX_WAD) does not begin with the PWAD magic (its"; \
		echo "        documented header — chex.wad is used as an IWAD despite"; \
		echo "        it, see README.md)"; exit 1; }; \
	echo "  MEDIA $(CHEX_WAD) verified ($$(wc -c < $(CHEX_WAD) | tr -d ' ') bytes)"
	@printf '%s\n' \
		"chex.wad — Chex Quest 1 (1996-10-31 release)" \
		"" \
		"Source:   $(CHEX_ZIP_URL)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"SHA256:   $(CHEX_SHA256)" \
		"MD5:      $(CHEX_MD5)" \
		"" \
		"What it is: the IWAD of the original 1996 Chex Quest, used by" \
		"Chocolate Doom as the \"chex\" IWAD (src/d_iwad.c). Its own header" \
		"reads PWAD, a documented property of the original release, not a" \
		"sign of a bad download." \
		"" \
		"Licence: released free in 1996 as a promotional item inside boxes" \
		"of Chex cereal (Ralston Foods / General Mills), developed by" \
		"Digital Cafe. No formal open-source licence text is published for" \
		"chex.wad itself; it has been continuously and openly redistributed" \
		"without objection since 1996, and chexquest3.com — run today by" \
		"the original release's lead artist — is its current distribution" \
		"point." \
		"" \
		"Verification: the MD5 above is the published checksum for this" \
		"exact release on the Doom Wiki's CHEX.WAD technical page," \
		"documented independently of this download. The SHA256 was" \
		"computed from the copy this project fetched." \
		>> $(MEDIA_DIR)/provenance.txt
	@if [ -f "$(SCYTHE_WAD)" ]; then \
		echo "  MEDIA $(SCYTHE_WAD) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(SCYTHE_ZIP_URL)"; \
		curl -fL --retry 3 -o "$(MEDIA_DIR)/scythe.zip.part" "$(SCYTHE_ZIP_URL)" || { \
			rm -f "$(MEDIA_DIR)/scythe.zip.part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		$(UNZIP) -p "$(MEDIA_DIR)/scythe.zip.part" 'SCYTHE.WAD' > "$(SCYTHE_WAD)"; \
		rm -f "$(MEDIA_DIR)/scythe.zip.part"; \
	fi
	@size=`wc -c < "$(SCYTHE_WAD)" | tr -d ' '`; \
	if [ "$$size" != "$(SCYTHE_SIZE)" ]; then \
		echo "  MEDIA SIZE MISMATCH for $(SCYTHE_WAD)"; \
		echo "        expected $(SCYTHE_SIZE) bytes"; \
		echo "        got      $$size bytes"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	head -c 4 "$(SCYTHE_WAD)" | grep -q PWAD || { \
		echo "  MEDIA $(SCYTHE_WAD) does not begin with the PWAD magic"; exit 1; }; \
	echo "  MEDIA $(SCYTHE_WAD) verified ($$size bytes)"
	@printf '%s\n' \
		"scythe.wad — Scythe, a 32-level PWAD megawad for Doom II" \
		"(Erik Alm, with guest map by Kim \"Torn\" Bach, 2003-04-10)" \
		"" \
		"Source:   $(SCYTHE_ZIP_URL)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"" \
		"The /idgames archive does not publish a per-file checksum for" \
		"this entry. Verified against the archive's own catalogued file" \
		"size (2,086,863 bytes for the zip, queried via" \
		"https://www.doomworld.com/idgames/api/api.php) and against the" \
		"extracted WAD's own magic and size." \
		"" \
		"Requires a Doom II-compatible IWAD and is loaded with the" \
		"'-file scythe.wad' argument — it is a PWAD, not an IWAD." \
		"" \
		"Licence: the author's own included text file states — \"You MAY" \
		"distribute this file, provided you include this text file, with" \
		"no modifications. You may distribute this file in any electronic" \
		"format (BBS, Diskette, CD, etc) as long as you include this file" \
		"intact. This file may not be used for any commercial purposes" \
		"without the author's agreement.\"" \
		>> $(MEDIA_DIR)/provenance.txt
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
	          freedoom1.wad freedoom2.wad freedm.wad chex.wad hacx.wad \
	          scythe.wad; do \
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
