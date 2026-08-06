//
// sdl_ext_stubs.cpp — SDL_mixer and SDL_net, as a device that never opens
// and a network stack that never starts.
//
// Chocolate Doom's sound, music and multiplayer backends are all written
// against these two libraries, and circle-libsdl2 supplies neither. Rather
// than cut the backends out of the build — which would hide the fact that
// they exist and are wanted — they are compiled as upstream wrote them,
// against the declarations in sdl2ext/, and every entry point that would
// acquire a resource fails here.
//
// Chocolate Doom handles that failure the way it handles a missing sound
// card: each module's Init returns false, the game falls through to the next
// one, and with all of them refusing it runs silent and single player,
// printing the reason as it goes.
//
// Everything below either fails or does nothing at all. Nothing returns a
// handle, so nothing downstream can call in believing it has a device.
//
#include <cstdlib>

#include <SDL2/SDL.h>
#include "SDL_mixer.h"
#include "SDL_net.h"

extern "C" {

// ---------------------------------------------------------------------------
// SDL_mixer
// ---------------------------------------------------------------------------

// Mix_Init reports which of the requested decoders are available. None are.
int Mix_Init(int) { return 0; }
void Mix_Quit(void) {}

int Mix_OpenAudioDevice(int, Uint16, int, int, const char *, int)
{
    SDL_SetError("SDL_mixer is not available on this port: no audio device was opened");
    return -1;
}

void Mix_CloseAudio(void) {}

// Zero means "no device open", which is what every caller checks for.
int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    if (frequency != nullptr) *frequency = 0;
    if (format != nullptr)    *format = 0;
    if (channels != nullptr)  *channels = 0;
    return 0;
}

int Mix_AllocateChannels(int) { return 0; }

int Mix_PlayChannel(int, Mix_Chunk *, int)
{
    SDL_SetError("no audio device: nothing was played");
    return -1;
}

int Mix_HaltChannel(int) { return 0; }
int Mix_Playing(int) { return 0; }

int Mix_RegisterEffect(int, Mix_EffectFunc_t, Mix_EffectDone_t, void *)
{
    SDL_SetError("no audio device: no effect was registered");
    return 0;
}

int Mix_UnregisterEffect(int, Mix_EffectFunc_t) { return 0; }

int Mix_SetPanning(int, Uint8, Uint8)
{
    SDL_SetError("no audio device: panning was not applied");
    return 0;
}

void Mix_HookMusic(void (SDLCALL *)(void *, Uint8 *, int), void *) {}

Mix_Music *Mix_LoadMUS(const char *)
{
    SDL_SetError("music playback is not available on this port");
    return nullptr;
}

Mix_Music *Mix_LoadMUS_RW(SDL_RWops *, int)
{
    SDL_SetError("music playback is not available on this port");
    return nullptr;
}

void Mix_FreeMusic(Mix_Music *) {}

int Mix_PlayMusic(Mix_Music *, int)
{
    SDL_SetError("music playback is not available on this port");
    return -1;
}

int Mix_PlayingMusic(void) { return 0; }
void Mix_PauseMusic(void) {}
void Mix_ResumeMusic(void) {}
int Mix_HaltMusic(void) { return 0; }
int Mix_VolumeMusic(int) { return 0; }

int Mix_SetMusicPosition(double)
{
    SDL_SetError("music playback is not available on this port");
    return -1;
}

int Mix_SetMusicCMD(const char *)
{
    SDL_SetError("there is no shell to run a music command with");
    return -1;
}

// ---------------------------------------------------------------------------
// SDL_net
// ---------------------------------------------------------------------------

int SDLNet_Init(void)
{
    SDL_SetError("networking is not available on this port");
    return -1;
}

void SDLNet_Quit(void) {}

int SDLNet_ResolveHost(IPaddress *address, const char *, Uint16 port)
{
    if (address != nullptr)
    {
        address->host = INADDR_NONE;
        address->port = port;
    }
    SDL_SetError("networking is not available on this port");
    return -1;
}

const char *SDLNet_ResolveIP(const IPaddress *) { return nullptr; }

UDPsocket SDLNet_UDP_Open(Uint16)
{
    SDL_SetError("networking is not available on this port");
    return nullptr;
}

void SDLNet_UDP_Close(UDPsocket) {}

int SDLNet_UDP_Send(UDPsocket, int, UDPpacket *)
{
    SDL_SetError("networking is not available on this port");
    return 0;      // zero destinations reached
}

int SDLNet_UDP_Recv(UDPsocket, UDPpacket *) { return 0; }   // nothing waiting

UDPpacket *SDLNet_AllocPacket(int)
{
    SDL_SetError("networking is not available on this port");
    return nullptr;
}

void SDLNet_FreePacket(UDPpacket *) {}

} // extern "C"
