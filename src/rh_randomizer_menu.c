// New Game randomizer settings screen (Universal Pokemon Randomizer style).
// Tabs: L/R or LEFT/RIGHT on the tab bar. Rows: UP/DOWN. Values: LEFT/RIGHT. START: begin the adventure.
#include "global.h"
#include "bg.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "menu.h"
#include "oak_speech.h"
#include "palette.h"
#include "random.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "rh.h"
#include "constants/rgb.h"
#include "constants/songs.h"

enum { WIN_HEADER, WIN_OPTIONS, WIN_DESC };

#define ROWS_VISIBLE 5
#define ROW_HEIGHT 16
#define FRAME_TILE 0x200

struct RhOption
{
    u8 tab;
    u8 offset;           // offsetof(struct RhSettings, field); 0xFF = special row
    u8 count;
    const u8 *label;
    const u8 *const *values;
    const u8 *desc;
};

enum { ROW_ENABLED = 0xF0, ROW_PRESET, ROW_SEED };

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------
static const u8 *const sOffOn[] = { COMPOUND_STRING("OFF"), COMPOUND_STRING("ON") };
static const u8 *const sNoYes[] = { COMPOUND_STRING("NO"), COMPOUND_STRING("YES") };
static const u8 *const sPool[] = { COMPOUND_STRING("KANTO 151"), COMPOUND_STRING("GEN 1-3"), COMPOUND_STRING("ALL 1025"), COMPOUND_STRING("ALL + FORMS") };
static const u8 *const sPresets[] = { COMPOUND_STRING("CUSTOM"), COMPOUND_STRING("CLASSIC"), COMPOUND_STRING("NUZLOCKE"), COMPOUND_STRING("CHAOS") };
static const u8 *const sStats[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("SHUFFLE"), COMPOUND_STRING("RANDOM") };
static const u8 *const sTypes[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("FOLLOW EVOS"), COMPOUND_STRING("RANDOM") };
static const u8 *const sUnchRandom[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("RANDOM") };
static const u8 *const sEvos[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("SIMILAR STR.") };
static const u8 *const sStarters[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("3-STAGE ONLY") };
static const u8 *const sStatics[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("SWAP LEGENDS"), COMPOUND_STRING("RANDOM") };
static const u8 *const sTrades[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("GIVEN ONLY"), COMPOUND_STRING("BOTH") };
static const u8 *const sMovesets[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("PREFER TYPE"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("METRONOME") };
static const u8 *const sDamaging[] = { COMPOUND_STRING("OFF"), COMPOUND_STRING("25%"), COMPOUND_STRING("50%"), COMPOUND_STRING("75%") };
static const u8 *const sTrainers[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("SIMILAR STR."), COMPOUND_STRING("TYPE THEMED"), COMPOUND_STRING("GYM THEMES") };
static const u8 *const sForceEvo[] = { COMPOUND_STRING("OFF"), COMPOUND_STRING("LV 30+"), COMPOUND_STRING("LV 40+"), COMPOUND_STRING("LV 50+") };
static const u8 *const sLevels[] = { COMPOUND_STRING("-50%"), COMPOUND_STRING("-40%"), COMPOUND_STRING("-30%"), COMPOUND_STRING("-20%"), COMPOUND_STRING("-10%"),
                                     COMPOUND_STRING("NORMAL"), COMPOUND_STRING("+10%"), COMPOUND_STRING("+20%"), COMPOUND_STRING("+30%"), COMPOUND_STRING("+40%"), COMPOUND_STRING("+50%") };
static const u8 *const sWild[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("AREA 1-TO-1"), COMPOUND_STRING("GLOBAL 1-TO-1") };
static const u8 *const sCatch[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("EASIER"), COMPOUND_STRING("MUCH EASIER"), COMPOUND_STRING("GUARANTEED") };
static const u8 *const sCompat[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("PREFER TYPE"), COMPOUND_STRING("RANDOM"), COMPOUND_STRING("FULL") };
static const u8 *const sFieldItems[] = { COMPOUND_STRING("UNCHANGED"), COMPOUND_STRING("SHUFFLE"), COMPOUND_STRING("RANDOM") };

static const u8 *const sTabNames[] = {
    COMPOUND_STRING("GENERAL"), COMPOUND_STRING("POKéMON TRAITS"), COMPOUND_STRING("STARTERS & GIFTS"),
    COMPOUND_STRING("MOVES & MOVESETS"), COMPOUND_STRING("FOE POKéMON"), COMPOUND_STRING("WILD POKéMON"),
    COMPOUND_STRING("TMs & TUTORS"), COMPOUND_STRING("ITEMS"),
};
#define NUM_TABS ARRAY_COUNT(sTabNames)

#define OPT(tab_, field, list, label_, desc_) { .tab = tab_, .offset = offsetof(struct RhSettings, field), .count = ARRAY_COUNT(list), .label = COMPOUND_STRING(label_), .values = list, .desc = COMPOUND_STRING(desc_) }
#define SPECIAL(tab_, id, list, label_, desc_) { .tab = tab_, .offset = id, .count = ARRAY_COUNT(list), .label = COMPOUND_STRING(label_), .values = list, .desc = COMPOUND_STRING(desc_) }

static const struct RhOption sOptions[] =
{
    SPECIAL(0, ROW_ENABLED, sOffOn, "RANDOMIZER", "Turn the randomizer on or off.\nOFF plays the normal game."),
    SPECIAL(0, ROW_PRESET, sPresets, "QUICK PRESET", "Load a ready-made set of options.\nYou can still tweak them after."),
    OPT(0, speciesPool, sPool, "POKéMON POOL", "Which Pokémon can show up.\n+FORMS adds regional/alt forms."),
    SPECIAL(0, ROW_SEED, sOffOn, "SEED", "Same seed + options = same game.\n{A_BUTTON} Edit digits  SELECT New seed"),

    OPT(1, baseStats, sStats, "BASE STATS", "SHUFFLE swaps stats around.\nRANDOM redistributes the total."),
    OPT(1, baseStatsFollowEvos, sNoYes, "  FOLLOW EVOS", "Evolutions keep the same\nstat shape as their family."),
    OPT(1, types, sTypes, "TYPES", "FOLLOW EVOS keeps a family's\ntypes the same as it evolves."),
    OPT(1, abilities, sUnchRandom, "ABILITIES", "Give every Pokémon random\nabilities."),
    OPT(1, abilitiesFollowEvos, sNoYes, "  FOLLOW EVOS", "Evolutions keep their family's\nrandom abilities."),
    OPT(1, banWonderGuard, sNoYes, "  BAN WONDER GUARD", "Never hand out Wonder Guard."),
    OPT(1, banTrapAbilities, sNoYes, "  BAN TRAPPING", "Never hand out Arena Trap,\nShadow Tag or Magnet Pull."),
    OPT(1, banBadAbilities, sNoYes, "  BAN BAD ABILITIES", "No Truant, Slow Start, Stall,\nDefeatist or Klutz."),
    OPT(1, evolutions, sEvos, "EVOLUTIONS", "Evolve into random Pokémon.\nSIMILAR STR. keeps power close."),
    OPT(1, evoSameType, sNoYes, "  SHARE A TYPE", "Random evolutions share a\ntype with the Pokémon."),

    OPT(2, starters, sStarters, "STARTERS", "3-STAGE ONLY picks basics\nthat evolve twice."),
    OPT(2, statics, sStatics, "STATIC POKéMON", "Gifts, legends and set fights.\nSWAP keeps legends legendary."),
    OPT(2, trades, sTrades, "IN-GAME TRADES", "Randomize NPC trade Pokémon.\nBOTH also changes what they ask."),

    OPT(3, movesets, sMovesets, "MOVESETS", "Random level-up moves. PREFER\nTYPE favors the Pokémon's type."),
    OPT(3, goodDamaging, sDamaging, "  DAMAGING MOVES", "At least this share of learned\nmoves can deal damage."),
    OPT(3, banBrokenMoves, sNoYes, "  BAN BROKEN MOVES", "No OHKO or fixed-damage moves\nin random movesets and TMs."),
    OPT(3, movePower, sOffOn, "MOVE POWER", "Randomize attack power\n(20 to 140)."),
    OPT(3, moveAccuracy, sOffOn, "MOVE ACCURACY", "Randomize move accuracy\n(60% to 100%)."),
    OPT(3, movePP, sOffOn, "MOVE PP", "Randomize move PP\n(5 to 40)."),
    OPT(3, moveType, sOffOn, "MOVE TYPES", "Randomize the type of\nevery move."),
    OPT(3, moveCategory, sOffOn, "PHYS./SPECIAL", "Randomly make attacks\nphysical or special."),

    OPT(4, trainers, sTrainers, "TRAINER POKéMON", "GYM THEMES: Gyms & Elite Four\nkeep types, others go random."),
    OPT(4, rivalStarter, sNoYes, "  RIVAL HAS STARTER", "Your rival keeps using the\nstarter he picked."),
    OPT(4, trainerNoLegends, sNoYes, "  NO LEGENDARIES", "Trainers never use legendary\nor mythical Pokémon."),
    OPT(4, trainerForceEvolved, sForceEvo, "  FORCE EVOLVED", "Trainer Pokémon at this level\nor higher are fully evolved."),
    OPT(4, trainerLevel, sLevels, "TRAINER LEVELS", "Raise or lower every trainer\nPokémon's level."),
    OPT(4, trainerItems, sNoYes, "RANDOM HELD ITEMS", "Trainer Pokémon with no item\nget a random held item."),

    OPT(5, wild, sWild, "WILD POKéMON", "AREA: every route gets its own\nmix. GLOBAL: one swap for all."),
    OPT(5, wildSimilar, sNoYes, "  SIMILAR STRENGTH", "Replacements have a similar\nbase stat total."),
    OPT(5, wildTypeThemed, sNoYes, "  TYPE THEMED AREAS", "Each area gets a random type;\nonly that type appears there."),
    OPT(5, wildNoLegends, sNoYes, "  NO LEGENDARIES", "Keep legendaries out of\nthe wild."),
    OPT(5, wildLevel, sLevels, "WILD LEVELS", "Raise or lower every wild\nPokémon's level."),
    OPT(5, wildCatchRate, sCatch, "CATCH RATE", "Make wild Pokémon easier\nto catch."),

    OPT(6, tmMoves, sUnchRandom, "TM MOVES", "TMs teach random moves.\nHMs are never changed."),
    OPT(6, tmCompat, sCompat, "TM COMPATIBILITY", "Who can learn each TM. FULL:\neveryone learns all TMs/HMs."),
    OPT(6, tutorCompat, sCompat, "TUTOR COMPAT.", "Who can learn move tutor\nmoves."),

    OPT(7, fieldItems, sFieldItems, "FIELD ITEMS", "SHUFFLE moves items around.\nRANDOM puts new items out."),
    OPT(7, banBadItems, sNoYes, "  BAN JUNK ITEMS", "Keep near-useless items out\nof the random item pool."),
    OPT(7, shopItems, sUnchRandom, "SHOP ITEMS", "Marts sell random items.\nBalls and medicine stay."),
};

// ---------------------------------------------------------------------------
// Window / bg setup
// ---------------------------------------------------------------------------
static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER]  = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 1,  .width = 26, .height = 2,  .paletteNum = 1, .baseBlock = 2 },
    [WIN_OPTIONS] = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 5,  .width = 26, .height = 10, .paletteNum = 1, .baseBlock = 2 + 52 },
    [WIN_DESC]    = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 16, .width = 26, .height = 4,  .paletteNum = 1, .baseBlock = 2 + 52 + 260 },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
};

static const u16 sTextPal[] = {
    RGB(0, 0, 0), RGB(31, 31, 31), RGB(9, 9, 9), RGB(22, 22, 21),
    RGB(28, 6, 4), RGB(31, 20, 18), RGB(3, 18, 6), RGB(17, 29, 17),
    RGB(4, 10, 28), RGB(18, 22, 31), RGB(31, 31, 31), RGB(31, 31, 31),
    RGB(31, 31, 31), RGB(31, 31, 31), RGB(31, 31, 31), RGB(31, 31, 31),
};
static const u16 sBgColor[] = { RGB(8, 12, 22) };

static const u8 sColorNormal[] = { 1, 2, 3 };
static const u8 sColorValue[]  = { 1, 4, 5 };
static const u8 sColorOff[]    = { 1, 3, 1 };
static const u8 sColorTitle[]  = { 1, 8, 9 };

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static EWRAM_DATA u8 sTab = 0;
static EWRAM_DATA u8 sRow = 0;
static EWRAM_DATA u8 sScroll = 0;
static EWRAM_DATA u8 sPreset = 0;
static EWRAM_DATA bool8 sConfirming = FALSE;
static EWRAM_DATA s8 sSeedCursor = 0;    // >= 0 while editing the seed (-1 otherwise; set on init)

static u8 *Field(u8 offset)
{
    return ((u8 *)&gRhPendingSettings) + offset;
}

static u32 RowsInTab(u32 tab, const struct RhOption **rows)
{
    u32 i, n = 0;
    for (i = 0; i < ARRAY_COUNT(sOptions); i++)
        if (sOptions[i].tab == tab)
            rows[n++] = &sOptions[i];
    return n;
}

static u8 GetValue(const struct RhOption *o)
{
    switch (o->offset)
    {
    case ROW_ENABLED: return gRhPendingSettings.enabled;
    case ROW_PRESET:  return sPreset;
    case ROW_SEED:    return 0;
    }
    return *Field(o->offset);
}

static void ApplyPreset(u32 preset)
{
    struct RhSettings *s = &gRhPendingSettings;
    u32 seed = s->seed;
    u8 pool = s->speciesPool;
    if (preset == 0)
        return;
    RH_SetDefaultSettings(s);
    s->seed = seed;
    s->speciesPool = pool;
    s->enabled = TRUE;
    switch (preset)
    {
    case 1: // Classic: the usual randomizer run
        s->starters = 2; s->statics = 1; s->trades = 1;
        s->wild = 2; s->wildSimilar = TRUE;
        s->trainers = 4; s->trainerForceEvolved = 2;
        s->tmMoves = 1; s->tmCompat = 1; s->fieldItems = 2;
        break;
    case 2: // Nuzlocke-friendly: every route different, fair fights
        s->starters = 2; s->statics = 1;
        s->wild = 2; s->wildSimilar = TRUE; s->wildNoLegends = TRUE;
        s->trainers = 2; s->trainerNoLegends = TRUE;
        s->wildCatchRate = 1; s->fieldItems = 1;
        break;
    case 3: // Chaos: everything
        s->baseStats = 2; s->types = 2; s->abilities = 1; s->evolutions = 1;
        s->starters = 1; s->statics = 2; s->trades = 2;
        s->movesets = 2; s->goodDamaging = 2; s->movePower = s->moveAccuracy = s->movePP = s->moveType = s->moveCategory = TRUE;
        s->trainers = 1; s->trainerItems = TRUE;
        s->wild = 1; s->tmMoves = 1; s->tmCompat = 2; s->tutorCompat = 2;
        s->fieldItems = 2; s->shopItems = 1;
        break;
    }
}

static void SetValue(const struct RhOption *o, u8 v)
{
    switch (o->offset)
    {
    case ROW_ENABLED: gRhPendingSettings.enabled = v; return;
    case ROW_PRESET:  sPreset = v; ApplyPreset(v); return;
    case ROW_SEED:    return;
    }
    *Field(o->offset) = v;
    sPreset = 0;
    gRhPendingSettings.enabled = TRUE;   // touching any option switches the randomizer on
}

static void NewSeed(void)
{
    gRhPendingSettings.seed = Random32() ^ (gMain.vblankCounter1 * 2654435761u);
    if (gRhPendingSettings.seed == 0)
        gRhPendingSettings.seed = 1;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void DrawHeader(void)
{
    u8 buf[48];
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 4, 1, sColorTitle, TEXT_SKIP_DRAW, COMPOUND_STRING("RANDOMIZER"));
    StringCopy(buf, COMPOUND_STRING("{L_BUTTON} "));
    StringAppend(buf, sTabNames[sTab]);
    StringAppend(buf, COMPOUND_STRING(" {R_BUTTON}"));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, buf, 204), 1, sColorNormal, TEXT_SKIP_DRAW, buf);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void SeedString(u8 *dst)
{
    dst = ConvertIntToHexStringN(dst, gRhPendingSettings.seed >> 16, STR_CONV_MODE_LEADING_ZEROS, 4);
    ConvertIntToHexStringN(dst, gRhPendingSettings.seed & 0xFFFF, STR_CONV_MODE_LEADING_ZEROS, 4);
}

static void DrawRows(void)
{
    const struct RhOption *rows[ARRAY_COUNT(sOptions)];
    u32 n = RowsInTab(sTab, rows), i;
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < ROWS_VISIBLE && sScroll + i < n; i++)
    {
        const struct RhOption *o = rows[sScroll + i];
        u8 buf[24];
        const u8 *value;
        bool32 dim = !gRhPendingSettings.enabled && o->offset != ROW_ENABLED;
        u32 y = i * ROW_HEIGHT + 1;
        AddTextPrinterParameterized3(WIN_OPTIONS, FONT_NORMAL, 8, y, dim ? sColorOff : sColorNormal, TEXT_SKIP_DRAW, o->label);
        if (o->offset == ROW_SEED)
        {
            SeedString(buf);
            value = buf;
            if (sSeedCursor >= 0)
            {
                // draw digit by digit so the edited one can be highlighted
                u32 x = GetStringRightAlignXOffset(FONT_NORMAL, value, 200), d;
                for (d = 0; d < 8; d++)
                {
                    u8 ch[2] = { buf[d], EOS };
                    AddTextPrinterParameterized3(WIN_OPTIONS, FONT_NORMAL, x, y, d == sSeedCursor ? sColorTitle : sColorValue, TEXT_SKIP_DRAW, ch);
                    x += GetStringWidth(FONT_NORMAL, ch, 0);
                }
                continue;
            }
        }
        else
        {
            value = o->values[min(GetValue(o), o->count - 1)];
        }
        AddTextPrinterParameterized3(WIN_OPTIONS, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, value, 200), y,
                                     dim ? sColorOff : sColorValue, TEXT_SKIP_DRAW, value);
    }
    // scroll hints
    if (sScroll > 0)
        AddTextPrinterParameterized3(WIN_OPTIONS, FONT_SMALL, 0, 0, sColorOff, TEXT_SKIP_DRAW, COMPOUND_STRING("{UP_ARROW}"));
    if (sScroll + ROWS_VISIBLE < n)
        AddTextPrinterParameterized3(WIN_OPTIONS, FONT_SMALL, 0, ROWS_VISIBLE * ROW_HEIGHT - 10, sColorOff, TEXT_SKIP_DRAW, COMPOUND_STRING("{DOWN_ARROW}"));
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE((sRow - sScroll) * ROW_HEIGHT + 40, (sRow - sScroll) * ROW_HEIGHT + 56));
}

static void DrawDesc(void)
{
    const struct RhOption *rows[ARRAY_COUNT(sOptions)];
    u32 n = RowsInTab(sTab, rows);
    FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(1));
    if (sConfirming)
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 4, 1, sColorNormal, TEXT_SKIP_DRAW,
            gRhPendingSettings.enabled ? COMPOUND_STRING("Start a RANDOMIZED adventure?\n{A_BUTTON} Yes   {B_BUTTON} Back to options")
                                       : COMPOUND_STRING("Start a NORMAL (not randomized) game?\n{A_BUTTON} Yes   {B_BUTTON} Back to options"));
    }
    else if (sRow < n)
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 4, 1, sColorNormal, TEXT_SKIP_DRAW, rows[sRow]->desc);
    }
    CopyWindowToVram(WIN_DESC, COPYWIN_GFX);
}

static void DrawFrame(u32 left, u32 top, u32 width, u32 height)
{
    FillBgTilemapBufferRect(1, FRAME_TILE + 0, left - 1, top - 1, 1, 1, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 1, left, top - 1, width, 1, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 2, left + width, top - 1, 1, 1, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 3, left - 1, top, 1, height, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 5, left + width, top, 1, height, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 6, left - 1, top + height, 1, 1, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 7, left, top + height, width, 1, 7);
    FillBgTilemapBufferRect(1, FRAME_TILE + 8, left + width, top + height, 1, 1, 7);
}

static void DrawAll(void)
{
    DrawHeader();
    DrawRows();
    DrawDesc();
}

// ---------------------------------------------------------------------------
// Callbacks / input
// ---------------------------------------------------------------------------
static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void Task_Input(u8 taskId);
static void Task_FadeOut(u8 taskId);

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_Input;
}

static void ChangeTab(s32 delta)
{
    sTab = (sTab + NUM_TABS + delta) % NUM_TABS;
    sRow = 0;
    sScroll = 0;
    PlaySE(SE_SELECT);
    DrawAll();
}

static void Task_Input(u8 taskId)
{
    const struct RhOption *rows[ARRAY_COUNT(sOptions)];
    u32 n = RowsInTab(sTab, rows);
    const struct RhOption *o = rows[sRow];

    if (sConfirming)
    {
        if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            gTasks[taskId].func = Task_FadeOut;
        }
        else if (JOY_NEW(B_BUTTON))
        {
            sConfirming = FALSE;
            DrawDesc();
        }
        return;
    }

    if (sSeedCursor >= 0)
    {
        u32 shift = (7 - sSeedCursor) * 4;
        u32 digit = (gRhPendingSettings.seed >> shift) & 0xF;
        if (JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON))
            sSeedCursor = -1;
        else if (JOY_NEW(DPAD_LEFT))
            sSeedCursor = (sSeedCursor + 7) % 8;
        else if (JOY_NEW(DPAD_RIGHT))
            sSeedCursor = (sSeedCursor + 1) % 8;
        else if (JOY_NEW(DPAD_UP | DPAD_DOWN))
        {
            digit = JOY_NEW(DPAD_UP) ? (digit + 1) & 0xF : (digit + 15) & 0xF;
            gRhPendingSettings.seed = (gRhPendingSettings.seed & ~(0xFu << shift)) | (digit << shift);
            if (gRhPendingSettings.seed == 0)
                gRhPendingSettings.seed = 1;
        }
        else
            return;
        PlaySE(SE_SELECT);
        DrawRows();
        return;
    }

    if (JOY_NEW(START_BUTTON))
    {
        PlaySE(SE_SELECT);
        sConfirming = TRUE;
        DrawDesc();
    }
    else if (JOY_NEW(L_BUTTON))
        ChangeTab(-1);
    else if (JOY_NEW(R_BUTTON))
        ChangeTab(1);
    else if (JOY_NEW(DPAD_UP))
    {
        sRow = (sRow == 0) ? n - 1 : sRow - 1;
        if (sRow < sScroll)
            sScroll = sRow;
        if (sRow >= sScroll + ROWS_VISIBLE)
            sScroll = sRow - ROWS_VISIBLE + 1;
        PlaySE(SE_SELECT);
        DrawRows();
        DrawDesc();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sRow = (sRow + 1 >= n) ? 0 : sRow + 1;
        if (sRow < sScroll)
            sScroll = sRow;
        if (sRow >= sScroll + ROWS_VISIBLE)
            sScroll = sRow - ROWS_VISIBLE + 1;
        PlaySE(SE_SELECT);
        DrawRows();
        DrawDesc();
    }
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        if (o->offset == ROW_SEED)
        {
            return;
        }
        else
        {
            u8 v = GetValue(o);
            if (JOY_NEW(DPAD_RIGHT))
                v = (v + 1) % o->count;
            else
                v = (v == 0) ? o->count - 1 : v - 1;
            SetValue(o, v);
        }
        PlaySE(SE_SELECT);
        DrawRows();
    }
    else if (JOY_NEW(A_BUTTON) && o->offset == ROW_SEED)
    {
        sSeedCursor = 0;
        PlaySE(SE_SELECT);
        DrawRows();
    }
    else if (JOY_NEW(SELECT_BUTTON) && o->offset == ROW_SEED)
    {
        NewSeed();
        PlaySE(SE_SELECT);
        DrawRows();
    }
}

static void Task_FadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        StartNewGameSceneFrlg();
    }
}

void CB2_InitRandomizerMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        InitWindows(sWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0 | WININ_WIN0_BG1);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 3);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, FRAME_TILE);
        LoadPalette(sBgColor, BG_PLTT_ID(0), sizeof(sBgColor));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        LoadPalette(sTextPal, BG_PLTT_ID(1), sizeof(sTextPal));
        gMain.state++;
        break;
    case 4:
        if (gRhPendingSettings.version != RH_SETTINGS_VERSION)
        {
            RH_SetDefaultSettings(&gRhPendingSettings);
            NewSeed();
        }
        sTab = sRow = sScroll = 0;
        sPreset = 0;
        sConfirming = FALSE;
        sSeedCursor = -1;
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_OPTIONS);
        PutWindowTilemap(WIN_DESC);
        DrawFrame(2, 1, 26, 2);
        DrawFrame(2, 5, 26, 15);
        FillBgTilemapBufferRect(1, FRAME_TILE + 7, 2, 15, 26, 1, 7);   // divider above the description
        CopyBgTilemapBufferToVram(1);
        DrawAll();
        gMain.state++;
        break;
    case 5:
        CreateTask(Task_FadeIn, 0);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}
