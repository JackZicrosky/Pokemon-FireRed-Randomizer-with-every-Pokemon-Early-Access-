// UI Theme (Misc. Tweaks): Default / Randomizer / AMOLED.
// The game's menus are drawn for a white background. Instead of inverting them, every palette that belongs to a
// menu, text box or window frame is re-lit when it is loaded: each color keeps its hue and saturation, but its
// lightness goes through a curve that turns the white backgrounds dark, the dark text light and the light text
// shadows dark. Randomizer uses the randomizer screen's blue-grey colors; AMOLED goes pitch black with grey accents.
#include "global.h"
#include "graphics.h"
#include "constants/rgb.h"
#include "menu.h"
#include "palette.h"
#include "text_window.h"
#include "rh.h"
#include "rh_internal.h"

#define MODE_TEXT   RH_THEME_MODE_TEXT
#define MODE_SCREEN RH_THEME_MODE_SCREEN

// Lightness curves (old lightness -> new lightness, 0..256).
// Text: through black, standard text grey (12,12,12), standard text shadow (26,26,25), white.
// Screen greys: like text, but mid greys (outlines, shading, dark backgrounds) stay dark.
#define CURVE_POINTS 5
static const s16 sCurve[2][RH_THEME_COUNT - 1][CURVE_POINTS][2] =
{
    [MODE_TEXT] = {
        [RH_THEME_RANDOMIZER - 1] = { {0, 248}, {99, 232}, {157, 151}, {215, 70}, {256, 33} },   // white -> panel (3,4,5)
        [RH_THEME_AMOLED - 1]     = { {0, 256}, {99, 240}, {157, 140}, {215, 40}, {256, 0} },    // white -> black
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
    if (mode == MODE_SCREEN && chroma >= 8 * 8)
    {
        // Colored art: the same picture, much darker (AMOLED: nearly black, with a hint of its color).
        if (theme == RH_THEME_AMOLED)
            nl = 1 + l * 22 / 248;
        else
            nl = 20 + l * 72 / 248;
    }
    else
    {
        nl = Curve(sCurve[mode][theme - 1], l * 256 / 248) * 248 / 256;
    }

    // Keep the saturation (chroma relative to the most a color of that lightness can have).
    cmax = 248 - abs(2 * l - 248);
    newChroma = (cmax != 0) ? chroma * (248 - abs(2 * nl - 248)) / cmax : 0;
    if (newChroma > chroma)
        newChroma = chroma;   // never more colorful than before (pale tints stay subtle when they turn dark)
    if (mode == MODE_SCREEN && l >= 168)
        newChroma = newChroma * (l >= 200 ? 2 : 3) / 5;   // pale "paper" surfaces become near-neutral dark panels
    if (theme == RH_THEME_AMOLED && chroma < 3 * 8)
        newChroma = 0;   // near-greys become pure greys
    else if (theme == RH_THEME_AMOLED && mode == MODE_SCREEN)
        newChroma = newChroma / 2;

    for (i = 0; i < 3; i++)
    {
        v = (chroma != 0) ? nl + (c[i] - l) * newChroma / chroma : nl;
        if (theme == RH_THEME_RANDOMIZER && chroma < 2 * 8)
            v += (i - 1) * 8;   // greys get the randomizer screen's blue tint, like (3,4,5) and (28,28,29)
        v = (v + 4) / 8;
        if (v < 0)
            v = 0;
        if (v > 31)
            v = 31;
        out |= v << (i * 5);
    }
    return out;
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
};

// Menu, text box and window palettes (anything drawn on a white base). Loading one of these anywhere gets themed.
static const struct ThemedPal sThemed[] =
{
    { gStandardMenuPalette, 16, MODE_TEXT },
    { gMessageBox_Pal, 16, MODE_TEXT },
    { gBagScreenMale_Pal, 32, MODE_SCREEN, .merge = (1 << 12) | (1 << 13) },     // (12-13: the two background stripes)
    { gBagScreenFemale_Pal, 32, MODE_SCREEN, .merge = (1 << 12) | (1 << 13) },
    { gPartyMenuBg_Pal, 16, MODE_SCREEN },
    { gPartyMenuBg_Pal + 16, 2 * 16, MODE_SCREEN, (1 << 1) | (1 << 10), .merge = (1 << 4) | (1 << 5) },   // background stripes, Cancel/Confirm
    { gPartyMenuBg_Pal + 48, 8 * 16, MODE_SCREEN, (1 << 1) | (1 << 2) | (1 << 3) | (3 << 9) | (7 << 13) },   // Pokemon boxes (text, HP bar)
    { gSummaryScreen_Pal, 6 * 16, MODE_SCREEN, 1 << 2 },
    { gSummaryScreen_Pal + 96, 2 * 16, MODE_TEXT, (1 << 3) | (1 << 4) },   // (3-4: white labels with a grey shadow)
    { gPPTextPalette, 16, MODE_TEXT },
    { gShopMenu_Pal, 16, MODE_SCREEN },
    // Battle message box. Its white text moves to spare color 8 (battle_message.c), because color 1 is also the
    // background of the action menu's cursor.
    { gBattleTextboxPalette, 16, MODE_SCREEN, (7 << 2) | (1 << 6), 1 << 8 },   // (2-4: the red "more text" arrow)
    { gBattleTextboxPalette + 16, 16, MODE_TEXT },
    { gPokedexBgHoenn_Pal, 6 * 16, MODE_SCREEN, (1 << 8) | (1 << 9) },   // (the navy panel and its white text)
    { gPokedexBgNational_Pal, 6 * 16, MODE_SCREEN, (1 << 8) | (1 << 9) },
    { gPokedexSearchResults_Pal, 6 * 16, MODE_SCREEN, (1 << 8) | (1 << 9) },
    { gPokedexSearchMenu_Pal, 4 * 16, MODE_SCREEN },
    { gBattleWindowTextPalette, 16, MODE_TEXT },
};

// Returns the mode for a themed palette, or -1.
// Returns the mode for a color of a themed palette (or -1), its keep mask and its index in its row of 16.
static s32 ThemedMode(const u16 *src, u16 *keep, u16 *white, u16 *merge, u32 *index)
{
    u32 i;
    *keep = *white = *merge = 0;
    for (i = 0; i < ARRAY_COUNT(sThemed); i++)
    {
        if (src >= sThemed[i].pal && src < sThemed[i].pal + sThemed[i].count)
        {
            *keep = sThemed[i].keep;
            *white = sThemed[i].white;
            *merge = sThemed[i].merge;
            *index = (src - sThemed[i].pal) & 15;
            return sThemed[i].mode;
        }
    }
    *index = (src - GetTextWindowPalette(0)) & 15;
    if (src >= GetTextWindowPalette(0) && src < GetTextWindowPalette(4) + 16)
    {
        if (src >= GetTextWindowPalette(2) && src < GetTextWindowPalette(2) + 16)
            *keep = 0xFC00;   // its colors 10-15 draw the blue title bars (naming screen, Hall of Fame PC)
        return MODE_TEXT;
    }
    for (i = 0; i < WINDOW_FRAMES_COUNT; i++)
    {
        const u16 *frame = GetWindowFrameTilesPal(i)->pal;
        if (src >= frame && src < frame + 16)
        {
            *index = src - frame;
            return MODE_TEXT;
        }
    }
    *index = 0;
    return -1;
}

// Called by LoadPalette / LoadPaletteFast after the copy (only when a theme is on).
void RH_ThemeLoadedPalette(const void *src, u32 offset, u32 size)
{
    u32 theme = gSaveBlock3Ptr->rhSettings.uiTheme, i, end, index;
    const u16 *pal = src;
    u16 keep, white, merge;
    u32 start, sum[3], count, j;
    s32 mode;
    if (theme == RH_THEME_DEFAULT || offset >= PLTT_BUFFER_SIZE)
        return;
    size /= 2;
    if (offset + size > PLTT_BUFFER_SIZE)
        size = PLTT_BUFFER_SIZE - offset;
    // A load can span several table entries (the party menu loads all 11 palettes at once), so look each color up.
    for (i = 0; i < size; i = end)
    {
        mode = ThemedMode(&pal[i], &keep, &white, &merge, &index);
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
            else if (!(keep & (1 << index)))
                *color = ThemeColor(*color, theme, mode);
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
