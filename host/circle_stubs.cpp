//
// circle_stubs.cpp — the 8-bit paletted surface support Chocolate Doom
// needs that circle-libsdl2 does not carry.
//
// circle-libsdl2's SDL_CreateRGBSurface makes 32-bit surfaces only.
// Chocolate Doom's screen buffer is 8-bit paletted, the way Doom has always
// drawn, so this file adds the paletted case through the linker's --wrap
// (see the WRAPPED_SDL list in the Makefile): __wrap_SDL_CreateRGBSurface
// and __wrap_SDL_FreeSurface hand 32-bit requests straight to the library's
// own __real_ versions and only take on the 8-bit case themselves.
// Redefining SDL_CreateRGBSurface and SDL_FreeSurface outright would be a
// duplicate symbol at best and a silent shadow at worst.
//
// A surface made here carries allocations the library knows nothing about —
// a palette, a heap pixel format, sometimes the pixels — so freeing it is
// this file's job too; see "Surfaces this file owns" below for how a freed
// surface is told apart from the library's own.
//
// When the shim library gains a function this port used to carry itself,
// the way to adopt it is to DELETE the copy here: the archive is linked
// whole, so a leftover copy becomes a duplicate-symbol error at link time
// rather than a silent winner over the real thing.
//
#include <cstdlib>

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

} // extern "C"
