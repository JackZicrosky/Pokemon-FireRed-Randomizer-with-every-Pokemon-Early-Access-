// UI Theme (Misc. Tweaks): Default / Randomizer / AMOLED.
// The game's menus are drawn for a white background. Instead of inverting them, every palette that belongs to a
// menu, text box or window frame is re-lit when it is loaded:
//  - text palettes: white backgrounds turn dark, dark/grey text turns light, light text shadows turn dark; colored
//    text (red, yellow, blue...) keeps its own color, it reads fine on a dark background;
//  - screen art (bag, party, summary, Pokedex...): the same picture, dark;
//  - window frames keep their shading (AMOLED: the vanilla frames as they are);
//  - "dark" palettes (around the battle text box): everything dark.
// Randomizer draws everything that isn't colored text with the randomizer screen's own blue-grey colors (sPal[] in
// rh_randomizer_menu.c); AMOLED goes pitch black with grey accents.
#include "global.h"
#include "graphics.h"
#include "constants/rgb.h"
#include "menu.h"
#include "palette.h"
#include "text_window.h"
#include "sprite.h"
#include "malloc.h"
#include "bg.h"
#include "decompress.h"
#include "rh.h"
#include "rh_internal.h"

#define MODE_TEXT   RH_THEME_MODE_TEXT
#define MODE_SCREEN RH_THEME_MODE_SCREEN
#define MODE_FRAME  RH_THEME_MODE_FRAME
#define MODE_DARK   RH_THEME_MODE_DARK

// Lightness curves (old lightness -> new lightness, 0..256) through: black, mid grey (16,16,16; PP numbers and other
// grey text stay bright), light grey (22,22,22), standard text shadow (26,26,25), white.
// Screen greys: like text, but mid greys (outlines, shading, dark backgrounds) stay dark.
#define CURVE_POINTS 5
static const s16 sCurve[2][RH_THEME_COUNT - 1][CURVE_POINTS][2] =
{
    [MODE_TEXT] = {
        [RH_THEME_RANDOMIZER - 1] = { {0, 248}, {140, 240}, {180, 110}, {215, 70}, {256, 33} },   // white -> panel (3,4,5)
        [RH_THEME_AMOLED - 1]     = { {0, 256}, {140, 244}, {180, 100}, {215, 40}, {256, 0} },    // white -> black
    },
    [MODE_SCREEN] = {
        [RH_THEME_RANDOMIZER - 1] = { {0, 248}, {99, 232}, {120, 77}, {215, 62}, {256, 33} },
        [RH_THEME_AMOLED - 1]     = { {0, 256}, {99, 240}, {120, 60}, {215, 40}, {256, 0} },
    },
};

static s32 Curve(const s16 (*pts)[2], s32 x)
{
    u32 i;
    for (i = 1; i < CURVE_POINTS - 1 && x > pts[i][0]; i++)
        ;
    return pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * (x - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]);
}

// The randomizer screen's colors from dark to light (background, panel, header bar, shadow, selected row, dim text,
// a grey between dim and text, text), with their lightness ((max + min) / 2 in 1/8 steps).
static const struct { u8 l, r, g, b; } sBrandRamp[] =
{
    { 24, 2, 3, 4 }, { 32, 3, 4, 5 }, { 56, 5, 6, 9 }, { 68, 7, 8, 10 }, { 80, 7, 9, 13 }, { 116, 13, 14, 16 },
    { 172, 20, 21, 23 }, { 228, 28, 28, 29 },
};

// Randomizer theme: the brand color of a lightness (0..248).
static u16 BrandColor(s32 l)
{
    u32 i;
    s32 r, g, b, t, span;
    if (l <= sBrandRamp[0].l)
        return RGB(sBrandRamp[0].r, sBrandRamp[0].g, sBrandRamp[0].b);
    for (i = 1; i < ARRAY_COUNT(sBrandRamp) - 1 && l > sBrandRamp[i].l; i++)
        ;
    if (l >= sBrandRamp[i].l)
        return RGB(sBrandRamp[i].r, sBrandRamp[i].g, sBrandRamp[i].b);
    span = sBrandRamp[i].l - sBrandRamp[i - 1].l;
    t = l - sBrandRamp[i - 1].l;
    r = (sBrandRamp[i - 1].r * (span - t) + sBrandRamp[i].r * t + span / 2) / span;
    g = (sBrandRamp[i - 1].g * (span - t) + sBrandRamp[i].g * t + span / 2) / span;
    b = (sBrandRamp[i - 1].b * (span - t) + sBrandRamp[i].b * t + span / 2) / span;
    return RGB(r, g, b);
}

static u16 ThemeColor(u16 color, u32 theme, u32 mode)
{
    s32 c[3], mx, mn, l, nl, chroma, newChroma, cmax, i, v;
    u16 out = 0;

    if (theme == RH_THEME_DEFAULT || theme >= RH_THEME_COUNT)
        return color;
    // Work in 1/8 steps (0..248) so rounding doesn't drift the hue.
    c[0] = (color & 31) * 8;
    c[1] = ((color >> 5) & 31) * 8;
    c[2] = ((color >> 10) & 31) * 8;
    mx = max(c[0], max(c[1], c[2]));
    mn = min(c[0], min(c[1], c[2]));
    chroma = mx - mn;
    l = (mx + mn) / 2;

    // Colored text (red, yellow, blue, green...) already reads well on a dark background: keep it. Its pale shadows
    // (l > 184) still turn dark.
    if (mode == MODE_TEXT && chroma >= 12 * 8 && l >= 48 && l <= 184)
        return color;
    // AMOLED: window frames stay as they are (they read well on black), except their white inner edge: black.
    if (mode == MODE_FRAME && theme == RH_THEME_AMOLED)
        return (chroma < 3 * 8 && l >= 200) ? RGB_BLACK : color;

    switch (mode)
    {
    case MODE_FRAME:    // keeps the frame's shading, from the header bar to the dim grey
        return BrandColor(56 + l * (116 - 56) / 248);
    case MODE_DARK:     // everything dark (white = the panel color)
        nl = (theme == RH_THEME_AMOLED) ? l * 6 / 248 : 20 + l * 14 / 248;
        break;
    case MODE_SCREEN:
        if (chroma >= 8 * 8)
        {
            // Colored art: the same picture, much darker (AMOLED: nearly black, with a hint of its color).
            nl = (theme == RH_THEME_AMOLED) ? 1 + l * 22 / 248 : 20 + l * 72 / 248;
            break;
        }
        // fallthrough
    default:
        nl = Curve(sCurve[mode == MODE_SCREEN ? MODE_SCREEN : MODE_TEXT][theme - 1], l * 256 / 248) * 248 / 256;
        break;
    }
    if (theme == RH_THEME_RANDOMIZER)
        return BrandColor(nl);

    // AMOLED: keep the saturation (chroma relative to the most a color of that lightness can have).
    cmax = 248 - abs(2 * l - 248);
    newChroma = (cmax != 0) ? chroma * (248 - abs(2 * nl - 248)) / cmax : 0;
    if (newChroma > chroma)
        newChroma = chroma;   // never more colorful than before (pale tints stay subtle when they turn dark)
    if ((mode == MODE_SCREEN || mode == MODE_DARK) && l >= 168)
        newChroma = newChroma * (l >= 200 ? 2 : 3) / 5;   // pale "paper" surfaces become near-neutral dark panels
    if (chroma < 3 * 8)
        newChroma = 0;   // near-greys become pure greys
    else if (mode != MODE_TEXT)
        newChroma = newChroma / 2;

    for (i = 0; i < 3; i++)
    {
        v = (chroma != 0) ? nl + (c[i] - l) * newChroma / chroma : nl;
        v = (v + 4) / 8;
        if (v < 0)
            v = 0;
        if (v > 31)
            v = 31;
        out |= v << (i * 5);
    }
    return out;
}

// Randomizer theme: a highlight color (orange selector, red arrows, red selected text) becomes the randomizer screen's
// purple selection color (20,13,31), as light or dark as the original. Other themes and grey colors: unchanged.
static u16 AccentColor(u16 color, u32 theme)
{
    s32 r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
    s32 mx = max(r, max(g, b)), mn = min(r, min(g, b)), l = (mx + mn) * 4;   // 0..248
    if (theme != RH_THEME_RANDOMIZER || mx - mn < 12)
        return color;
    if (l >= 176)
        return RGB(20, 13, 31);
    return RGB(20 * l / 176, 13 * l / 176, 31 * l / 176);
}

u16 RH_ThemeColor(u16 color, u32 theme)
{
    return ThemeColor(color, theme, MODE_TEXT);
}

void RH_ThemePalette(u16 *pal, u32 count, u32 theme)
{
    u32 i;
    for (i = 0; i < count; i++)
        pal[i] = ThemeColor(pal[i], theme, MODE_TEXT);
}

struct ThemedPal
{
    const u16 *pal;
    u16 count;
    u8 mode;
    u16 keep;   // color indices (in every row of 16) left unchanged, e.g. white text drawn on colored boxes
    u16 white;  // color indices (in every row of 16) set to white (spare colors used for text by themed code)
    u16 merge;  // color indices (in every row of 16) that all get their average: turns striped backgrounds plain
    u16 frame;  // color indices (in every row of 16) that draw a window frame (MODE_FRAME)
    u16 objKeep; // extra colors left unchanged when the palette is loaded for sprites
    u16 accent; // highlight colors that turn purple in the Randomizer theme (else as "keep")
};

extern const u16 sRedInterface_Pal[];   // list_menu.c: scroll arrows and cursors (bag, shop, lists)

#define DEX_PANEL_KEEP ((1 << 8) | (1 << 9))
#define DEX_LABEL_KEEP ((1 << 1) | (1 << 15))

// Menu, text box and window palettes (anything drawn on a white base). Loading one of these anywhere gets themed.
static const struct ThemedPal sThemed[] =
{
    { gStandardMenuPalette, 16, MODE_TEXT },
    { gMessageBox_Pal, 16, MODE_TEXT, .frame = 0xFC00 },   // (10-15: the message box's frame)
    { gBagScreenMale_Pal, 32, MODE_SCREEN, .merge = (1 << 12) | (1 << 13) },     // (12-13: the two background stripes)
    { gBagScreenFemale_Pal, 32, MODE_SCREEN, .merge = (1 << 12) | (1 << 13) },
    { gPartyMenuBg_Pal, 16, MODE_SCREEN },
    // (orange selector outlines: purple in the Randomizer theme)
    { gPartyMenuBg_Pal + 16, 2 * 16, MODE_SCREEN, (1 << 1) | (1 << 10), .merge = (1 << 4) | (1 << 5), .accent = 1 << 1 },   // background stripes, Cancel/Confirm
    { gPartyMenuBg_Pal + 48, 8 * 16, MODE_SCREEN, (1 << 1) | (1 << 2) | (1 << 3) | (0xF << 9) | (7 << 13), .accent = (1 << 1) | (3 << 7) },   // Pokemon boxes (text, HP bar, gender)
    { sRedInterface_Pal, 16, MODE_SCREEN, 0xFFFF, .accent = 0xFFFE },   // red arrows: purple in the Randomizer theme
    { gSummaryScreen_Pal, 6 * 16, MODE_SCREEN, 1 << 2 },
    { gSummaryScreen_Pal + 96, 2 * 16, MODE_TEXT, (1 << 3) | (1 << 4) },   // (3-4: white labels with a grey shadow)
    { gPPTextPalette, 16, MODE_TEXT },
    { gShopMenu_Pal, 16, MODE_SCREEN },
    // Battle message box and everything around it: dark. Its white text moves to spare color 8 (battle_message.c),
    // because color 1 is also the background of the action menu's cursor.
    { gBattleTextboxPalette, 16, MODE_DARK, (7 << 2) | (1 << 6), 1 << 8 },   // (2-4: the red "more text" arrow)
    { gBattleTextboxPalette + 16, 16, MODE_TEXT },
    // (8-9: the navy panel. The interface sprites (SEEN / OWN / MENU / SEARCH, START / SELECT) use a copy of the
    // first row: there the white letters and black outlines (1, 15) stay too, see objKeep)
    { gPokedexBgHoenn_Pal, 6 * 16, MODE_SCREEN, DEX_PANEL_KEEP, .objKeep = DEX_LABEL_KEEP },
    { gPokedexBgNational_Pal, 6 * 16, MODE_SCREEN, DEX_PANEL_KEEP, .objKeep = DEX_LABEL_KEEP },
    { gPokedexSearchResults_Pal, 6 * 16, MODE_SCREEN, DEX_PANEL_KEEP, .objKeep = DEX_LABEL_KEEP },
    { gPokedexSearchMenu_Pal, 4 * 16, MODE_SCREEN },
    { gBattleWindowTextPalette, 16, MODE_TEXT },
};

// The owner's exact colors for single palette entries (from their edited screenshots), per theme: { Randomizer, AMOLED }.
// NONE = that theme's normal result. Default is never changed. OV_BG / OV_OBJ: only where the palette is loaded for
// backgrounds / sprites. OV_LATE: the screen themes the palette itself (RH_ThemeLoadedRange), which then calls
// RH_ThemeOverrides.
#define NONE 0xFFFF
#define OV_BG   1
#define OV_OBJ  2
#define OV_LATE 4
struct ThemeOverride
{
    const u16 *pal;
    u8 index;
    u8 flags;
    u16 color[RH_THEME_COUNT - 1];
};

extern const u16 sTextWindowPalettes[][16];
extern const u16 sOptionMenuText_Pal[];
extern const u16 sMarkings_Pal[];
extern const u16 sKeyboard_Pal[];
extern const u16 sScrollingBg_Pal[];


#define BOTH(r, g, b) { RGB(r, g, b), RGB(r, g, b) }
#define DEX_OVERRIDES(pal)                                                                                     \
    { pal, 7, OV_BG, { NONE, RGB(12, 3, 2) } },      /* the ball's red top (RH_ThemePokedexTiles) */             \
    { pal, 9, OV_BG, { NONE, RGB(14, 14, 14) } },    /* and its grey half */                                     \
    { pal, 10, OV_OBJ, { NONE, RGB(5, 5, 5) } },     /* START / SELECT buttons (RH_ThemePokedexTiles) */         \
    { pal, 13, OV_OBJ, { NONE, RGB_WHITE } },        /* their text and the scroll bar */                         \
    { pal, 14, 0, { NONE, RGB_BLACK } }              /* the scroll bar's track and rim */

static const struct ThemeOverride sOverrides[] =
{
    // Summary: page dots (current: the theme's accent; later pages: white; earlier pages: dark, see
    // RH_ThemeSummaryTiles), OT name, female symbol, EXP label; AMOLED: dim markings.
    { gSummaryScreen_Pal, 68, 0, { RGB(13, 8, 21), RGB(16, 0, 0) } },
    { gSummaryScreen_Pal, 69, 0, { RGB(16, 10, 26), RGB(24, 0, 0) } },
    { gSummaryScreen_Pal, 74, 0, { RGB(1, 2, 2), RGB(2, 2, 2) } },
    { gSummaryScreen_Pal, 75, 0, { RGB(3, 4, 5), RGB(5, 5, 5) } },
    { gSummaryScreen_Pal, 76, 0, { RGB(23, 23, 24), RGB(23, 23, 23) } },
    { gSummaryScreen_Pal, 107, 0, BOTH(17, 30, 31) },
    { gSummaryScreen_Pal, 105, 0, BOTH(31, 19, 18) },
    { gSummaryScreen_Pal, 35, 0, BOTH(30, 30, 30) },   // EXP label (RH_ThemeSummaryTiles)
    { gSummaryScreen_Pal, 47, 0, { NONE, RGB_BLACK } },   // AMOLED: the backgrounds pure black
    { gSummaryScreen_Pal, 67, 0, { NONE, RGB_BLACK } },
    { gSummaryScreen_Pal, 72, 0, { NONE, RGB_BLACK } },
    { gSummaryScreen_Pal, 55, 0, { NONE, RGB_BLACK } },
    { gSummaryScreen_Pal, 56, 0, { NONE, RGB_BLACK } },
    { gSummaryScreen_Pal, 57, 0, { NONE, RGB_BLACK } },
    { gSummaryScreen_Pal, 58, 0, { NONE, RGB_BLACK } },
    { sMarkings_Pal, 1, 0, { NONE, RGB(6, 5, 9) } },
    { sMarkings_Pal, 2, 0, { NONE, RGB(6, 5, 9) } },
    // Window frame (type 1), AMOLED: its rounded corners black.
    { gTextWindowFrame1_Pal, 7, 0, { NONE, RGB_BLACK } },
    // Bag: the current pocket's dot; AMOLED: the pocket ball red and grey (RH_ThemeBagTiles).
    { gBagScreenMale_Pal, 30, 0, { RGB(14, 9, 21), RGB_WHITE } },
    { gBagScreenFemale_Pal, 30, 0, { RGB(14, 9, 21), RGB_WHITE } },
    { gBagScreenMale_Pal, 14, 0, { NONE, RGB(12, 3, 2) } },
    { gBagScreenFemale_Pal, 14, 0, { NONE, RGB(12, 3, 2) } },
    { gBagScreenMale_Pal, 15, 0, { NONE, RGB(14, 14, 14) } },
    { gBagScreenFemale_Pal, 15, 0, { NONE, RGB(14, 14, 14) } },
    // Pokedex (AMOLED): a red and grey Poke Ball behind SEEN / OWN, white START / SELECT and scroll bar.
    DEX_OVERRIDES(gPokedexBgHoenn_Pal),
    DEX_OVERRIDES(gPokedexBgNational_Pal),
    DEX_OVERRIDES(gPokedexSearchResults_Pal),
    // Shop list and PC box background (AMOLED): their stripes nearly black.
    { gShopMenu_Pal, 9, 0, { NONE, RGB(1, 1, 1) } },
    { gShopMenu_Pal, 10, 0, { NONE, RGB_BLACK } },
    { sScrollingBg_Pal, 9, OV_LATE, { NONE, RGB(1, 1, 1) } },
    // Options: the chosen values and their shadows.
    { sOptionMenuText_Pal, 5, OV_LATE, { RGB(13, 8, 20), RGB(24, 4, 2) } },
    { sOptionMenuText_Pal, 4, OV_LATE, { RGB(17, 11, 26), RGB(24, 13, 12) } },
    // Naming screen: title bar (white text), keyboard letters (drawn with spare colors 6-7 when themed, see
    // naming_screen.c), gender symbol, the text box's edge, buttons.
    { sTextWindowPalettes[2], 15, 0, { RGB(19, 12, 29), RGB(4, 4, 4) } },
    { sTextWindowPalettes[2], 1, 0, BOTH(31, 31, 31) },
    { sTextWindowPalettes[2], 2, 0, BOTH(8, 8, 8) },
    { sKeyboard_Pal, 6, OV_LATE, BOTH(31, 31, 31) },
    { sKeyboard_Pal, 7, OV_LATE, { RGB(6, 7, 9), RGB(8, 8, 8) } },
    { gNamingScreenMenu_Pal[0], 2, OV_LATE | OV_BG, { RGB(7, 8, 10), RGB(7, 7, 7) } },
    { gNamingScreenMenu_Pal[0], 12, OV_LATE | OV_BG, { NONE, RGB_BLACK } },   // AMOLED: backgrounds pure black
    { gNamingScreenMenu_Pal[0], 13, OV_LATE | OV_BG, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 29, OV_LATE | OV_BG, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 44, OV_LATE | OV_BG, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 45, OV_LATE | OV_BG, { NONE, RGB_BLACK } },
    { sKeyboard_Pal, 13, OV_LATE, { NONE, RGB_BLACK } },
    { sKeyboard_Pal, 14, OV_LATE, { NONE, RGB_BLACK } },
    { sKeyboard_Pal, 15, OV_LATE, { NONE, RGB_BLACK } },
    { sKeyboard_Pal, 4, OV_LATE, BOTH(28, 1, 1) },
    { sKeyboard_Pal, 5, OV_LATE, BOTH(31, 23, 14) },
    { gNamingScreenMenu_Pal[0], 15, OV_LATE | OV_BG, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 76, OV_OBJ, { RGB(13, 8, 20), RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 77, OV_OBJ, { RGB(19, 12, 29), RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 67, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 68, OV_OBJ, { NONE, RGB(7, 7, 7) } },
    { gNamingScreenMenu_Pal[0], 72, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 73, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 43, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 45, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 61, OV_OBJ, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 74, OV_OBJ, { NONE, RGB(4, 4, 4) } },
    { gNamingScreenMenu_Pal[0], 75, OV_OBJ, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 78, OV_OBJ, { NONE, RGB_BLACK } },
    { gNamingScreenMenu_Pal[0], 79, OV_OBJ, { NONE, RGB_BLACK } },
};

static void ApplyOverrides(const u16 *src, u32 offset, u32 count, bool32 late)
{
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme, i, at;
    const u16 *p;
    if (theme == RH_THEME_DEFAULT || theme >= RH_THEME_COUNT)
        return;
    for (i = 0; i < ARRAY_COUNT(sOverrides); i++)
    {
        const struct ThemeOverride *ov = &sOverrides[i];
        if (ov->color[theme - 1] == NONE || late != ((ov->flags & OV_LATE) != 0))
            continue;
        p = ov->pal + ov->index;
        if (p < src || p >= src + count)
            continue;
        at = offset + (p - src);
        if (at >= PLTT_BUFFER_SIZE || ((ov->flags & OV_BG) && at >= OBJ_PLTT_OFFSET) || ((ov->flags & OV_OBJ) && at < OBJ_PLTT_OFFSET))
            continue;
        gPlttBufferUnfaded[at] = gPlttBufferFaded[at] = ov->color[theme - 1];
    }
}

void RH_ThemeOverrides(const u16 *src, u32 offset, u32 count)
{
    ApplyOverrides(src, offset, count, TRUE);
}

// Recolors 4bpp tiles in VRAM (16-bit accesses only). recolor(x, y, color) gets the pixel's position in a block
// of blockW tiles across and returns its new color.
static void RecolorVramTiles(u16 *vram, u32 firstTile, u32 blockW, u32 blockH, const u8 *tileIds, u32 (*recolor)(u32, u32, u32, const u8 *), const u8 *block)
{
    u32 i, y, x, c;
    for (i = 0; i < blockW * blockH; i++)
    {
        u16 *tile = vram + (firstTile + tileIds[i]) * 16;
        for (y = 0; y < 8; y++)
        {
            for (x = 0; x < 8; x += 4)
            {
                u16 v = tile[y * 2 + x / 4], n = 0;
                u32 k;
                for (k = 0; k < 4; k++)
                {
                    c = (v >> (k * 4)) & 15;
                    c = recolor((i % blockW) * 8 + x + k, (i / blockW) * 8 + y, c, block);
                    n |= c << (k * 4);
                }
                tile[y * 2 + x / 4] = n;
            }
        }
    }
}

// Summary page dots: an earlier page's dot is one solid disc of color 2, the same color as the later dots' rims. Its
// rim and its middle (a 5x5 square) move to the spare colors 10 and 11 (sOverrides: dark).
static u32 RecolorEarlierDot(u32 x, u32 y, u32 c, const u8 *block)
{
    // the disc is 7x7 at (3, 4) of its 16x16 block of tiles (the header's underline below it: color 2 too)
    if (c != 2 || y < 4 || y > 10)
        return c;
    return (x >= 4 && x <= 8 && y >= 5 && y <= 9) ? 11 : 10;
}

// The EXP label (colors 7-8, also used elsewhere) -> spare color 3 (white).
static u32 RecolorExpLabel(u32 x, u32 y, u32 c, const u8 *block)
{
    return (c == 7 || c == 8) ? 3 : c;
}

// The later pages' dots: a rim of color 2 (also the header's purple underline) -> spare color 12 (white).
static u32 RecolorLaterDot(u32 x, u32 y, u32 c, const u8 *block)
{
    return (c == 2 && y >= 4 && y <= 10) ? 12 : c;
}

void RH_ThemeSummaryTiles(void)
{
    static const u8 sEarlierDot[] = { 70, 71, 86, 87 };
    static const u8 sLaterDot[] = { 67, 68, 83, 84 };
    static const u8 sLastDot[] = { 72, 73, 88, 89 };
    static const u8 sExpLabel[] = { 96, 97 };
    if (gSaveBlock3Ptr->rhSettings.uiTheme == RH_THEME_DEFAULT)
        return;
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(2), 0, 2, 2, sEarlierDot, RecolorEarlierDot, NULL);
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(2), 0, 2, 2, sLaterDot, RecolorLaterDot, NULL);
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(2), 0, 2, 2, sLastDot, RecolorLaterDot, NULL);
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(2), 0, 2, 1, sExpLabel, RecolorExpLabel, NULL);
}

// Bag (AMOLED): the pocket ball's two halves share color 14; the lower half moves to spare color 15 (grey).
static u32 RecolorBallLowerHalf(u32 x, u32 y, u32 c, const u8 *block)
{
    return c == 14 ? 15 : c;
}

void RH_ThemeBagTiles(void)
{
    static const u8 sLowerHalf[] = { 12, 13 };
    if (gSaveBlock3Ptr->rhSettings.uiTheme != RH_THEME_AMOLED)
        return;
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(3), 0, 2, 1, sLowerHalf, RecolorBallLowerHalf, NULL);
}

// Pokedex (AMOLED): the START / SELECT buttons' text shares color 15 with every outline: the text pixels (not
// touching the edge of the button) move to spare color 13 (white), their body (11, also the list's arrows) to spare
// color 10. Tiles 32-39 and 48-55: 32x16 sprites. The scroll bar (tile 3): its fill 15 -> 13 too.
static u32 RecolorScrollBar(u32 x, u32 y, u32 c, const u8 *block)
{
    return c == 15 ? 13 : c;
}

static u32 RecolorButtonText(u32 x, u32 y, u32 c, const u8 *block)
{
    s32 dx, dy;
    if (c == 11)
        return 10;            // the button's body
    if (c != 15 || x >= 22)   // (from x 22: the dark round button, kept)
        return c;
    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            s32 nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= 32 || ny >= 16 || block[ny * 32 + nx] == 0)
                return c;   // outline
        }
    }
    return 13;
}

// The Pokedex list's ball (BG1, palette row 0, color 8): its top and bottom halves are the same tiles flipped, so it
// gets new tiles (from tile 256 on): the ring red above the middle, then a gap, then grey; the round button grey.
#define DEX_BALL_COLS 7
#define DEX_BALL_ROWS 20
#define DEX_BALL_FIRST_NEW_TILE 256

void RH_ThemePokedexTiles(u32 interfaceTileStart, const u32 *menuGfx)
{
    static const u8 sButtons[2][8] = { { 32, 33, 34, 35, 36, 37, 38, 39 }, { 48, 49, 50, 51, 52, 53, 54, 55 } };
    static const u8 sScrollBar[] = { 3 };
    u16 *tilemap, *objVram, *bgVram, *menuTiles;
    u8 *pix, *block;
    u32 i, x, y, k, newTiles = 0;
    if (gSaveBlock3Ptr->rhSettings.uiTheme != RH_THEME_AMOLED)
        return;

    // START / SELECT text
    objVram = (u16 *)(OBJ_VRAM0) + interfaceTileStart * 16;
    block = Alloc(32 * 16);
    if (block == NULL)
        return;
    for (i = 0; i < 2; i++)
    {
        for (k = 0; k < 8; k++)   // the button's pixels, to find its edge
        {
            u16 *tile = objVram + sButtons[i][k] * 16;
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x++)
                    block[((k / 4) * 8 + y) * 32 + (k % 4) * 8 + x] = (tile[y * 2 + x / 4] >> ((x % 4) * 4)) & 15;
        }
        RecolorVramTiles(objVram, 0, 4, 2, sButtons[i], RecolorButtonText, block);
    }
    Free(block);
    RecolorVramTiles(objVram, 0, 1, 1, sScrollBar, RecolorScrollBar, NULL);

    // the ball
    tilemap = GetBgTilemapBuffer(1);
    bgVram = (u16 *)BG_CHAR_ADDR(0);
    pix = Alloc(DEX_BALL_COLS * 8 * DEX_BALL_ROWS * 8);
    menuTiles = Alloc(GetDecompressedDataSize(menuGfx));   // (the copy to VRAM may not be done yet)
    if (tilemap == NULL || pix == NULL || menuTiles == NULL)
    {
        Free(pix);
        Free(menuTiles);
        return;
    }
    DecompressDataWithHeaderWram(menuGfx, menuTiles);
    for (y = 0; y < DEX_BALL_ROWS * 8; y++)
    {
        for (x = 0; x < DEX_BALL_COLS * 8; x++)
        {
            u16 e = tilemap[(y / 8) * 32 + x / 8];
            u32 tx = (e & 0x400) ? 7 - x % 8 : x % 8, ty = (e & 0x800) ? 7 - y % 8 : y % 8;
            u16 *tile = menuTiles + (e & 0x3FF) * 16;
            pix[y * DEX_BALL_COLS * 8 + x] = ((e >> 12) == 0) ? (tile[ty * 2 + tx / 4] >> ((tx % 4) * 4)) & 15 : 0;
        }
    }
    for (y = 0; y < DEX_BALL_ROWS * 8; y++)
    {
        u8 *row = pix + y * DEX_BALL_COLS * 8;
        u32 runs = 0, firstRunEnd = 0;
        for (x = 0; x < DEX_BALL_COLS * 8; x++)   // a row with two runs of color 8: the first is the round button
        {
            if (row[x] == 8 && (x == 0 || row[x - 1] != 8))
                runs++;
            if (runs == 1 && row[x] == 8)
                firstRunEnd = x;
        }
        for (x = 0; x < DEX_BALL_COLS * 8; x++)
        {
            if (row[x] != 8)
                continue;
            if (runs >= 2 && x <= firstRunEnd)
                row[x] = 9;                 // round button: grey
            else if (y < 78)
                row[x] = 7;                 // ring, top: red
            else if (y <= 83)
                row[x] = 0;                 // the gap
            else
                row[x] = 9;                 // ring, bottom: grey
        }
    }
    for (y = 0; y < DEX_BALL_ROWS; y++)
    {
        for (x = 0; x < DEX_BALL_COLS; x++)
        {
            u16 *e = &tilemap[y * 32 + x], tile[16];
            bool32 hasBall = FALSE;
            u32 ty, tx;
            if ((*e >> 12) != 0)
                continue;
            for (ty = 0; ty < 8; ty++)
            {
                for (tx = 0; tx < 8; tx++)
                {
                    u32 c = pix[(y * 8 + ty) * DEX_BALL_COLS * 8 + x * 8 + tx];
                    if (c == 7 || c == 9 || (c == 0 && y * 8 + ty >= 78 && y * 8 + ty <= 83))
                        hasBall = TRUE;
                    if (tx % 4 == 0)
                        tile[ty * 2 + tx / 4] = 0;
                    tile[ty * 2 + tx / 4] |= c << ((tx % 4) * 4);
                }
            }
            if (!hasBall)
                continue;
            CpuCopy16(tile, bgVram + (DEX_BALL_FIRST_NEW_TILE + newTiles) * 16, sizeof(tile));
            *e = DEX_BALL_FIRST_NEW_TILE + newTiles++;
        }
    }
    Free(pix);
    Free(menuTiles);
}

// Returns the mode for a themed palette, or -1.
// Returns the mode for a color of a themed palette (or -1), its keep mask and its index in its row of 16.
static s32 ThemedMode(const u16 *src, u32 offset, u16 *keep, u16 *white, u16 *merge, u16 *frame, u16 *accent, u32 *index)
{
    u32 i;
    *keep = *white = *merge = *frame = *accent = 0;
    for (i = 0; i < ARRAY_COUNT(sThemed); i++)
    {
        if (src >= sThemed[i].pal && src < sThemed[i].pal + sThemed[i].count)
        {
            *keep = sThemed[i].keep | (offset >= OBJ_PLTT_OFFSET ? sThemed[i].objKeep : 0);
            *white = sThemed[i].white;
            *merge = sThemed[i].merge;
            *frame = sThemed[i].frame;
            *accent = sThemed[i].accent;
            *index = (src - sThemed[i].pal) & 15;
            return sThemed[i].mode;
        }
    }
    *index = (src - GetTextWindowPalette(0)) & 15;
    if (src >= GetTextWindowPalette(0) && src < GetTextWindowPalette(4) + 16)
    {
        if (src >= GetTextWindowPalette(2) && src < GetTextWindowPalette(2) + 16)
            *keep = 0xFC00;   // its colors 10-15 draw the blue title bars (naming screen, Hall of Fame PC)
        else
            *frame = 0xFC00;  // the others' colors 10-15 draw window / sign frames
        return MODE_TEXT;
    }
    for (i = 0; i < WINDOW_FRAMES_COUNT; i++)
    {
        const u16 *frame = GetWindowFrameTilesPal(i)->pal;
        if (src >= frame && src < frame + 16)
        {
            *index = src - frame;
            return MODE_FRAME;
        }
    }
    *index = 0;
    return -1;
}

#ifndef RELEASE
// Test builds: the source of every palette row's color 0 (for working out which palette draws what on screen).
EWRAM_DATA const u16 *gRhThemeRowSrc[PLTT_BUFFER_SIZE / 16] = {0};
#endif

// Called by LoadPalette / LoadPaletteFast after the copy (only when a theme is on).
void RH_ThemeLoadedPalette(const void *src, u32 offset, u32 size)
{
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme, i, end, index;
    const u16 *pal = src;
    u16 keep, white, merge, frame, accent;
    u32 start, sum[3], count, j;
    s32 mode;
    if (theme == RH_THEME_DEFAULT || offset >= PLTT_BUFFER_SIZE)
        return;
#ifndef RELEASE
    for (i = offset & ~15; i < offset + size / 2 && i < PLTT_BUFFER_SIZE; i += 16)
        gRhThemeRowSrc[i / 16] = pal + i - offset;
#endif
    size /= 2;
    if (offset + size > PLTT_BUFFER_SIZE)
        size = PLTT_BUFFER_SIZE - offset;
    // A load can span several table entries (the party menu loads all 11 palettes at once), so look each color up.
    for (i = 0; i < size; i = end)
    {
        mode = ThemedMode(&pal[i], offset + i, &keep, &white, &merge, &frame, &accent, &index);
        end = i + 16 - index;   // table entries are whole rows of 16
        if (end > size)
            end = size;
        if (mode < 0)
            continue;
        start = i;
        sum[0] = sum[1] = sum[2] = count = 0;
        for (; i < end; i++, index++)
        {
            u16 *color = &gPlttBufferUnfaded[offset + i];
            if (white & (1 << index))
                *color = RGB_WHITE;
            else if (accent & (1 << index))
                *color = AccentColor(*color, theme);
            else if (!(keep & (1 << index)))
                *color = ThemeColor(*color, theme, (frame & (1 << index)) ? MODE_FRAME : mode);
            if (merge & (1 << index))
            {
                for (j = 0; j < 3; j++)
                    sum[j] += (*color >> (j * 5)) & 31;
                count++;
            }
        }
        if (count > 1)
        {
            u16 average = RGB((sum[0] + count / 2) / count, (sum[1] + count / 2) / count, (sum[2] + count / 2) / count);
            for (j = start; j < end; j++)
            {
                if (merge & (1 << (index - (end - j))))
                    gPlttBufferUnfaded[offset + j] = average;
            }
        }
    }
    CpuCopy16(&gPlttBufferUnfaded[offset], &gPlttBufferFaded[offset], size * 2);
    ApplyOverrides(pal, offset, size, FALSE);
}

// For screens whose palettes are private to their own file: theme colors just loaded at offset (row-aligned keep mask).
void RH_ThemeLoadedRange(u32 offset, u32 count, u32 mode, u16 keep)
{
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme, i;
    if (theme == RH_THEME_DEFAULT || offset >= PLTT_BUFFER_SIZE)
        return;
    if (offset + count > PLTT_BUFFER_SIZE)
        count = PLTT_BUFFER_SIZE - offset;
    for (i = offset; i < offset + count; i++)
    {
        if (!(keep & (1 << (i & 15))))
            gPlttBufferUnfaded[i] = ThemeColor(gPlttBufferUnfaded[i], theme, mode);
    }
    CpuCopy16(&gPlttBufferUnfaded[offset], &gPlttBufferFaded[offset], count * 2);
}

// Randomizer theme: colors (mask, in every row of 16) of a palette just loaded at offset become the brand purple.
void RH_ThemeAccentRange(u32 offset, u16 mask)
{
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme, i;
    if (theme != RH_THEME_RANDOMIZER || offset >= PLTT_BUFFER_SIZE)
        return;
    for (i = 0; i < 16 && offset + i < PLTT_BUFFER_SIZE; i++)
    {
        if (mask & (1 << ((offset + i) & 15)))
        {
            gPlttBufferUnfaded[offset + i] = AccentColor(gPlttBufferUnfaded[offset + i], theme);
            gPlttBufferFaded[offset + i] = gPlttBufferUnfaded[offset + i];
        }
    }
}

// Battle move menu: the normal PP numbers (state 3 of gPPTextPalette; the low-PP colors stay) in the owner's colors.
// Returns TRUE if it changed them.
bool32 RH_ThemePPNumberColors(u32 state, u16 *text, u16 *shadow)
{
    static const u16 sText[RH_THEME_COUNT - 1] = { RGB(28, 28, 29), RGB(30, 30, 30) };
    static const u16 sShadow[RH_THEME_COUNT - 1] = { RGB(7, 8, 11), RGB(6, 6, 6) };
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme;
    if (theme == RH_THEME_DEFAULT || theme >= RH_THEME_COUNT || state != 3)
        return FALSE;
    *text = sText[theme - 1];
    *shadow = sShadow[theme - 1];
    return TRUE;
}


// Battle cursor arrow (AMOLED): its shadow (color 7) is also the menu frames' corner color, so in the cursor's tiles
// (1-2 of the battle text box graphics) it moves to spare color 10 (dark red, battle_bg.c).
static u32 RecolorCursorShadow(u32 x, u32 y, u32 c, const u8 *block)
{
    return c == 7 ? 10 : c;
}

void RH_ThemeBattleCursorTiles(void)
{
    static const u8 sCursor[] = { 1, 2 };
    if (gSaveBlock3Ptr->rhSettings.uiTheme != RH_THEME_AMOLED)
        return;
    RecolorVramTiles((u16 *)BG_CHAR_ADDR(0), 0, 2, 1, sCursor, RecolorCursorShadow, NULL);
}
