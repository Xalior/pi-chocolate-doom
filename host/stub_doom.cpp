//
// stub_doom.cpp — boot-bisection payload.
//
// Links in place of Chocolate Doom's objects, so the image is the host
// scaffolding alone: the same kernel wrapper, the same world, the same shim
// archive, the same link recipe, and none of the game's code.
//
// It answers one question and only one. If this image logs, the scaffolding
// is proven and the fault is in the game's objects or in what linking them
// does to the image. If it stays silent, the fault is in the scaffolding,
// the world or the build, and nothing about the game is implicated.
//
//   make stub RAPI_BOARD=rpi5
//
// Reach for it when a board goes quiet with no output at all, which is the
// one failure that cannot be narrowed down from the serial log.
//
// STUB_PAD_BYTES grows the image without adding anything that runs. The pad
// is constant data: no code, no symbols the rest of the build can reach, no
// constructors, nothing referenced from anywhere. An image padded to the
// size of the real one therefore differs from the bare stub in exactly one
// property — how many bytes it occupies — which separates "this image is
// too big or lands somewhere bad" from "something in the game's objects
// does it".
//
//   make stub RAPI_BOARD=rpi5 STUB_PAD_BYTES=700000
//
#include <circle/logger.h>

#ifdef STUB_BSS_BYTES
// Zero-initialised, so it lands in .bss and costs the image NOTHING on disk
// — it is memory the kernel claims and clears at startup, not bytes that are
// loaded. volatile and used so nothing discards an array no one reads.
// This is the axis the real games differ on and the pads did not.
unsigned char g_stub_bss[STUB_BSS_BYTES] __attribute__ ((used));
#endif

#ifdef STUB_PAD_IN_C
extern "C" const volatile unsigned char g_stub_pad[];
#endif

#if defined(STUB_PAD_BYTES) && !defined(STUB_PAD_IN_C)
// const so it lands in .rodata, which occupies image bytes; volatile and
// used so no optimiser decides an array nobody reads can be discarded.
extern "C" const volatile unsigned char g_stub_pad[STUB_PAD_BYTES]
    __attribute__ ((used)) = { 'P', 'A', 'D', 0 };
#endif

extern "C" int doom_main(int argc, char *argv[])
{
    CLogger::Get()->Write("stub", LogNotice,
                          "stub payload reached: host scaffolding boots");
    CLogger::Get()->Write("stub", LogNotice, "argc = %d, argv[0] = %s",
                          argc, argc > 0 ? argv[0] : "(none)");
#ifdef STUB_BSS_BYTES
    g_stub_bss[0] = 1;
    CLogger::Get()->Write("stub", LogNotice, "inert bss: %u bytes, first %u",
                          (unsigned) STUB_BSS_BYTES, (unsigned) g_stub_bss[0]);
#endif
#ifdef STUB_PAD_BYTES
    CLogger::Get()->Write("stub", LogNotice, "inert pad: %u bytes (%s), first %c%c%c",
                          (unsigned) STUB_PAD_BYTES,
#ifdef STUB_PAD_IN_C
                          "compiled as C",
#else
                          "compiled as C++",
#endif
                          g_stub_pad[0], g_stub_pad[1], g_stub_pad[2]);
#endif
    return 0;
}
