// Pokemon palettes (FVX "Pokemon Palettes"): each Pokemon's battle sprite colors are hue-shifted at runtime.
// Follow Types: the main colors turn toward the color of its (new) first type, the others toward its second type.
// Follow Evolutions: a family shares its shift. Shiny From Normal: the shiny colors are the original colors.
#include "global.h"
#include "constants/rgb.h"
#include "pokemon.h"
#include "rh_internal.h"

#define PAL_BUFFERS 8
static EWRAM_DATA u16 sPalBuf[PAL_BUFFERS][16] = {0};
static EWRAM_DATA u8 sPalNext = 0;

// Hue (0..359) of each type's color.
static const u16 sTypeHue[NUMBER_OF_MON_TYPES] = {
    [TYPE_NORMAL] = 40, [TYPE_FIGHTING] = 15, [TYPE_FLYING] = 225, [TYPE_POISON] = 285, [TYPE_GROUND] = 35,
    [TYPE_ROCK] = 45, [TYPE_BUG] = 75, [TYPE_GHOST] = 265, [TYPE_STEEL] = 200, [TYPE_FIRE] = 10, [TYPE_WATER] = 215,
    [TYPE_GRASS] = 110, [TYPE_ELECTRIC] = 52, [TYPE_PSYCHIC] = 330, [TYPE_ICE] = 185, [TYPE_DRAGON] = 250,
    [TYPE_DARK] = 20, [TYPE_FAIRY] = 320,
};

struct Hsv { s16 h; u8 s, v; };   // h 0..359, s/v 0..255

static struct Hsv ToHsv(u16 c)
{
    s32 r = (c & 31) * 255 / 31, g = ((c >> 5) & 31) * 255 / 31, b = ((c >> 10) & 31) * 255 / 31;
    s32 mx = max(r, max(g, b)), mn = min(r, min(g, b)), d = mx - mn, h = 0;
    struct Hsv o;
    if (d != 0)
    {
        if (mx == r)
            h = 60 * (g - b) / d;
        else if (mx == g)
            h = 120 + 60 * (b - r) / d;
        else
            h = 240 + 60 * (r - g) / d;
    }
    if (h < 0)
        h += 360;
    o.h = h;
    o.s = mx ? d * 255 / mx : 0;
    o.v = mx;
    return o;
}

static u16 FromHsv(struct Hsv x)
{
    s32 h = ((x.h % 360) + 360) % 360, s = x.s, v = x.v;
    s32 c = v * s / 255, hh = h / 60, f = h % 60;
    s32 xx = c * (60 - abs((hh & 1) * 60 + f - 60)) / 60;   // chroma * (1 - |(h/60 mod 2) - 1|)
    s32 r = 0, g = 0, b = 0, m = v - c;
    switch (hh)
    {
    case 0: r = c; g = xx; break;
    case 1: r = xx; g = c; break;
    case 2: g = c; b = xx; break;
    case 3: g = xx; b = c; break;
    case 4: r = xx; b = c; break;
    default: r = c; b = xx; break;
    }
    r = (r + m) * 31 / 255;
    g = (g + m) * 31 / 255;
    b = (b + m) * 31 / 255;
    return RGB(r, g, b);
}

static s32 HueDist(s32 a, s32 b)
{
    s32 d = abs(a - b) % 360;
    return d > 180 ? 360 - d : d;
}

const u16 *RH_MonPalette(u16 species, bool32 isShiny, const u16 *normal, const u16 *vanilla)
{
    u16 *out;
    u32 i, key, h;
    s32 domHue = -1, bestWeight = -1, shift1, shift2;
    struct Hsv col[16];
    const u16 *src;
    if (S->paletteMode == 0 || species == SPECIES_NONE || species >= NUM_SPECIES || vanilla == NULL)
        return vanilla;
    if (isShiny && S->paletteShinyFromNormal && normal != NULL)
        return normal;                                       // the shiny colors are the original ones
    src = vanilla;
    out = sPalBuf[sPalNext];
    sPalNext = (sPalNext + 1) % PAL_BUFFERS;
    key = S->paletteFollowEvos ? RH_TraitRoot(species) : species;
    h = RH_Hash(SALT_STATS + 0x9A1, key, isShiny);
    // the dominant hue: the most saturated / most common colored hue bucket
    for (i = 1; i < 16; i++)
        col[i] = ToHsv(src[i]);
    {
        u16 bucket[12] = {0};
        for (i = 1; i < 16; i++)
            if (col[i].s > 60 && col[i].v > 40)
                bucket[col[i].h / 30] += col[i].s;
        for (i = 0; i < 12; i++)
            if ((s32)bucket[i] > bestWeight && bucket[i] > 0)
                bestWeight = bucket[i], domHue = i * 30 + 15;
    }
    if (S->paletteFollowTypes)
    {
        u8 t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
        s32 jitter = (s32)(h % 31) - 15;
        shift1 = (domHue < 0) ? 0 : sTypeHue[t1] + jitter - domHue;
        shift2 = (domHue < 0) ? 0 : sTypeHue[t2] + jitter - domHue;
    }
    else
    {
        shift1 = shift2 = 30 + h % 300;                      // any other hue
    }
    out[0] = src[0];
    for (i = 1; i < 16; i++)
    {
        struct Hsv x = col[i];
        if (x.s > 40 && x.v > 30)                            // greys, black and white stay
        {
            bool32 secondary = domHue >= 0 && HueDist(x.h, domHue) > 60;
            x.h += secondary ? shift2 : shift1;
            if (!S->paletteFollowTypes)
                x.s = min(255, x.s * (80 + (h >> 12) % 50) / 100);
            out[i] = FromHsv(x);
        }
        else
        {
            out[i] = src[i];
        }
    }
    return out;
}
