/*
 * stub_pad.c — the bisection pad, compiled through the C path.
 *
 * The twin of the pad in stub_doom.cpp, byte for byte the same data, built
 * by the C rule with the C flags instead of the C++ rule with the C++ ones.
 * Nothing here is called, referenced or constructed; the two images differ
 * in which compiler produced one inert translation unit and in nothing else.
 *
 * It exists to separate "an image this size does not boot" from "an image
 * containing a C object does not boot", which no comparison between a real
 * game and a real host layer can separate on its own.
 */
#ifdef STUB_PAD_BYTES
const volatile unsigned char g_stub_pad[STUB_PAD_BYTES] = { 'P', 'A', 'D', 0 };
#endif
