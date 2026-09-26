// Where Pokemon come from: starters, static encounters, in-game trades and wild encounters.
#include "global.h"
#include "constants/characters.h"
#include "battle_util.h"
#include "event_data.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "string_util.h"
#include "menu.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"
#include "data/rh_randomizer_tables.h"
#include "data/rh_names.h"

enum Species RH_SafariSpecies(enum Species species, enum WildPokemonArea area, bool32 *handled);

// ---------------------------------------------------------------------------
// Starters
// ---------------------------------------------------------------------------
// FireRed's three balls: index 0 = Bulbasaur, 1 = Charmander, 2 = Squirtle (the "Starter 1-3" order of the menu).
static const u16 sVanillaStarters[3] = { SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE };

struct StarterCache { u32 key; u16 species[3]; };
static EWRAM_DATA struct StarterCache sStarters = {0};

static u32 StarterKey(void)
{
    return (RH_SettingsHash() + 4) | 1;   // every setting (0 = never valid)
}

static bool32 Beats(u8 a, u8 b)
{
    return GetTypeModifier(a, b) > UQ_4_12(1.0);
}

// Picks 3 slot types for "Any Type Triangle": slot0 beats slot2 (Squirtle's slot), slot2 beats slot1, slot1 beats slot0,
// mirroring Grass > Water > Fire > Grass.
static bool32 PickTriangle(u8 *types)
{
    u32 count = 0, target = 0, pass, a, b, c;
    for (pass = 0; pass < 2; pass++)
    {
        if (pass == 1)
        {
            if (count == 0)
                return FALSE;
            target = RH_Hash(SALT_STARTER_TYPE, 0, 0) % count;
            count = 0;
        }
        for (a = 0; a < 18; a++)
            for (b = 0; b < 18; b++)
                for (c = 0; c < 18; c++)
                {
                    if (a == b || b == c || a == c)
                        continue;
                    if (!Beats(gRhMonTypes[a], gRhMonTypes[c]) || !Beats(gRhMonTypes[c], gRhMonTypes[b]) || !Beats(gRhMonTypes[b], gRhMonTypes[a]))
                        continue;
                    if (pass == 1 && count == target)
                    {
                        types[0] = gRhMonTypes[a];
                        types[1] = gRhMonTypes[b];
                        types[2] = gRhMonTypes[c];
                        return TRUE;
                    }
                    count++;
                }
    }
    return FALSE;
}

// Context for the starter filter callback.
static EWRAM_DATA u8 sStarterSlotTypes[3] = {0};
static EWRAM_DATA u8 sStarterSlot = 0;

static u32 ChainNow(u16 species)
{
    // stages still ahead with the current (possibly randomized) evolutions
    u32 n = 0;
    while (n < 3 && (species = RH_EvolveOnce(species, 0)) != SPECIES_NONE)
        n++;
    return n;
}

static bool32 StarterExtraOk(u16 species)
{
    u32 k;
    // Fire/Water/Grass and Type Triangle: the other type may not be another slot's type (FVX)
    if (S->starterTypes == 1 || S->starterTypes == 2)
    {
        for (k = 0; k < 3; k++)
        {
            if (k == sStarterSlot)
                continue;
            if (RH_SpeciesHasType(species, sStarterSlotTypes[k]))
                return FALSE;
        }
    }
    return TRUE;
}

static u32 CountTypeCandidates(u8 type, const struct RhFilter *base)
{
    struct RhFilter f = *base;
    u32 i, n = 0;
    f.type = type;
    for (i = 0; i < RH_PoolCount() && n < 3; i++)
        n += RH_FilterAccepts(&f, i);
    return n;
}

static void BuildStarters(void)
{
    u32 i;
    struct RhFilter base = {0};
    u8 slotTypes[3] = { TYPE_NONE, TYPE_NONE, TYPE_NONE };
    sStarters.key = StarterKey();

    if (S->starters == 3)
    {
        base.threeStageBasic = (S->evolutions == 0);
        base.stage = S->evolutions ? 1 : 0;
    }
    if (S->starters == 4)
        base.stage = 1;                                      // Random (basic): not evolved from anything
    base.allowVariants = S->starterAllowAltFormes;
    if (S->starters != 1)                                    // blank Custom slots are plain random picks (FVX)
    {
        if (S->starterNoLegends)
            base.legend = 1;
        if (S->starterBstMinOn)
            base.minBst = S->starterBstMin;
        if (S->starterBstMaxOn)
            base.maxBst = S->starterBstMax;
        base.monoType = S->starterNoDualTypes;
    }

    switch (S->starters == 1 ? 0 : S->starterTypes)
    {
    case 1:
        slotTypes[0] = TYPE_GRASS;
        slotTypes[1] = TYPE_FIRE;
        slotTypes[2] = TYPE_WATER;
        break;
    case 2:
        PickTriangle(slotTypes);
        break;
    case 4:
        slotTypes[0] = S->starterSingleType;
        if (slotTypes[0] == TYPE_NONE || slotTypes[0] == 0)
        {
            // random single type: only types with at least 3 possible starters
            u32 k;
            for (k = 0; k < 18; k++)
            {
                u8 type = gRhMonTypes[(RH_Hash(SALT_STARTER_TYPE, 1, 0) + k) % 18];
                if (CountTypeCandidates(type, &base) >= 3)
                {
                    slotTypes[0] = type;
                    break;
                }
            }
            if (k == 18)
                slotTypes[0] = TYPE_NONE;
        }
        slotTypes[1] = slotTypes[2] = slotTypes[0];
        break;
    }
    memcpy(sStarterSlotTypes, slotTypes, 3);

    for (i = 0; i < 3; i++)
    {
        struct RhFilter f = base;
        u32 j;
        if (S->starters == 1 && S->customStarters[i] != SPECIES_NONE)
        {
            sStarters.species[i] = S->customStarters[i];
            continue;
        }
        for (j = 0; j < i; j++)
            RH_FilterExclude(&f, sStarters.species[j]);
        if (S->starters != 1)
            f.type = slotTypes[i];
        sStarterSlot = i;
        if (S->starters != 1)
            f.extra = StarterExtraOk;
        sStarters.species[i] = SPECIES_NONE;
        // "2 Evolutions" with Random evolutions: judged with this run's evolutions (a few tries, then any basic)
        if (S->starters == 3 && S->evolutions == 1)
        {
            u32 tries;
            for (tries = 0; tries < 16 && sStarters.species[i] == SPECIES_NONE; tries++)
            {
                struct RhFilter g = f;
                u16 sp = RH_PickSpecies(&g, RH_Hash(SALT_STARTER, i, 100 + tries), SPECIES_NONE);
                bool32 clash = FALSE;
                for (j = 0; j < i && S->starterTypes == 3; j++)
                {
                    u16 o = sStarters.species[j];
                    if (RH_SpeciesHasType(sp, GetSpeciesType(o, 0)) || RH_SpeciesHasType(sp, GetSpeciesType(o, 1)))
                        clash = TRUE;
                }
                if (sp != SPECIES_NONE && !clash && ChainNow(sp) == 2)
                    sStarters.species[i] = sp;
            }
        }
        if (S->starterTypes == 3 && i > 0 && S->starters != 1)   // Unique: no type shared with the starters already chosen
        {
            u32 tries;
            for (tries = 0; tries < 64; tries++)
            {
                struct RhFilter g = f;
                u16 sp = RH_PickSpecies(&g, RH_Hash(SALT_STARTER, i, tries), SPECIES_NONE);
                bool32 clash = FALSE;
                for (j = 0; j < i; j++)
                {
                    u16 o = sStarters.species[j];
                    if (RH_SpeciesHasType(sp, GetSpeciesType(o, 0)) || RH_SpeciesHasType(sp, GetSpeciesType(o, 1)))
                        clash = TRUE;
                }
                if (!clash)
                {
                    sStarters.species[i] = sp;
                    break;
                }
            }
        }
        if (sStarters.species[i] == SPECIES_NONE)
            sStarters.species[i] = RH_PickSpecies(&f, RH_Hash(SALT_STARTER, i, 0), SPECIES_NONE);
        if (sStarters.species[i] == SPECIES_NONE)
            sStarters.species[i] = sVanillaStarters[i];
    }
}

static bool32 StartersChanged(void)
{
    return S->enabled && S->starters != 0;
}

u16 RH_StarterForSlot(u32 slot)
{
    if (!StartersChanged())
        return sVanillaStarters[slot];
    if (sStarters.key != StarterKey() || sStarters.species[0] == SPECIES_NONE)
        BuildStarters();
    return sStarters.species[slot];
}

// Which starter slot a vanilla starter-line species belongs to (-1 if none); *stage = 0/1/2.
s32 RH_StarterFamily(u16 species, u32 *stage)
{
    static const u16 sLines[3][3] = {
        { SPECIES_BULBASAUR, SPECIES_IVYSAUR, SPECIES_VENUSAUR },
        { SPECIES_CHARMANDER, SPECIES_CHARMELEON, SPECIES_CHARIZARD },
        { SPECIES_SQUIRTLE, SPECIES_WARTORTLE, SPECIES_BLASTOISE },
    };
    u32 i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (sLines[i][j] == species)
            {
                *stage = j;
                return i;
            }
    return -1;
}

enum Species RH_StarterSpecies(enum Species vanilla)
{
    u32 stage;
    s32 slot = RH_StarterFamily(vanilla, &stage);
    if (!StartersChanged() || slot < 0 || stage != 0)
        return vanilla;
    return RH_StarterForSlot(slot);
}

// Oak's lab: VAR_TEMP_2 = player's starter species, VAR_TEMP_3 = rival's.
void RH_RemapStarterVars(void)
{
    VarSet(VAR_TEMP_2, RH_StarterSpecies(VarGet(VAR_TEMP_2)));
    VarSet(VAR_TEMP_3, RH_StarterSpecies(VarGet(VAR_TEMP_3)));
}

u16 RH_StartersRandomized(void)
{
    return StartersChanged();
}

static bool32 InOaksLab(void)
{
    return gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB)
        && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB);
}

// Gen 3 rule: all three starters hold the same random item.
enum Item RH_StarterHeldItem(enum Item vanilla)
{
    if (!S->enabled || !S->starterHeldItems || !InOaksLab())
        return vanilla;
    return RH_RandomHeldItem(RH_Hash(SALT_STARTER_ITEM, 0, 0), S->starterBanBadItems, FALSE);
}

// ---------------------------------------------------------------------------
// Static Pokemon: every static encounter / gift (per map, see sRhStaticEncounters) gets its own replacement and
// no replacement is used twice (FVX).
// ---------------------------------------------------------------------------
static bool32 IsStaticSpecies(u16 species)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhStaticSpecies); i++)
        if (sRhStaticSpecies[i] == species)
            return TRUE;
    return species == SPECIES_RAIKOU || species == SPECIES_ENTEI || species == SPECIES_SUICUNE;
}

bool32 RH_IsStaticGift(u16 species)
{
    return IsStaticSpecies(species) && !InOaksLab();
}

// Misc. tweak "Balance Static Pokemon Levels" (FVX: fossils in FRLG).
u8 RH_BalanceStaticLevel(u16 species, u8 level)
{
    if (!S->balanceStaticLevels)
        return level;
    if (species == SPECIES_OMANYTE || species == SPECIES_KABUTO || species == SPECIES_AERODACTYL)
        return max(level, 30);
    return level;
}

struct StaticCache { u32 key; u16 result[RH_STATIC_ENCOUNTER_COUNT]; };
static EWRAM_DATA struct StaticCache sStatics = {0};
static EWRAM_DATA u8 sStaticsBuilt = 0;                        // how many results are final (for the no-repeat filter)
static EWRAM_DATA u16 sLastStaticOriginal = 0;                 // for "Fix Music"

static u32 StaticKey(void)
{
    return (RH_SettingsHash() + 5) | 1;   // every setting (0 = never valid)
}

static bool32 StaticNotUsedYet(u16 species)
{
    u32 i;
    for (i = 0; i < sStaticsBuilt; i++)
        if (sStatics.result[i] == species)
            return FALSE;
    return TRUE;
}

static void BuildStatics(void)
{
    u32 i;
    for (i = 0; i < RH_STATIC_ENCOUNTER_COUNT; i++)
    {
        u16 species = sRhStaticEncounters[i].species;
        u16 similar = SPECIES_NONE, result;
        struct RhFilter f = {0};
        sStaticsBuilt = i;
        f.extra = StaticNotUsedYet;
        f.allowVariants = S->staticAllowAltFormes;
        switch (S->statics)
        {
        case 1:                                              // Swap Legendaries & Swap Standards
            f.legend = RH_IsLegendary(species) ? 2 : 1;
            break;
        case 3:                                              // Similar Strength
            if (!(S->staticRandomize600 && RH_VanillaBST(species) >= 600))
                similar = species;                           // "Randomize 600+ BST": those get a purely random pick
            break;
        }
        result = RH_PickSpecies(&f, RH_Hash(SALT_STATIC_SLOT, i, 0), similar);
        sStatics.result[i] = result ? result : species;
    }
    sStaticsBuilt = RH_STATIC_ENCOUNTER_COUNT;
    sStatics.key = StaticKey();
}

static u16 StaticResult(u32 index)
{
    if (sStatics.key != StaticKey() || sStaticsBuilt != RH_STATIC_ENCOUNTER_COUNT)
        BuildStatics();
    return sStatics.result[index];
}

static bool32 StaticsActive(void)
{
    return S->enabled && S->statics != 0;
}

enum Species RH_StaticSpecies(enum Species species)
{
    u32 i;
    u16 mapKey = (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
    if (!StaticsActive() || InOaksLab())
        return species;
    for (i = 0; i < RH_STATIC_ENCOUNTER_COUNT; i++)
    {
        if (sRhStaticEncounters[i].map == mapKey && sRhStaticEncounters[i].species == species)
        {
            sLastStaticOriginal = species;
            return StaticResult(i);
        }
    }
    return species;
}

enum Species RH_RoamerSpecies(enum Species species)
{
    u32 i;
    if (!StaticsActive())
        return species;
    for (i = 0; i < RH_STATIC_ENCOUNTER_COUNT; i++)
        if (sRhStaticEncounters[i].map == RH_STATIC_MAP_ROAMER && sRhStaticEncounters[i].species == species)
            return StaticResult(i);
    return species;
}

// Script special: VAR_TEMP_1 = a gift's original species -> VAR_RESULT = its replacement, name in STR_VAR_1.
void RH_BufferStaticSpecies(void)
{
    gSpecialVar_Result = RH_StaticSpecies(VarGet(VAR_TEMP_1));
    StringCopy(gStringVar1, GetSpeciesName(gSpecialVar_Result));
}

// Game Corner prize list (multichoice) with the replacement names. "texts" is the vanilla list.
static EWRAM_DATA u8 sPrizeText[5][POKEMON_NAME_LENGTH + 24] = {0};
static EWRAM_DATA struct MenuAction sPrizeList[6] = {0};

const struct MenuAction *RH_GameCornerPrizeList(const struct MenuAction *vanilla, u32 count)
{
#if defined(FIRERED)
    static const u16 sPrizes[] = { SPECIES_ABRA, SPECIES_CLEFAIRY, SPECIES_DRATINI, SPECIES_SCYTHER, SPECIES_PORYGON };
#else
    static const u16 sPrizes[] = { SPECIES_ABRA, SPECIES_CLEFAIRY, SPECIES_PINSIR, SPECIES_DRATINI, SPECIES_PORYGON };
#endif
    u32 i;
    if (!StaticsActive() || count > ARRAY_COUNT(sPrizeList))
        return vanilla;
    for (i = 0; i < count; i++)
    {
        const u8 *rest = vanilla[i].text;
        sPrizeList[i] = vanilla[i];
        if (i >= ARRAY_COUNT(sPrizes))
            continue;
        while (*rest != EOS && *rest != EXT_CTRL_CODE_BEGIN)
            rest++;
        StringCopy(StringCopy(sPrizeText[i], GetSpeciesName(RH_StaticSpecies(sPrizes[i]))), rest);
        sPrizeList[i].text = sPrizeText[i];
    }
    return sPrizeList;
}

// "Fix Music" (FVX): encounters that have special music keep it after randomizing. Without it, the special theme
// follows the new Pokemon (a regular Pokemon gets the regular wild theme).
bool32 RH_StaticUsesLegendMusic(u16 newSpecies, bool32 vanillaLegendMusic)
{
    if (!StaticsActive() || S->staticFixMusic)
        return vanillaLegendMusic;
    return RH_IsLegendary(newSpecies);
}

// Species whose music plays for a legendary battle: with Fix Music, the original one (VAR_0x8004 in FRLG scripts).
u16 RH_LegendMusicSpecies(u16 newSpecies)
{
    u16 original = sLastStaticOriginal;
    sLastStaticOriginal = SPECIES_NONE;                      // used once, so a later battle can't pick up a stale one
    if (StaticsActive() && S->staticFixMusic && original != SPECIES_NONE)
        return original;
    return newSpecies;
}

u8 RH_StaticLevel(u8 level)
{
    s32 l = level;
    if (!S->enabled)
        return level;
    if (S->staticLevelModOn)                                 // FVX: works even with statics unchanged
        l = RH_ApplyPercent(l, S->staticLevelMod);
    return min(max(l, 1), MAX_LEVEL);
}

// ---------------------------------------------------------------------------
// In-game trades
// ---------------------------------------------------------------------------
#define MAX_RH_TRADES 16
struct TradeCache { u32 key; u16 given[MAX_RH_TRADES]; u16 requested[MAX_RH_TRADES]; };
static EWRAM_DATA struct TradeCache sTrades = {0};
static EWRAM_DATA u8 sTradesBuilt = 0;

static u32 TradeKey(void)
{
    return (RH_SettingsHash() + 6) | 1;   // every setting (0 = never valid)
}

static bool32 TradeSpeciesUnused(u16 species)
{
    u32 i;
    for (i = 0; i < sTradesBuilt; i++)
        if (sTrades.given[i] == species || sTrades.requested[i] == species)
            return FALSE;
    return TRUE;
}

// FVX: every trade gives (and, with "Both", asks for) a different random Pokemon, never the same one twice.
static void BuildTrades(void)
{
    u32 i, n = min(RH_InGameTradeCount(), MAX_RH_TRADES);
    for (i = 0; i < n; i++)
    {
        u16 given, requested;
        struct RhFilter f = {0};
        RH_InGameTradeSpecies(i, &given, &requested);
        sTradesBuilt = i;
        f.extra = TradeSpeciesUnused;
        if (S->trades >= 2)
        {
            u16 r = RH_PickSpecies(&f, RH_Hash(SALT_TRADE, i, 1), SPECIES_NONE);
            if (r != SPECIES_NONE)
                requested = r;
            f.extra = TradeSpeciesUnused;
            f.excludeCount = 0;
        }
        sTrades.requested[i] = requested;
        RH_FilterExclude(&f, requested);
        {
            u16 g = RH_PickSpecies(&f, RH_Hash(SALT_TRADE, i, 0), SPECIES_NONE);
            if (g != SPECIES_NONE)
                given = g;
        }
        sTrades.given[i] = given;
    }
    sTradesBuilt = n;
    sTrades.key = TradeKey();
}

enum Species RH_TradeSpecies(u32 tradeId, enum Species species, bool32 requested)
{
    if (!S->enabled || S->trades == 0 || (requested && S->trades < 2) || tradeId >= MAX_RH_TRADES || tradeId >= RH_InGameTradeCount())
        return species;
    if (sTrades.key != TradeKey() || sTradesBuilt == 0)
        BuildTrades();
    return requested ? sTrades.requested[tradeId] : sTrades.given[tradeId];
}

void RH_ModifyTradeMon(struct Pokemon *mon, u32 tradeId)
{
    u32 i;
    if (!S->enabled || S->trades == 0)
        return;
    if (S->tradeNicknames)
    {
        u8 name[POKEMON_NAME_LENGTH + 1];
        // a different nickname for every trade (a keyed permutation of the list)
        StringCopy(name, sRhNicknames[RH_Permute(SALT_NICKNAME, tradeId, ARRAY_COUNT(sRhNicknames))]);
        SetMonData(mon, MON_DATA_NICKNAME, name);
    }
    if (S->tradeOTs)
    {
        u8 name[PLAYER_NAME_LENGTH + 1];
        u32 otId = RH_Hash(SALT_OT, tradeId, 1) & 0xFFFF;    // FVX: a random 16-bit trainer ID
        u32 tries;
        for (tries = 0; tries < 16; tries++)
        {
            const u8 *n = sRhTrainerNames[RH_Hash(SALT_OT, tradeId, tries + 2) % ARRAY_COUNT(sRhTrainerNames)];
            if (StringLength(n) <= PLAYER_NAME_LENGTH)
            {
                StringCopy(name, n);
                break;
            }
        }
        if (tries == 16)
            StringCopy(name, COMPOUND_STRING("Joe"));
        SetMonData(mon, MON_DATA_OT_NAME, name);
        SetMonData(mon, MON_DATA_OT_ID, &otId);
    }
    if (S->tradeIVs)
    {
        for (i = 0; i < NUM_STATS; i++)
        {
            u32 iv = RH_Hash(SALT_IV, tradeId, i) % (MAX_PER_STAT_IVS + 1);
            SetMonData(mon, MON_DATA_HP_IV + i, &iv);
        }
    }
    if (S->tradeItems)
    {
        u16 item = RH_RandomHeldItem(RH_Hash(SALT_HELD_ITEM, 0x7700 + tradeId, 0), FALSE, FALSE);
        u8 noMail = MAIL_NONE;
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
        SetMonData(mon, MON_DATA_MAIL, &noMail);
    }
}

// ---------------------------------------------------------------------------
// Wild Pokemon
// ---------------------------------------------------------------------------
enum { ZONE_GLOBAL, ZONE_LOCATION, ZONE_SET, ZONE_MAX, ZONE_ALWAYS, ZONE_MAP };

static u8 AreaGroup(enum WildPokemonArea area)
{
    return area;                      // land / water / rocks / fishing / hidden
}

static u32 AreaSlotCount(enum WildPokemonArea area)
{
    static const u8 sSlots[] = { [WILD_AREA_LAND] = 12, [WILD_AREA_WATER] = 5, [WILD_AREA_ROCKS] = 5, [WILD_AREA_FISHING] = 10, [WILD_AREA_HIDDEN] = 3 };
    return (area < ARRAY_COUNT(sSlots)) ? sSlots[area] : 0;
}

// A type shared by every Pokemon of the area (vanilla types), or TYPE_NONE. Used for maps outside the pair table.
static u8 AreaThemeType(const struct WildPokemonInfo *info, enum WildPokemonArea area)
{
    u32 t, i, n = AreaSlotCount(area);
    for (t = 0; t < 2; t++)
    {
        u8 type = gSpeciesInfo[info->wildPokemon[0].species].types[t];
        for (i = 1; i < n; i++)
        {
            const struct SpeciesInfo *si = &gSpeciesInfo[info->wildPokemon[i].species];
            if (si->types[0] != type && si->types[1] != type)
                break;
        }
        if (n > 1 && i == n)
            return type;
    }
    return TYPE_NONE;
}

static u32 LowestLevelInSet(const struct WildPokemonInfo *info, enum WildPokemonArea area, u16 species)
{
    u32 i, low = MAX_LEVEL, n = AreaSlotCount(area);
    for (i = 0; i < n; i++)
        if (info->wildPokemon[i].species == species)
            low = min(low, (info->wildPokemon[i].minLevel + info->wildPokemon[i].maxLevel) / 2);
    return low;
}

// Zone of a wild slot: every (zone, species) gets one fixed replacement.
static u32 ZoneOf(u16 mapKey, u32 mapsec, u32 area)
{
    bool32 split = S->wildSplitEncounterTypes;
    switch (S->wildZone)
    {
    case ZONE_GLOBAL:   return split ? AreaGroup(area) + 1 : 0;
    case ZONE_LOCATION: return 0x10000 + mapsec + (split ? (AreaGroup(area) + 1) * 0x100 : 0);
    case ZONE_MAP:      return 0x20000 + mapKey * 8 + (split ? AreaGroup(area) + 1 : 0);
    default:            return 0x40000 + mapKey * 8 + area;  // encounter set (and "Maximum Possible")
    }
}

static u32 PairZone(u32 p)
{
    const struct RhWildPair *q = &sRhWildPairs[p];
    return ZoneOf(q->map, q->mapsec, q->area);
}

static s32 FindPair(u16 mapKey, u32 area, u16 species)
{
    u32 p;
    if (area > 3)
        return -1;
    for (p = 0; p < RH_WILD_PAIR_COUNT; p++)
        if (sRhWildPairs[p].map == mapKey && sRhWildPairs[p].species == species && sRhWildPairs[p].area == area)
            return p;
    return -1;
}

// "Keep Set/Zone Themes": a type shared by every Pokemon of the zone (vanilla types), or TYPE_NONE.
static u8 ZoneThemeType(u32 zone)
{
    u32 p;
    u8 a = TYPE_NONE, b = TYPE_NONE;
    bool32 first = TRUE;
    for (p = 0; p < RH_WILD_PAIR_COUNT; p++)
    {
        const struct SpeciesInfo *si;
        if (PairZone(p) != zone)
            continue;
        si = &gSpeciesInfo[sRhWildPairs[p].species];
        if (first)
        {
            a = si->types[0];
            b = si->types[1];
            first = FALSE;
            continue;
        }
        if (a != si->types[0] && a != si->types[1])
            a = TYPE_NONE;
        if (b != si->types[0] && b != si->types[1])
            b = TYPE_NONE;
        if (a == TYPE_NONE && b == TYPE_NONE)
            return TYPE_NONE;
    }
    return a != TYPE_NONE ? a : b;
}

// "Balance Low Level": the species' lowest (average) level anywhere in its zone, after the level modifier.
static u32 ZoneLowLevel(u32 zone, u16 species)
{
    u32 p, low = MAX_LEVEL;
    for (p = 0; p < RH_WILD_PAIR_COUNT; p++)
        if (sRhWildPairs[p].species == species && sRhWildPairs[p].low < low && PairZone(p) == zone)
            low = sRhWildPairs[p].low;
    return RH_ModifyWildLevel(low);
}

static EWRAM_DATA u8 sRelationStage = 0;
static bool32 ChainLongEnough(u16 species)
{
    return RH_SpeciesChain(species) >= sRelationStage;
}

// Catch Em' All: a dense rank per (zone, species) -> the rank-th allowed Pokemon in a keyed order, so different
// ranks never share a Pokemon until every allowed Pokemon has been used once.
static u16 CatchAllPick(u32 rank, const struct RhFilter *f)
{
    u32 n = RH_PoolCount(), accepted = 0, k, p;
    for (p = 0; p < n; p++)
        accepted += RH_FilterAccepts(f, p);
    if (accepted == 0)
        return SPECIES_NONE;
    k = RH_Permute(SALT_CATCH_ALL, rank % accepted, accepted);
    for (p = 0; p < n; p++)
    {
        if (RH_FilterAccepts(f, p))
        {
            if (k == 0)
                return RH_PoolSpecies(p);
            k--;
        }
    }
    return SPECIES_NONE;
}

static u32 CatchAllRankMode(void)
{
    bool32 split = S->wildSplitEncounterTypes;
    switch (S->wildZone)
    {
    case ZONE_GLOBAL:   return split ? 1 : 0;
    case ZONE_LOCATION: return split ? 3 : 2;
    case ZONE_MAP:      return split ? 5 : 4;
    default:            return 6;
    }
}

static bool32 UsesMapping(void)
{
    return S->wildZone != ZONE_MAX && S->wildZone != ZONE_ALWAYS;
}

// Results per (map, area, species) pair, so a map's encounters are only worked out once per boot.
struct WildCache { u32 key; u16 result[RH_WILD_PAIR_COUNT]; };
static EWRAM_DATA struct WildCache sWildCache = {0};

static enum Species PickWild(u16 mapKey, u32 mapsec, s32 pair, const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area)
{
    struct RhFilter f = {0};
    enum Species species = info->wildPokemon[slot].species;
    u32 zone = ZoneOf(mapKey, mapsec, area), themeZone = zone, speciesKey = species, hash, bst = 0;
    u16 result;
    bool32 relations;
    u8 keptTheme = TYPE_NONE;

    if (S->wildZone == ZONE_MAX)
        speciesKey = 0x8000 + slot;
    else if (S->wildZone == ZONE_ALWAYS)
    {
        zone = Random32();
        speciesKey = Random32();
        themeZone = 0x40000 + mapKey * 8 + area;             // no zones: themes per encounter set (FVX)
    }

    f.allowVariants = S->wildAllowAltFormes;
    if (S->wildNoLegends)
        f.legend = 1;
    if (S->wildKeepThemes)                                   // "Keep Set/Zone Themes" wins over the other type rules
        keptTheme = (pair >= 0 && S->wildZone != ZONE_ALWAYS) ? ZoneThemeType(themeZone) : AreaThemeType(info, area);
    if (keptTheme != TYPE_NONE)
    {
        f.type = keptTheme;
    }
    else
    {
        switch (S->wildTypeRestriction)
        {
        case 1:
            f.type = RH_RandomMonType(RH_Hash(SALT_WILD_THEME, themeZone, 0));
            break;
        case 2:
            f.primaryType = gSpeciesInfo[species].types[0];
            break;
        }
    }
    switch (S->wildEvoRestriction)
    {
    case 1:
        f.stage = 1;
        break;
    case 2:
        f.stage = RH_SpeciesStage(species);
        break;
    }
    relations = S->wildKeepRelations && UsesMapping() && S->wildEvoRestriction != 2;
    if (S->wildSimilarStrength)
    {
        bst = RH_VanillaBST(species);
        if (S->wildBalanceLowLevel)                          // FVX: no Geodude at level 2
        {
            u32 low = (pair >= 0 && UsesMapping()) ? ZoneLowLevel(zone, species) : RH_ModifyWildLevel(LowestLevelInSet(info, area, species));
            bst = min(bst, low * 10 + 250);
        }
    }
    hash = RH_Hash(SALT_WILD, zone, speciesKey);
    if (S->wildMegas && (RH_Hash(SALT_WILD_AREA, zone, speciesKey) % 100) < 3)
    {
        f.allowMegas = TRUE;
        f.megasOnly = TRUE;
    }

    if (relations && !f.megasOnly)
    {
        // Related Pokemon stay related: the family gets one random basic Pokemon, evolved to the original's stage.
        u32 stage = RH_SpeciesStage(species);
        struct RhFilter g = f;
        g.stage = 1;
        sRelationStage = stage;
        g.extra = ChainLongEnough;
        result = RH_PickSpeciesNearBst(&g, RH_Hash(SALT_WILD, zone, RH_FamilyRoot(species)), bst ? RH_VanillaBST(RH_FamilyRoot(species)) : 0);
        if (result != SPECIES_NONE)
            return RH_EvolveTimes(result, stage - 1);
    }
    if (S->wildCatchEmAll && UsesMapping() && !f.megasOnly && pair >= 0)
    {
        result = CatchAllPick(sRhWildRank[pair][CatchAllRankMode()], &f);   // catch 'em all wins over similar strength (FVX)
        if (result != SPECIES_NONE)
            return result;
    }
    result = RH_PickSpeciesNearBst(&f, hash, bst);
    return result ? result : species;
}

// The randomized species for a wild slot of any map (used for encounters and for "Use Local Pokemon").
enum Species RH_WildSpeciesAt(u8 mapGroup, u8 mapNum, const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level)
{
    enum Species species = info->wildPokemon[slot].species;
    u16 mapKey = (mapGroup << 8) | mapNum;
    u32 mapsec, key;
    s32 pair;
    enum Species result;

    if (!S->enabled || !S->wild || species == SPECIES_NONE)
        return species;
    mapsec = Overworld_GetMapHeaderByGroupAndId(mapGroup, mapNum)->regionMapSectionId;
    pair = FindPair(mapKey, area, species);
    if (pair < 0 || !UsesMapping())
        return PickWild(mapKey, mapsec, pair, info, slot, area);
    key = RH_SettingsHash() | 1;
    if (sWildCache.key != key)
    {
        memset(sWildCache.result, 0, sizeof(sWildCache.result));
        sWildCache.key = key;
    }
    if (sWildCache.result[pair] == SPECIES_NONE)
        sWildCache.result[pair] = PickWild(mapKey, mapsec, pair, info, slot, area);
    result = sWildCache.result[pair];
    return result;
}

enum Species RH_ModifyWildSpecies(const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level)
{
    enum Species species = info->wildPokemon[slot].species;
    bool32 handled;
    enum Species safari = RH_SafariSpecies(species, area, &handled);
    if (handled)
        return safari;                                    // the Safari generation setting wins
    return RH_WildSpeciesAt(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, info, slot, area, level);
}

u8 RH_ModifyWildLevel(u8 level)
{
    s32 l;
    if (!S->enabled || !S->wildLevelModOn)
        return level;
    l = RH_ApplyPercent(level, S->wildLevelMod);
    return min(max(l, 1), MAX_LEVEL);
}

// FVX: species that held nothing keep holding nothing; the others get 50% none, 15% common only, 15% rare only,
// 15% common + rare, 5% always holding the item; species that always held their item keep that 90% of the time.
enum Item RH_WildHeldItem(enum Species species, bool32 rare, enum Item vanilla)
{
    const struct SpeciesInfo *info;
    u32 d;
    u16 a, b;
    if (!S->enabled || !S->wildHeldItems || species == SPECIES_NONE || species >= NUM_SPECIES)
        return vanilla;
    info = &gSpeciesInfo[species];
    if (info->itemCommon == ITEM_NONE && info->itemRare == ITEM_NONE)
        return ITEM_NONE;
    a = RH_RandomHeldItem(RH_Hash(SALT_WILD_ITEM, species, 0), S->wildBanBadItems, FALSE);
    b = RH_RandomHeldItem(RH_Hash(SALT_WILD_ITEM, species, 1), S->wildBanBadItems, FALSE);
    if (a == b)
        b = RH_RandomHeldItem(RH_Hash(SALT_WILD_ITEM, species, 2), S->wildBanBadItems, FALSE);
    d = RH_Hash(SALT_WILD_ITEM, species, 99) % 100;
    if (info->itemCommon != ITEM_NONE && info->itemCommon == info->itemRare && d < 90)
        return a;                                            // guaranteed item (common == rare)
    if (d < 50)
        return ITEM_NONE;
    if (d < 65)
        return rare ? ITEM_NONE : a;                         // common only
    if (d < 80)
        return rare ? b : ITEM_NONE;                         // rare only
    if (d < 95)
        return rare ? b : a;                                 // both
    return a;                                                // always holding it (common == rare)
}
