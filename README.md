# pi-chocolate-doom

**Doom running directly on a Raspberry Pi with no operating system.** The board
powers on and the game is what boots: no Linux, no desktop, no launcher, and
nothing else running beside it.

It builds for the Raspberry Pi 3, Pi 4 and Pi 5, all three from one source
tree.

![Doom running on a Raspberry Pi 5 with no operating system](docs/doom-on-bare-metal.jpg)

*Captured from the Pi 5's HDMI output. The board is running this image and
nothing else — no kernel underneath it, no window system, no launcher.*

## What this is

[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom) is a
faithful recreation of the original 1993 Doom engine, written to be portable
and built on SDL2. This repository is the thin layer that lets it run with
nothing underneath: a [Circle](https://github.com/rsta2/circle) kernel that
brings the board up, and
[circle-libsdl2](https://github.com/Xalior/circle-libsdl2), an SDL2
implementation built on Circle's bare-metal drivers.

The game's own source is not copied or modified here. It is a submodule,
pinned at an upstream commit, and the build reads it without ever writing to
it. Where the game needs something the SDL2 layer does not provide, this
repository supplies it in `host/` rather than changing the game.

The game draws at 320x200, the size Doom has always drawn at, and the picture
is scaled once onto whatever your screen actually is.

## What works

The game plays at full speed on all three boards.

- **Picture.** The full 320x200 rendering Doom has always had, scaled to your
  screen.
- **Sound and music.** Sound effects, and the music through Doom's own
  emulation of the OPL2 chip it was written for — no external synthesiser and
  no MIDI hardware involved.
- **Keyboard and mouse.** Both, including mouse movement for turning.
- **Saved games and settings.** Written back to the SD card, so they survive
  a power cut.

Multiplayer is the one thing missing: the network game is not built, so this
is single player.

## What you need to supply

**This repository contains no game data, and cannot.** Doom's levels,
graphics and sounds live in WAD files that are not part of the engine and are
not this project's to distribute. Building the images does not download any,
and neither does writing a card.

You need at least one IWAD file. The engine recognises several:

| File | What it is |
|---|---|
| `doom1.wad` | The Doom shareware episode. Freely redistributable by id Software's own terms, and a complete playable game on its own. |
| `freedoom1.wad`, `freedoom2.wad` | [Freedoom](https://freedoom.github.io/)'s Phase 1 and Phase 2 — complete, original-content replacements for Doom and Doom II, BSD 3-Clause licensed. |
| `freedm.wad` | Freedoom's deathmatch-only IWAD, same licence. |
| `chex.wad` | Chex Quest 1, freeware since its original 1996 release. |
| `doom.wad` | The full registered Doom, or The Ultimate Doom. |
| `doom2.wad` | Doom II. |
| `tnt.wad`, `plutonia.wad` | Final Doom. |

Where to get one legitimately:

```sh
make media
```

fetches the freely redistributable set — `doom1.wad`, `freedoom1.wad`,
`freedoom2.wad`, `freedm.wad` and `chex.wad` — into `media/`, verifying each
against a published checksum
where one exists and against the file's own magic and size otherwise. It
writes `media/provenance.txt` naming exactly where each file came from and
under what licence. Re-running it re-verifies what is already there rather
than downloading again.

**Every other IWAD is a commercial product.** Use the copy inside a version
you own — the Steam, GOG or disc release all install the WAD as a plain
file — and copy that file into `media/` by hand; `make media` does not fetch
it and does not try to. Do not use a copy obtained by working around a
licence, a paywall or a copy-protection system.

### Several IWADs on one card

`make card` stages every recognised WAD it finds in `media/`, not just one,
so a card can carry the whole set — the port picks between them at boot.

Chocolate Doom checks a fixed list of names in this order, and the first one
that exists in the game's directory on the card wins:

    doom2.wad, plutonia.wad, tnt.wad, doom.wad, doom1.wad, doom2f.wad,
    chex.wad, hacx.wad, freedoom2.wad, freedoom1.wad, freedm.wad,
    heretic.wad, heretic1.wad, hexen.wad, strife1.wad

So a card carrying only the free set fetched by `make media` boots
`doom1.wad` by default — it is earlier in the table than `chex.wad`,
`freedoom2.wad`, `freedoom1.wad` and `freedm.wad`, all of which are present.

Which one boots is decided by that table and nothing else, so the way to
choose is to put only the IWAD you want to play in the game's directory on
the card and leave the others off it.

## Building

You need a Linux or macOS machine, GNU make, and the Arm GNU toolchain for
`aarch64-none-elf` (release 15.2.Rel1). Put its `bin` directory on your
`PATH`, or unpack it into `toolchains/` in this repository.

```sh
git clone --recursive https://github.com/Xalior/pi-chocolate-doom.git
cd pi-chocolate-doom
make deps       # long: builds newlib and libc++ from source, once per board
make kernels    # the three board images
make verify     # confirms each image exists and is not empty
```

`make deps` is the slow step, and it is slow once. It builds a complete C and
C++ world for each board, because each board's world is compiled for its own
processor.

Part of that world is libc++, whose sources are fetched from a git tag that
carries the bare-metal patches. That tag is hosted on Codeberg, which is small
and volunteer run. One copy is enough for every board and for every project on
your machine, so tell the build where to keep it and it is fetched once:

```sh
make deps CIRCLE_LLVM=/path/to/circle-llvm
```

The default puts that checkout beside this repository, which is the right
answer for a plain clone or a continuous-integration runner and needs no
setting at all.

The images land in `host/build/<board>/`:

| Board | Image |
|---|---|
| Pi 3 | `host/build/rpi3/kernel8.img` |
| Pi 4 | `host/build/rpi4/kernel8-rpi4.img` |
| Pi 5 | `host/build/rpi5/kernel_2712.img` |

Building one board on its own is `make rpi5`, and its dependencies alone are
`make deps-rpi5`, which is what a machine without room for three worlds
wants.

## Putting it on a card

```sh
make card
```

That stages the card into `build/sd-card/` for you to copy onto FAT32 media:
the three kernel images under the names each board's firmware looks for, the
boot configuration, the game's two configuration files in `doom/`, and every
recognised WAD found in `media/` (run `make media` first — see above — or
copy a commercial WAD there by hand). `make card` names any WAD it did not
find rather than failing silently.

One thing is not staged and has to be added by hand:

- **The Raspberry Pi firmware files** — `bootcode.bin`, `start*.elf`,
  `fixup*.dat` and, for the Pi 4, `armstub8-rpi4.bin`. Take them from a
  Raspberry Pi OS card or from the
  [firmware repository](https://github.com/raspberrypi/firmware).

### The configuration files

`doom/chocolate-doom.cfg` is staged with settings this port needs, and each
one is commented in the file itself. One of them matters more than the
others: **`smooth_pixel_scaling` must stay at 0.** With it switched on the
game runs normally and the screen stays black.

Chocolate Doom rewrites both configuration files when it exits, so a change
made on the card is the starting point rather than a permanent setting.

### The thermal settings in `cmdline.txt`

One card boots any of the three boards, so all three read the same
`cmdline.txt`. It carries `socmaxtemp=70`, the temperature in degrees Celsius
at which the processor is slowed down to cool itself.

If your board has a fan, add `gpiofanpin=` and the GPIO pin it is wired to —
`gpiofanpin=45` is a Raspberry Pi 5 Case Fan or Active Cooler. Naming a fan
pin changes what happens at that temperature: the fan is switched on and the
processor is left at full speed, instead of being slowed down. That is what a
game wants, because a slowed processor drops frames.

## License

The code in this repository — the kernel layer in `host/` and the build — is
released under the GNU Lesser General Public License, version 3. See
[LICENSE](LICENSE).

The submodules are other people's work and carry their own terms, and both
matter before you distribute anything you build here:

- **Chocolate Doom** is released under the GNU General Public License,
  version 2 or later.
- **Circle** is released under the GNU General Public License, version 3.

Building a kernel image here combines all of them, and the result is covered
by the GNU General Public License, version 3. Doing that for yourself is
straightforward; redistributing the result means satisfying every one of
those terms at once, including supplying complete source.

Doom is a trademark of id Software LLC. This project is not affiliated with
id Software or ZeniMax.
