//
// circle_stubs.cpp — the SDL2 surface layer and the odd entry points
// Chocolate Doom references that circle-libsdl2 does not implement.
//
// circle-libsdl2 renders from textures alone: its SDL_Surface is a 32-bit
// staging buffer and nothing in the library blits, converts or fills one.
// Chocolate Doom draws the way Doom has always drawn — into an 8-bit
// paletted buffer, converted to 32-bit once a frame on its way to a
// streaming texture — so that conversion has to exist somewhere. It exists
// here, in this port's own layer, rather than in the game or in the library.
//
// Two of these functions REPLACE a library function instead of adding one:
// SDL_CreateRGBSurface and SDL_FreeSurface exist in the shim and refuse
// anything but 32 bits. They are reached through the linker's --wrap (see
// the WRAPPED_SDL list in the Makefile), so the library's own versions stay
// in place and still do the 32-bit work — this file only adds the paletted
// case on top and hands everything else straight back. Redefining them
// outright would be a duplicate symbol at best and a silent shadow at worst.
//
// Everything else here is an addition. Each one either does the job
// properly or fails honestly — returns an error, returns null — so that
// nothing pretends to work. Where a function is a deliberate no-op it says
// why: on a bare-metal board with one fullscreen display there is nothing
// for it to do.
//
// These are seams, not permanent furniture. When the shim implements one of
// these for real, the way to adopt it is to DELETE the stub here: the
// archive is linked whole, so a leftover stub becomes a duplicate-symbol
// error at link time rather than a silent winner over the real thing.
//
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL2/SDL.h>

extern "C" {

// The library's own versions of the two wrapped functions.
SDL_Surface *__real_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask);
void __real_SDL_FreeSurface(SDL_Surface *surface);

// ---------------------------------------------------------------------------
// Surfaces this file owns
// ---------------------------------------------------------------------------
//
// A surface made here carries allocations the library knows nothing about —
// a palette, a heap pixel format, sometimes the pixels — so freeing it is
// this file's job too. Rather than guess from the surface's contents, every
// one made here is recorded, and the free path looks it up: found means ours
// and freed our way, not found means the library's and handed back to it.
//
// The list is short by construction. Chocolate Doom holds one paletted
// screen buffer, one 32-bit staging surface over the streaming texture, and
// a window icon it frees immediately.

struct OwnedSurface
{
    SDL_Surface     *surface;
    SDL_PixelFormat *format;      // always heap, always ours
    SDL_Palette     *palette;     // null for the direct-colour formats
    bool             owns_pixels; // false when the caller supplied them
    OwnedSurface    *next;
};

static OwnedSurface *s_owned = nullptr;

// Fill in a heap pixel format for one of the two layouts this port needs.
// depth 8 gets a palette; depth 32 is ARGB8888, which is byte-for-byte what
// the shim's streaming textures take.
static SDL_PixelFormat *MakeFormat(int depth)
{
    SDL_PixelFormat *fmt = (SDL_PixelFormat *)calloc(1, sizeof(SDL_PixelFormat));
    if (fmt == nullptr)
        return nullptr;

    fmt->BitsPerPixel  = (Uint8)depth;
    fmt->BytesPerPixel = (Uint8)(depth / 8);
    fmt->refcount      = 1;

    if (depth == 8)
    {
        fmt->format = SDL_PIXELFORMAT_INDEX8;
    }
    else
    {
        fmt->format = SDL_PIXELFORMAT_ARGB8888;
        fmt->Rmask  = 0x00FF0000;
        fmt->Gmask  = 0x0000FF00;
        fmt->Bmask  = 0x000000FF;
        fmt->Amask  = 0xFF000000;
        fmt->Rshift = 16;
        fmt->Gshift = 8;
        fmt->Bshift = 0;
        fmt->Ashift = 24;
    }
    return fmt;
}

static SDL_Palette *MakePalette(void)
{
    SDL_Palette *pal = (SDL_Palette *)calloc(1, sizeof(SDL_Palette));
    if (pal == nullptr)
        return nullptr;

    pal->colors = (SDL_Color *)calloc(256, sizeof(SDL_Color));
    if (pal->colors == nullptr)
    {
        free(pal);
        return nullptr;
    }
    pal->ncolors  = 256;
    pal->refcount = 1;
    // Opaque black until the game sets a real palette. A surface whose
    // palette had zero alpha throughout would convert to a fully
    // transparent picture and read as "the game drew nothing".
    for (int i = 0; i < 256; i++)
        pal->colors[i].a = 0xFF;
    return pal;
}

// The one place a surface is built. pixels == nullptr with prealloc set is
// legitimate and is what Chocolate Doom does for its staging surface: it
// points the surface at a locked texture's memory a frame at a time.
static SDL_Surface *NewOwnedSurface(int width, int height, int depth,
                                    void *pixels, int pitch)
{
    if (width <= 0 || height <= 0 || (depth != 8 && depth != 32))
    {
        SDL_SetError("unsupported surface: %dx%d at %d bits", width, height, depth);
        return nullptr;
    }

    const bool prealloc = (pixels != nullptr) || (pitch != 0);

    SDL_Surface *surface = (SDL_Surface *)calloc(1, sizeof(SDL_Surface));
    OwnedSurface *rec    = (OwnedSurface *)calloc(1, sizeof(OwnedSurface));
    SDL_PixelFormat *fmt = MakeFormat(depth);
    SDL_Palette *pal     = (depth == 8) ? MakePalette() : nullptr;

    if (surface == nullptr || rec == nullptr || fmt == nullptr
        || (depth == 8 && pal == nullptr))
    {
        if (pal != nullptr) { free(pal->colors); free(pal); }
        free(fmt);
        free(rec);
        free(surface);
        SDL_SetError("out of memory allocating surface");
        return nullptr;
    }

    fmt->palette = pal;

    if (pitch == 0)
        pitch = width * fmt->BytesPerPixel;

    if (!prealloc)
    {
        pixels = calloc(1, (size_t)pitch * height);
        if (pixels == nullptr)
        {
            free(pal ? pal->colors : nullptr);
            free(pal);
            free(fmt);
            free(rec);
            free(surface);
            SDL_SetError("out of memory allocating surface pixels");
            return nullptr;
        }
    }

    surface->flags     = prealloc ? SDL_PREALLOC : 0;
    surface->format    = fmt;
    surface->w         = width;
    surface->h         = height;
    surface->pitch     = pitch;
    surface->pixels    = pixels;
    surface->clip_rect = { 0, 0, width, height };
    surface->refcount  = 1;

    rec->surface     = surface;
    rec->format      = fmt;
    rec->palette     = pal;
    rec->owns_pixels = !prealloc;
    rec->next        = s_owned;
    s_owned          = rec;

    return surface;
}

// The library makes 32-bit surfaces; this adds the paletted ones Doom's
// screen buffer needs and leaves everything else to the library.
SDL_Surface *__wrap_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask)
{
    if (depth == 8)
        return NewOwnedSurface(width, height, 8, nullptr, 0);

    return __real_SDL_CreateRGBSurface(flags, width, height, depth,
                                       Rmask, Gmask, Bmask, Amask);
}

void __wrap_SDL_FreeSurface(SDL_Surface *surface)
{
    if (surface == nullptr)
        return;

    OwnedSurface **link = &s_owned;
    for (OwnedSurface *o = s_owned; o != nullptr; link = &o->next, o = o->next)
    {
        if (o->surface != surface)
            continue;

        if (--surface->refcount > 0)
            return;

        *link = o->next;
        if (o->palette != nullptr)
        {
            free(o->palette->colors);
            free(o->palette);
        }
        if (o->owns_pixels)
            free(surface->pixels);
        free(o->format);
        free(o);
        free(surface);
        return;
    }

    __real_SDL_FreeSurface(surface);
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
                                      int depth, int pitch, Uint32 Rmask,
                                      Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;
    return NewOwnedSurface(width, height, depth, pixels,
                           pitch != 0 ? pitch : width * (depth / 8));
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width,
                                                int height, int depth,
                                                int pitch, Uint32 format)
{
    if (depth == 0)
        depth = SDL_BITSPERPIXEL(format);
    if (depth != 8 && depth != 32)
    {
        SDL_SetError("surface format 0x%08x is not implemented", format);
        return nullptr;
    }
    // A null pixel pointer with a zero pitch still means preallocated here:
    // the caller is about to point the surface at memory it locks elsewhere.
    SDL_Surface *surface = NewOwnedSurface(width, height, depth,
                                           pixels, width * (depth / 8));
    if (surface != nullptr && pitch != 0)
        surface->pitch = pitch;
    if (surface != nullptr && pixels == nullptr)
        surface->pixels = nullptr;
    return surface;
}

int SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors,
                         int firstcolor, int ncolors)
{
    if (palette == nullptr || colors == nullptr)
    {
        SDL_SetError("SDL_SetPaletteColors: no palette");
        return -1;
    }
    if (firstcolor < 0 || ncolors < 0 || firstcolor + ncolors > palette->ncolors)
    {
        SDL_SetError("SDL_SetPaletteColors: range outside the palette");
        return -1;
    }

    for (int i = 0; i < ncolors; i++)
    {
        SDL_Color c = colors[i];
        // Doom fills only r, g and b. A zero alpha here would convert the
        // whole picture to transparent.
        c.a = 0xFF;
        palette->colors[firstcolor + i] = c;
    }
    palette->version++;
    return 0;
}

// Nothing here is RLE encoded or hardware backed, so a lock is a formality.
int SDL_LockSurface(SDL_Surface *) { return 0; }
void SDL_UnlockSurface(SDL_Surface *) {}

static SDL_Rect WholeSurface(SDL_Surface *s)
{
    return SDL_Rect{ 0, 0, s->w, s->h };
}

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
    if (dst == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_FillRect: no destination");
        return -1;
    }

    SDL_Rect r = (rect != nullptr) ? *rect : WholeSurface(dst);
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > dst->w) r.w = dst->w - r.x;
    if (r.y + r.h > dst->h) r.h = dst->h - r.y;
    if (r.w <= 0 || r.h <= 0)
        return 0;

    const int bpp = dst->format->BytesPerPixel;
    for (int y = 0; y < r.h; y++)
    {
        Uint8 *row = (Uint8 *)dst->pixels + (size_t)(r.y + y) * dst->pitch
                     + (size_t)r.x * bpp;
        if (bpp == 1)
        {
            memset(row, (int)(color & 0xFF), (size_t)r.w);
        }
        else
        {
            Uint32 *p = (Uint32 *)row;
            for (int x = 0; x < r.w; x++)
                p[x] = color;
        }
    }
    return 0;
}

// The frame's one conversion: Doom's 8-bit screen buffer through its palette
// into 32-bit ARGB, straight into the memory a locked streaming texture
// handed back. Same-format copies are a row memcpy.
int SDL_LowerBlit(SDL_Surface *src, SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src == nullptr || dst == nullptr
        || src->pixels == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_LowerBlit: no source or no destination");
        return -1;
    }

    SDL_Rect sr = (srcrect != nullptr) ? *srcrect : WholeSurface(src);
    SDL_Rect dr = (dstrect != nullptr) ? *dstrect : WholeSurface(dst);

    // The rectangles are taken as given, and only the SOURCE is checked
    // against the memory behind it. This is the LOWER blit, and SDL's
    // contract for it is that the caller has already clipped.
    //
    // Clipping against the destination surface's own w and h here would be
    // actively wrong for the one caller that matters. Chocolate Doom points
    // its 32-bit staging surface at whatever memory a locked texture hands
    // back, frame by frame, and leaves the surface's declared size as the
    // window size it was created with. Those two numbers have nothing to do
    // with each other, so treating the declared size as the limit would trim
    // the picture — or drop it entirely — for reasons invisible from here.
    int w = sr.w < dr.w ? sr.w : dr.w;
    int h = sr.h < dr.h ? sr.h : dr.h;
    if (w > src->w - sr.x) w = src->w - sr.x;
    if (h > src->h - sr.y) h = src->h - sr.y;
    if (w <= 0 || h <= 0)
        return 0;

    const int sbpp = src->format->BytesPerPixel;
    const int dbpp = dst->format->BytesPerPixel;

    if (sbpp == 1 && dbpp == 4)
    {
        const SDL_Palette *pal = src->format->palette;
        if (pal == nullptr)
        {
            SDL_SetError("SDL_LowerBlit: paletted source has no palette");
            return -1;
        }

        // One flat lookup table per blit, built from the palette as it
        // stands. Doom changes the palette on damage, item pickups and the
        // menu, so it is rebuilt every frame rather than cached.
        Uint32 lut[256];
        const int n = pal->ncolors < 256 ? pal->ncolors : 256;
        for (int i = 0; i < n; i++)
        {
            const SDL_Color &c = pal->colors[i];
            lut[i] = 0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8)
                     | (Uint32)c.b;
        }
        for (int i = n; i < 256; i++)
            lut[i] = 0xFF000000u;

        for (int y = 0; y < h; y++)
        {
            const Uint8 *s = (const Uint8 *)src->pixels
                             + (size_t)(sr.y + y) * src->pitch + sr.x;
            Uint32 *d = (Uint32 *)((Uint8 *)dst->pixels
                        + (size_t)(dr.y + y) * dst->pitch) + dr.x;
            for (int x = 0; x < w; x++)
                d[x] = lut[s[x]];
        }
        return 0;
    }

    if (sbpp == dbpp)
    {
        for (int y = 0; y < h; y++)
        {
            const Uint8 *s = (const Uint8 *)src->pixels
                             + (size_t)(sr.y + y) * src->pitch
                             + (size_t)sr.x * sbpp;
            Uint8 *d = (Uint8 *)dst->pixels + (size_t)(dr.y + y) * dst->pitch
                       + (size_t)dr.x * dbpp;
            memcpy(d, s, (size_t)w * sbpp);
        }
        return 0;
    }

    SDL_SetError("SDL_LowerBlit: %d-bit to %d-bit is not implemented",
                 sbpp * 8, dbpp * 8);
    return -1;
}

// ---------------------------------------------------------------------------
// Window and renderer calls with nothing to do on this board
// ---------------------------------------------------------------------------
//
// The display is one fullscreen panel that the host kernel declared before
// the game started. Its size, its position and its scaling are settled
// before any of these can be called, so each answers success and changes
// nothing — which is the truth, not a pretence.

int SDL_SetWindowFullscreen(SDL_Window *, Uint32) { return 0; }
void SDL_SetWindowSize(SDL_Window *, int, int) {}
void SDL_SetWindowMinimumSize(SDL_Window *, int, int) {}
void SDL_SetWindowIcon(SDL_Window *, SDL_Surface *) {}
int SDL_RenderSetLogicalSize(SDL_Renderer *, int, int) { return 0; }
int SDL_RenderSetIntegerScale(SDL_Renderer *, SDL_bool) { return 0; }

// Render-to-texture: the shim's renderer draws to the screen and nowhere
// else. Restoring the default target succeeds because that is where it
// already is; asking for any other target fails, which is what stops a
// caller believing the picture went somewhere it did not.
//
// Chocolate Doom asks for one when `smooth_pixel_scaling` is on. The card's
// chocolate-doom.cfg turns it off — see the comment in that file — because
// with it on the game renders into a texture that was never created and the
// screen stays black.
int SDL_SetRenderTarget(SDL_Renderer *, SDL_Texture *texture)
{
    if (texture == nullptr)
        return 0;
    SDL_SetError("render targets are not implemented");
    return -1;
}

// Used by the text-mode screens, which need a static picture uploaded once.
SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer,
                                          SDL_Surface *surface)
{
    if (renderer == nullptr || surface == nullptr || surface->pixels == nullptr)
    {
        SDL_SetError("SDL_CreateTextureFromSurface: no surface");
        return nullptr;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             surface->w, surface->h);
    if (texture == nullptr)
        return nullptr;

    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0)
    {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    SDL_Surface staging{};
    SDL_PixelFormat fmt{};
    fmt.format        = SDL_PIXELFORMAT_ARGB8888;
    fmt.BitsPerPixel  = 32;
    fmt.BytesPerPixel = 4;
    staging.format    = &fmt;
    staging.w         = surface->w;
    staging.h         = surface->h;
    staging.pitch     = pitch;
    staging.pixels    = pixels;

    const int rc = SDL_LowerBlit(surface, nullptr, &staging, nullptr);
    SDL_UnlockTexture(texture);

    if (rc != 0)
    {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// Doom peeks one event ahead to pair a key press with the text it produced.
// The shim has no peek, so this answers "nothing queued", which is the same
// answer a keyboard producing no text input would give.
int SDL_PeepEvents(SDL_Event *, int, SDL_eventaction, Uint32, Uint32)
{
    return 0;
}

// The text-mode screens block here. Polling and yielding keeps the servo on
// core 0 running, which is what delivers the event this is waiting for.
int SDL_WaitEvent(SDL_Event *event)
{
    for (;;)
    {
        if (SDL_PollEvent(event))
            return 1;
        SDL_Delay(5);
    }
}

// ---------------------------------------------------------------------------
// Mutexes and condition variables
// ---------------------------------------------------------------------------
//
// The OPL emulation guards its callback queue with these, and reaches them
// whether or not it ever produces a sound. There are no threads on this
// board and nothing contends for them, but a lock that quietly did nothing
// would be the wrong thing to leave behind for the day something does, so
// the mutexes are real: a test-and-set over one word, which costs almost
// nothing.

struct SDL_mutex { volatile int held; };
struct SDL_cond  { volatile unsigned signalled; };

SDL_mutex *SDL_CreateMutex(void)
{
    SDL_mutex *m = (SDL_mutex *)calloc(1, sizeof(SDL_mutex));
    if (m == nullptr)
        SDL_SetError("out of memory allocating mutex");
    return m;
}

void SDL_DestroyMutex(SDL_mutex *mutex) { free(mutex); }

int SDL_LockMutex(SDL_mutex *mutex)
{
    if (mutex == nullptr)
        return -1;
    while (__atomic_exchange_n(&mutex->held, 1, __ATOMIC_ACQUIRE) != 0)
        asm volatile("yield" ::: "memory");
    return 0;
}

int SDL_UnlockMutex(SDL_mutex *mutex)
{
    if (mutex == nullptr)
        return -1;
    __atomic_store_n(&mutex->held, 0, __ATOMIC_RELEASE);
    return 0;
}

SDL_cond *SDL_CreateCond(void)
{
    SDL_cond *c = (SDL_cond *)calloc(1, sizeof(SDL_cond));
    if (c == nullptr)
        SDL_SetError("out of memory allocating condition variable");
    return c;
}

void SDL_DestroyCond(SDL_cond *cond) { free(cond); }

int SDL_CondSignal(SDL_cond *cond)
{
    if (cond == nullptr)
        return -1;
    __atomic_fetch_add(&cond->signalled, 1, __ATOMIC_RELEASE);
    return 0;
}

// A wait with nothing that can signal it is a hang, and a hang on a board
// with no console is indistinguishable from a crash. The only signaller
// would be a thread, and this port creates none, so an unsignalled wait
// returns an error instead of never returning at all.
int SDL_CondWait(SDL_cond *cond, SDL_mutex *mutex)
{
    if (cond == nullptr || mutex == nullptr)
        return -1;
    if (__atomic_load_n(&cond->signalled, __ATOMIC_ACQUIRE) > 0)
    {
        __atomic_fetch_sub(&cond->signalled, 1, __ATOMIC_ACQUIRE);
        return 0;
    }
    SDL_SetError("SDL_CondWait: nothing can signal this condition variable");
    return -1;
}

// ---------------------------------------------------------------------------
// Odds and ends
// ---------------------------------------------------------------------------

// Where the game keeps its configuration files and its save games. On a
// desktop this is a per-user directory; here it is the card directory the
// game was started from, which is also where its WAD lives. The caller frees
// what it gets, so this hands back a fresh copy every time.
char *SDL_GetPrefPath(const char *, const char *)
{
    static const char path[] = RAPI_GAME_DIR "/";
    char *copy = (char *)SDL_malloc(sizeof(path));
    if (copy != nullptr)
        memcpy(copy, path, sizeof(path));
    return copy;
}

// The board has no window manager to put a dialog on top of, so the message
// goes where every other diagnostic goes: the serial console.
int SDL_ShowSimpleMessageBox(Uint32, const char *title, const char *message,
                             SDL_Window *)
{
    printf("%s: %s\n", title != nullptr ? title : "message",
           message != nullptr ? message : "");
    return 0;
}

// Names for the keys the text-mode screens print. SDL builds these from a
// table this port does not carry; the printable keys name themselves and
// everything else is reported honestly as unnamed.
const char *SDL_GetKeyName(SDL_Keycode key)
{
    static char name[2];
    if (key >= ' ' && key < 0x7F)
    {
        name[0] = (char)key;
        name[1] = '\0';
        return name;
    }
    return "";
}

} // extern "C"
