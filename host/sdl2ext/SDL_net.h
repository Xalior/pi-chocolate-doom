//
// SDL_net.h — the part of SDL_net's interface Chocolate Doom uses.
//
// SDL_net is a separate library from SDL2, and circle-libsdl2 has no network
// stack behind it: Circle's networking exists, but the shim does not expose
// it and nothing in this port drives it. Chocolate Doom's multiplayer
// transport is written against SDL_net, so without something here the game
// does not compile.
//
// This header declares that interface, and sdl_net_stub.cpp implements it as
// a stack that never initialises. The consequence is exact and visible:
// Chocolate Doom's SDL network module fails to start, so the game is single
// player. Nothing here pretends to send or receive anything.
//
#ifndef PI_CHOCOLATE_DOOM_SDL_NET_H
#define PI_CHOCOLATE_DOOM_SDL_NET_H

#include "SDL.h"
#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    Uint32 host;        // 32-bit IPv4 host address, network byte order
    Uint16 port;        // 16-bit protocol port, network byte order
} IPaddress;

#define INADDR_ANY      0x00000000
#define INADDR_NONE     0xFFFFFFFF
#define INADDR_BROADCAST 0xFFFFFFFF

typedef struct
{
    int       channel;
    Uint8    *data;
    int       len;
    int       maxlen;
    int       status;
    IPaddress address;
} UDPpacket;

typedef struct _UDPsocket *UDPsocket;

#define SDLNet_GetError SDL_GetError
#define SDLNet_SetError SDL_SetError

extern DECLSPEC int SDLCALL SDLNet_Init(void);
extern DECLSPEC void SDLCALL SDLNet_Quit(void);

extern DECLSPEC int SDLCALL SDLNet_ResolveHost(IPaddress *address,
                                               const char *host, Uint16 port);
extern DECLSPEC const char *SDLCALL SDLNet_ResolveIP(const IPaddress *ip);

extern DECLSPEC UDPsocket SDLCALL SDLNet_UDP_Open(Uint16 port);
extern DECLSPEC void SDLCALL SDLNet_UDP_Close(UDPsocket sock);
extern DECLSPEC int SDLCALL SDLNet_UDP_Send(UDPsocket sock, int channel,
                                            UDPpacket *packet);
extern DECLSPEC int SDLCALL SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet);

extern DECLSPEC UDPpacket *SDLCALL SDLNet_AllocPacket(int size);
extern DECLSPEC void SDLCALL SDLNet_FreePacket(UDPpacket *packet);

// Byte-order helpers. These are plain arithmetic in SDL_net too, and are
// safe to define here because they touch nothing outside the caller's own
// memory.
SDL_FORCE_INLINE void SDLNet_Write16(Uint16 value, void *area)
{
    Uint8 *p = (Uint8 *)area;
    p[0] = (Uint8)(value >> 8);
    p[1] = (Uint8)value;
}

SDL_FORCE_INLINE void SDLNet_Write32(Uint32 value, void *area)
{
    Uint8 *p = (Uint8 *)area;
    p[0] = (Uint8)(value >> 24);
    p[1] = (Uint8)(value >> 16);
    p[2] = (Uint8)(value >> 8);
    p[3] = (Uint8)value;
}

SDL_FORCE_INLINE Uint16 SDLNet_Read16(const void *area)
{
    const Uint8 *p = (const Uint8 *)area;
    return (Uint16)(((Uint16)p[0] << 8) | (Uint16)p[1]);
}

SDL_FORCE_INLINE Uint32 SDLNet_Read32(const void *area)
{
    const Uint8 *p = (const Uint8 *)area;
    return ((Uint32)p[0] << 24) | ((Uint32)p[1] << 16)
         | ((Uint32)p[2] << 8) | (Uint32)p[3];
}

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
