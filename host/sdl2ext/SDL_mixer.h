//
// SDL_mixer.h — the part of SDL_mixer's interface Chocolate Doom uses.
//
// SDL_mixer is a separate library from SDL2, and circle-libsdl2 does not
// provide it: the shim implements SDL's own callback audio API and stops
// there. Chocolate Doom reaches for SDL_mixer for every one of its sound and
// music backends, so without something here the game does not compile at
// all.
//
// This header declares that interface, and sdl_mixer_stub.cpp implements it
// as a device that never opens. The consequence is exact and visible: every
// Chocolate Doom sound and music module reports that it could not start, the
// game selects no sound device, and it runs silent. Nothing here pretends to
// mix, play or convert anything.
//
// Adopting real audio means replacing both files — this declaration with the
// real SDL_mixer headers or a purpose-built mixer's, and the stub with an
// implementation over SDL_OpenAudioDevice, which the shim does provide.
//
#ifndef PI_CHOCOLATE_DOOM_SDL_MIXER_H
#define PI_CHOCOLATE_DOOM_SDL_MIXER_H

#include "SDL.h"
#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_MIXER_MAJOR_VERSION 2
#define SDL_MIXER_MINOR_VERSION 6
#define SDL_MIXER_PATCHLEVEL    0

#define SDL_MIXER_VERSION_ATLEAST(X, Y, Z) \
    ((SDL_MIXER_MAJOR_VERSION >= X) && \
     (SDL_MIXER_MAJOR_VERSION > X || SDL_MIXER_MINOR_VERSION >= Y) && \
     (SDL_MIXER_MAJOR_VERSION > X || SDL_MIXER_MINOR_VERSION > Y || SDL_MIXER_PATCHLEVEL >= Z))

typedef enum
{
    MIX_INIT_FLAC   = 0x00000001,
    MIX_INIT_MOD    = 0x00000002,
    MIX_INIT_MP3    = 0x00000008,
    MIX_INIT_OGG    = 0x00000010,
    MIX_INIT_MID    = 0x00000020,
    MIX_INIT_OPUS   = 0x00000040
} MIX_InitFlags;

#define MIX_CHANNELS        8
#define MIX_MAX_VOLUME      SDL_MIX_MAXVOLUME
#define MIX_CHANNEL_POST    (-2)

// A loaded sound effect. Chocolate Doom embeds one of these in each of its
// cached sounds and fills the fields itself, so the layout matters.
typedef struct Mix_Chunk
{
    int    allocated;
    Uint8 *abuf;
    Uint32 alen;
    Uint8  volume;      // 0 to MIX_MAX_VOLUME
} Mix_Chunk;

typedef struct _Mix_Music Mix_Music;

typedef void (SDLCALL *Mix_EffectFunc_t)(int chan, void *stream, int len, void *udata);
typedef void (SDLCALL *Mix_EffectDone_t)(int chan, void *udata);

#define Mix_GetError    SDL_GetError
#define Mix_SetError    SDL_SetError

extern DECLSPEC int SDLCALL Mix_Init(int flags);
extern DECLSPEC void SDLCALL Mix_Quit(void);

extern DECLSPEC int SDLCALL Mix_OpenAudioDevice(int frequency, Uint16 format,
                                                int channels, int chunksize,
                                                const char *device,
                                                int allowed_changes);
extern DECLSPEC void SDLCALL Mix_CloseAudio(void);
extern DECLSPEC int SDLCALL Mix_QuerySpec(int *frequency, Uint16 *format,
                                          int *channels);

extern DECLSPEC int SDLCALL Mix_AllocateChannels(int numchans);
extern DECLSPEC int SDLCALL Mix_PlayChannel(int channel, Mix_Chunk *chunk,
                                            int loops);
extern DECLSPEC int SDLCALL Mix_HaltChannel(int channel);
extern DECLSPEC int SDLCALL Mix_Playing(int channel);

extern DECLSPEC int SDLCALL Mix_RegisterEffect(int chan, Mix_EffectFunc_t f,
                                               Mix_EffectDone_t d, void *arg);
extern DECLSPEC int SDLCALL Mix_UnregisterEffect(int channel,
                                                 Mix_EffectFunc_t f);
extern DECLSPEC int SDLCALL Mix_SetPanning(int channel, Uint8 left, Uint8 right);

extern DECLSPEC void SDLCALL Mix_HookMusic(void (SDLCALL *mix_func)(void *udata, Uint8 *stream, int len),
                                           void *arg);

extern DECLSPEC Mix_Music *SDLCALL Mix_LoadMUS(const char *file);
extern DECLSPEC Mix_Music *SDLCALL Mix_LoadMUS_RW(SDL_RWops *src, int freesrc);
extern DECLSPEC void SDLCALL Mix_FreeMusic(Mix_Music *music);
extern DECLSPEC int SDLCALL Mix_PlayMusic(Mix_Music *music, int loops);
extern DECLSPEC int SDLCALL Mix_PlayingMusic(void);
extern DECLSPEC void SDLCALL Mix_PauseMusic(void);
extern DECLSPEC void SDLCALL Mix_ResumeMusic(void);
extern DECLSPEC int SDLCALL Mix_HaltMusic(void);
extern DECLSPEC int SDLCALL Mix_VolumeMusic(int volume);
extern DECLSPEC int SDLCALL Mix_SetMusicPosition(double position);
extern DECLSPEC int SDLCALL Mix_SetMusicCMD(const char *command);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
