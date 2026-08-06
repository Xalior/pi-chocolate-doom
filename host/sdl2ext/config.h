//
// config.h — the build configuration Chocolate Doom's sources expect.
//
// Upstream generates this file from cmake/config.h.cin (or the autotools
// equivalent) as part of configuring a desktop build. This port has neither
// step: there is one target, one set of libraries, and no probing to do, so
// the answers are written down here instead.
//
// The version and package names are copied from upstream's CMakeLists.txt.
// They must be kept in step with the pinned upstream commit — the game
// prints them, and PACKAGE_TARNAME names the directory it keeps its
// configuration in.
//
#ifndef PI_CHOCOLATE_DOOM_CONFIG_H
#define PI_CHOCOLATE_DOOM_CONFIG_H

#define PACKAGE_NAME    "Chocolate Doom"
#define PACKAGE_TARNAME "chocolate-doom"
#define PACKAGE_VERSION "3.1.1"
#define PACKAGE_STRING  "Chocolate Doom 3.1.1"
#define PROGRAM_PREFIX  "chocolate-"

// Directory reading works: circle-libsdl2 serves opendir and readdir from
// core 0, and circle_syscalls.cpp puts that service under the C library.
#define HAVE_DIRENT_H

// Both are declared by this C library.
#define HAVE_DECL_STRCASECMP  1
#define HAVE_DECL_STRNCASECMP 1

// Deliberately absent, each for the same reason: the library is not part of
// this port, and the code guarded by it does not compile here.
//
//   HAVE_MMAP           no memory mapping — the WAD is read with stdio
//                       (w_file_stdc.c) rather than mapped (w_file_posix.c)
//   HAVE_LIBSAMPLERATE  no libsamplerate
//   HAVE_LIBPNG         no libpng, so screenshots are PCX only
//   HAVE_FLUIDSYNTH     no FluidSynth

#endif
