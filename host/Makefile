#
# Chocolate Doom as a bootable Raspberry Pi kernel image.
#
# One board per invocation (RAPI_BOARD, default rpi5). The board selects
# three things at once: the circle-stdlib world the kernel is compiled and
# linked against, the shim archive built against that world, and — through
# the world's Config.mk — the CPU tuning and RASPPI value. Each board's
# newlib and libc++ are built for its own architecture, so an image is
# never portable between boards.
#
#   make                     the default board's image
#   make RAPI_BOARD=rpi3     that board's image
#   make clean-board         drop just this board's build/<board>/ tree
#
# Everything this invocation generates lands under build/<board>/: the
# kernel's own objects, every Chocolate Doom object, and the final
# .elf/.map/.lst/.img. The object names are shared across boards but their
# contents are not (different -mcpu, different -DRASPPI, different world),
# so a flat build directory would silently relink one board's objects into
# another board's kernel — a corrupt image with no build error. A directory
# per board keeps all three isolated: no `make clean` between boards, and
# three boards can build at the same time without touching each other.
#
# Chocolate Doom's own source tree (../chocolate-doom) is never written
# into: it is read-only input, and every object it produces is redirected
# here.
#
# Prerequisite: the board's world and shim archive exist —
# `make -C ../circle-libsdl2 deps`, or `make deps` at the repository root.
#

include ../mk/toolchain.mk

BOARDS      = rpi3 rpi4 rpi5
RAPI_BOARD ?= rpi5

ROOT = ..
# WHERE THE SHIM COMES FROM.
#
# Unset, this repository's own pinned copy — what a clone of this repository
# gets, and what CI builds. A parent proving one shim across several games
# points SHIM at the single checkout it is proving, so the library is built
# once rather than once per game.
SHIM ?= $(ROOT)/circle-libsdl2
DOOM = $(ROOT)/chocolate-doom

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))

# Before the world is included, so an unknown board is named as an unknown
# board. After the include it arrives as a missing Config.mk under a directory
# whose name nobody recognises.
ifeq ($(filter $(RAPI_BOARD),$(BOARDS)),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected one of: $(BOARDS))
endif

# The board's circle-stdlib world. A `make deps` that has not been run, or has
# not finished, leaves this file absent; make would then report that it has no
# rule to make a Config.mk, which describes the symptom and not the cause.
# WHERE THE BUILT WORLD COMES FROM.
#
# The default is this game's own nested shim, so a clone of this repository
# alone builds and uses its own worlds and nothing needs setting. A parent
# that keeps ONE set of built worlds for several games — they are gigabytes
# each, and identical when the pin and the configuration match — points
# CIRCLE_WORLDS at them.
CIRCLE_WORLDS ?= $(SHIM)
WORLD = $(CIRCLE_WORLDS)/circle-stdlib-$(RAPI_BOARD)
ifeq ($(wildcard $(WORLD)/Config.mk),)
$(error the $(RAPI_BOARD) circle-stdlib world is not configured ($(WORLD)/Config.mk is missing) — run 'make deps-$(RAPI_BOARD)' at the repository root)
endif

include $(WORLD)/Config.mk

OBJDIR = build/$(RAPI_BOARD)

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# The kernel wrapper and the syscall layer: C++, and this project's own code.
# Nothing here implements any part of SDL — every SDL function the game calls
# comes from the shim archive.
HOST_SRCS = main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# Chocolate Doom's source list for the Doom executable, as upstream's
# src/CMakeLists.txt enumerates it, minus the files that describe a machine
# this is not:
#
#   i_videohr.c    the high-resolution startup screen, which only Hexen uses
#   w_file_posix.c the memory-mapped WAD reader, replaced by w_file_stdc.c
#                  because there is no mmap here (see config.h)
#   w_file_win32.c the Windows WAD reader
#   i_winmusic.c   Windows MIDI output
#   i_flmusic.c    FluidSynth
#
# Every remaining file is compiled exactly as upstream wrote it.
DOOM_COMMON = \
	i_main.c i_system.c m_argv.c m_misc.c

DOOM_GAME = \
	aes_prng.c d_event.c d_iwad.c d_loop.c d_mode.c deh_str.c gusconf.c \
	i_cdmus.c i_endoom.c i_glob.c i_input.c i_joystick.c i_musicpack.c \
	i_oplmusic.c i_pcsound.c i_sdlmusic.c i_sdlsound.c i_sound.c \
	i_timer.c i_video.c midifallback.c midifile.c mus2mid.c m_bbox.c \
	m_cheat.c m_config.c m_controls.c m_fixed.c net_client.c \
	net_common.c net_dedicated.c net_gui.c net_io.c net_loop.c \
	net_packet.c net_petname.c net_query.c net_sdl.c net_server.c \
	net_structrw.c p_rejectpad.c sha1.c memio.c tables.c v_diskicon.c \
	v_video.c w_checksum.c w_main.c w_wad.c w_file.c w_file_stdc.c \
	w_merge.c z_zone.c

DOOM_DEHACKED = \
	deh_io.c deh_main.c deh_mapping.c deh_text.c

# The Doom game library (src/doom/), as its own CMakeLists.txt enumerates it.
DOOM_ENGINE = $(addprefix doom/, \
	am_map.c deh_ammo.c deh_bexstr.c deh_cheat.c deh_doom.c deh_frame.c \
	deh_misc.c deh_ptr.c deh_sound.c deh_thing.c deh_weapon.c d_items.c \
	d_main.c d_net.c doomdef.c doomstat.c dstrings.c f_finale.c f_wipe.c \
	g_game.c hu_lib.c hu_stuff.c info.c m_menu.c m_random.c p_ceilng.c \
	p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c \
	p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c \
	p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c \
	r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c s_sound.c \
	sounds.c statdump.c st_lib.c st_stuff.c wi_stuff.c doom_icon.c)

DOOM_SRCS = $(addprefix src/,$(DOOM_COMMON) $(DOOM_GAME) $(DOOM_DEHACKED) \
	$(DOOM_ENGINE))

# The OPL emulation, the PC speaker emulation and the text-mode screen
# library, each as its own CMakeLists.txt enumerates it. The platform files
# in each — direct hardware OPL access, Linux and BSD speaker ioctls, the
# Windows backends — are compiled: every one is guarded by a define this
# build does not set, so each produces an empty object and says in its own
# source why.
#
# Two are left out because their guards do not cover them:
#
#   opl/opl_timer.c        only the hardware OPL backends call it, and it
#                          needs threads that this port does not have
#   textscreen/txt_fileselect.c
#                          runs a file-chooser program with fork and execv,
#                          and there is no process to fork on a bare-metal
#                          board. Only the setup tool calls it.
OPL_SRCS = $(addprefix opl/, \
	opl.c opl_linux.c opl_obsd.c opl_queue.c opl_sdl.c opl_win32.c \
	ioperm_sys.c opl3.c)

PCSOUND_SRCS = $(addprefix pcsound/, \
	pcsound.c pcsound_bsd.c pcsound_sdl.c pcsound_linux.c pcsound_win32.c)

TXT_SRCS = $(addprefix textscreen/, \
	txt_conditional.c txt_checkbox.c txt_desktop.c txt_dropdown.c \
	txt_gui.c txt_inputbox.c txt_io.c txt_button.c txt_label.c \
	txt_radiobutton.c txt_scrollpane.c txt_separator.c txt_spinctrl.c \
	txt_sdl.c txt_strut.c txt_table.c txt_utf8.c txt_widget.c \
	txt_window.c txt_window_action.c)

CD_SRCS = $(DOOM_SRCS) $(OPL_SRCS) $(PCSOUND_SRCS) $(TXT_SRCS)
CD_OBJS = $(addprefix $(OBJDIR)/cd/,$(CD_SRCS:.c=.o))

OBJS = $(HOST_OBJS) $(CD_OBJS)
DEPS = $(OBJS:.o=.d)

# Dependency files are written as a side effect of compiling rather than by
# rules of their own, and Circle's Rules.mk is told to keep its hands off
# (CHECK_DEPS) so its `-M -MG` .d rules are not defined for these objects.
#
# -MG lists a header make has no rule for as a prerequisite of the object,
# which stops the build; GNU make 3.81 stops with exit 2 and prints not one
# line saying why, which is why this build asks for 4.0 in mk/toolchain.mk. A
# .d left over from an earlier build naming a header that has since moved or
# been deleted stops it the same way, and is why "delete the board's object
# directory" kept being the cure.
#
# -MD writes the dependency file alongside the object it belongs to, so the
# two can never disagree about the flags that produced them, and -MP adds a
# rule with no recipe for every header named, so a header that disappears
# means a rebuild instead of a dead stop.
CHECK_DEPS = 0
DEPFLAGS   = -MD -MP

STANDARD = -std=gnu++17

# Stated, not inherited: this is Circle's own default, written down so the
# build's optimization level is a fact in the file rather than a forensic
# reconstruction.
OPTIMIZE = -O2

include $(CIRCLEHOME)/Rules.mk

# The world must be the multicore one circle-libsdl2 configures: the shim's
# core-split machinery cannot exist without it, and an object compiled
# against a single-core world links without complaint and then misbehaves.
# Every object here inherits the flag through Rules.mk's CPPFLAGS, so this
# check covers the whole build.
ifeq ($(findstring ARM_ALLOW_MULTI_CORE,$(DEFINE)),)
$(error the payload requires the multicore circle-stdlib world — build it with 'make -C ../circle-libsdl2 deps')
endif

# WHERE THIS GAME LIVES ON THE CARD. One card carries several games, and two
# of them writing a `default.cfg` or a `settings.ini` into the FAT root would
# each silently overwrite the other's. Every game therefore keeps to a
# directory of its own, and nothing of this port's ever touches the root.
#
# It is a build-time constant, not a boot switch: a card's layout is decided
# when the card is staged, and a game that could be relocated at boot would
# only mean two places to look for the same file.
#
# The path is used ONLY by this port's own host layer — argv, the working
# directory, and SDL's preference path. Upstream Chocolate Doom is never told
# about it and needs no patch: it looks where it is pointed.
RAPI_GAME_DIR ?= /games/doom
DEFINE += -DRAPI_GAME_DIR='"$(RAPI_GAME_DIR)"'

# Chocolate Doom writes `#include "SDL.h"`, the spelling a desktop
# sdl2-config include path provides, so the shim's SDL2/ directory goes on
# the path in its own right alongside the SDL2/-prefixed one this project's
# own code uses.
#
# sdl2ext/ carries config.h, which upstream's own build system generates
# while configuring a desktop build and which this port writes down instead.
#
# The game's own directories are on the path the way its build system puts
# them there: src/ for the shared code, and the library directories for the
# OPL, PC speaker and text-screen headers.
INCLUDE := -I $(SHIM)/include -I $(SHIM)/include/SDL2 \
	-I $(CURDIR)/sdl2ext \
	-I $(DOOM)/src -I $(DOOM)/opl -I $(DOOM)/pcsound -I $(DOOM)/textscreen \
	$(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

# The C sources are C99, which is what upstream's build asks for. It is
# stated here rather than added to CFLAGS because Rules.mk folds CFLAGS into
# the C++ flags as well, and a C standard handed to the C++ compiler is a
# warning on every C++ file in the build. $(STANDARD) above covers those.
DOOM_CFLAGS = -std=gnu99 $(RAPI_CFLAGS) $(DOOM_DEFINE) $(CFLAGS)

# SDL_net is a separate library from SDL2 and circle-libsdl2 does not provide
# it: there is no network stack behind the shim. Chocolate Doom carries a
# build for exactly this case, and this is the switch that selects it —
# upstream's own, offered by its CMakeLists as ENABLE_SDL2_NET.
#
# What it changes is which code exists. The network transport compiles to a
# version that reports it cannot start, so the game runs single player and
# says why. Nothing is cut out of the build and nothing here has to imitate a
# library it does not have.
#
# SDL_mixer's switch is deliberately NOT here. The shim implements SDL_mixer
# in the same archive as the rest of SDL, so sound effects come from the
# ordinary build with nothing cut out.
#
# Every music backend is built too, the OPL one included — the game emulates
# an OPL2 chip itself, so it needs no MIDI synthesiser from anyone. Which
# music backend runs, if any, is chosen by snd_musicdevice in
# chocolate-doom.cfg, and that file says what today's choice is and why.
DOOM_DEFINE = -DDISABLE_SDL2NET=1

# The shim comes in whole, every member, rather than only the members the
# link happens to need. An object file linked straight into the kernel beats
# an archive member of the same name and does it silently, so a local copy of
# an SDL function left behind after the library implemented the real thing
# would quietly win — the game would keep calling the copy and nothing would
# say so. Pulling the archive in whole turns that into a duplicate-symbol
# error at link time, which is the only moment it can still be cheap to fix.
#
# This guard has one blind spot, and it is why the --wrap note below exists:
# a wrapped name is a renamed reference and never a second definition, so no
# duplicate can arise for the linker to object to.
SHIM_LIB = $(SHIM)/libSDL2-$(RAPI_BOARD).a

LIBS = --whole-archive $(SHIM_LIB) --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# The shim archive is built in circle-libsdl2 and never here — it is a shared
# dependency, and a rule here would rebuild it at whatever board and crossing
# count this makefile happens to be running with. This rule exists only for the
# case where it is absent: it runs then, and only then, because a target with a
# recipe and no prerequisites is up to date as soon as the file exists.
$(SHIM_LIB):
	@echo "$@ is missing: the $(RAPI_BOARD) shim archive has not been built."
	@echo "Run: $(MAKE) -C $(SHIM) libSDL2-$(RAPI_BOARD).a BOARD=$(RAPI_BOARD)"
	@exit 1

# ---------------------------------------------------------------------------
# Filesystem syscalls, redirected at link time
# ---------------------------------------------------------------------------
#
# The game runs on the application core, and newlib's Circle glue drives
# FatFs and the SD card straight from wherever it is called — which off core
# 0 is illegal. circle_syscalls.cpp puts the shim's any-core I/O service
# underneath the C library instead; these flags are what put it there.
#
# --wrap rather than redefinition, because redefinition cannot work here:
# the glue defines _open, _read, _write, _close, _lseek, _fstat, opendir,
# readdir, closedir, dup and more in a SINGLE object file. Redefining part
# of that set either collides at link time or, worse, leaves the originals
# linked in beside the replacements with no diagnostic. --wrap renames the
# references instead and leaves the vendored glue untouched — and it is what
# keeps __real_* reachable, which is how the service performs the real work
# once it has reached core 0.
#
# Every name here must also appear in circle_syscalls.cpp: a wrap with no
# __wrap_ definition is an undefined reference, so the link is the check.
WRAPPED_SYSCALLS = \
	_open _close _read _write _lseek _fstat _stat lstat _unlink _rename \
	_fcntl mkdir chdir getcwd dup dup2 \
	opendir readdir closedir rewinddir

# NOTHING OF SDL IS WRAPPED HERE, and nothing of SDL ever should be. A --wrap
# renames a reference rather than defining a second symbol, so the whole-
# archive link above — which turns a leftover local copy of a library function
# into a duplicate-symbol error — cannot see one. A wrapped SDL function is
# therefore the one kind of private SDL that could sit here for years without
# anything saying so. Whatever the game needs from SDL, the library provides;
# where it does not, the library is what gets fixed.

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS))

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# This directory's own C sources — only the bisection pad — built with the
# same compiler and the same flags the game's C files get, which is the whole
# point of it.
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@echo "  CC    $@"
	@$(CC) $(DOOM_CFLAGS) $(DEPFLAGS) -c -o $@ $<

# Chocolate Doom's objects, mirroring its source tree under $(OBJDIR)/cd/.
# The source tree itself is the upstream submodule, and is never written to.
$(OBJDIR)/cd/%.o: $(DOOM)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(DOOM_CFLAGS) $(DEPFLAGS) -c -o $@ $<

# Chocolate Doom's main() is the program's entry point on a desktop, but
# here main() belongs to the Circle kernel (main.cpp in this directory),
# which calls the game once the board is up. Renaming the entry point
# through the preprocessor for this one translation unit keeps upstream's
# source untouched, and the kernel calls the new name.
$(OBJDIR)/cd/src/i_main.o: $(DOOM)/src/i_main.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@ (entry point renamed)"
	@$(CC) $(DOOM_CFLAGS) $(DEPFLAGS) -Dmain=doom_main -c -o $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)

-include $(DEPS)

# The image link rule. sdl-app.mk lets the caller choose the linker script,
# so this build supplies its own: the shim's script with room made at image
# offset 0x800 for the patchable-defaults block. Everything else about the
# script — including the TLS-section adjacency binutils 2.44 and later
# require — is inherited from the shim's.
SDL_APP_LDSCRIPT = $(CURDIR)/chocolate-doom-defaults.ld
include $(SHIM)/sdl-app.mk

# The other half of the guarantee the linker script's ASSERTs begin: check
# the BYTES of the finished image, not just the link. A pre-boot writer
# refuses to stamp an image whose magic is not at 0x800, so an image that
# lost it would boot and then quietly ignore everything ever sent to it —
# including --rapi-debug-uart, which is the channel used to drive the board
# when nobody is at the bench. Better to have no image than that one.
.PHONY: image
image: $(TARGET).img
	@if [ "`dd if=$(TARGET).img bs=4 skip=512 count=1 2>/dev/null`" = "PM8D" ]; \
	then \
		echo "  GATE  $(TARGET).img defaults block at 0x800: present"; \
	else \
		echo "  GATE  $(TARGET).img: defaults block ABSENT at 0x800 — image deleted"; \
		rm -f $(TARGET).img; \
		exit 1; \
	fi

# Board-scoped clean: drop just this board's tree, leaving the other two
# boards' images and objects untouched.
.PHONY: clean-board
clean-board:
	rm -rf $(OBJDIR) build/$(RAPI_BOARD)-stub

# This board from nothing. An incremental build is a decision made from
# timestamps, and a timestamp says when a file was written rather than what is
# in it; anything read off the resulting image describes what the last build
# happened to put there. Take a measurement that has to be right off one of
# these.
.PHONY: rebuild
rebuild:
	+@$(NOT_DRY_RUN)
	@$(MAKE) clean-board RAPI_BOARD=$(RAPI_BOARD)
	@$(MAKE) image RAPI_BOARD=$(RAPI_BOARD)
