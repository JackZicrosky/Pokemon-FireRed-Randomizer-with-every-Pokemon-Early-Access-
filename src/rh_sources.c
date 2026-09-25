// Where Pokemon come from: starters, static encounters, in-game trades and wild encounters.
#include "global.h"
#include "constants/characters.h"
#include "battle_util.h"
#include "event_data.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "string_util.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"
#include "data/rh_randomizer_tables.h"
#include "data/rh_names.h"

enum Species RH_SafariSpecies(enum Species species, enum WildPokemonArea area);

// ---------------------------------------------------------------------------
// Starters
// ---------------------------------------------------------------------------
// FireRed's three balls: index 0 = Bulbasaur, 1 = Charmander, 2 = Squirtle (the "Starter 1-3" order of the menu).
static const u16 sVanillaStarters[3] = { SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE };

struct StarterCache { u32 key; u16 species[3]; };
static EWRAM_DATA struct StarterCache sStarters = {0};

static u32 StarterKey(void)
{
    return S->seed ^ (S->starters << 1) ^ (S->starterNoLegends << 4) ^ (S->starterBstMinOn << 5) ^ (S->starterBstMaxOn << 6)
         ^ (S->starterBstMin << 7) ^ (S->starterBstMax << 17) ^ (S->starterTypes << 27) ^ (S->starterNoDualTypes << 30)
         ^ (S->starterSingleType * 0x10001) ^ (S->customStarters[0] * 3) ^ (S->customStarters[1] * 5)
         ^ (S->customStarters[2] * 7) ^ (S->speciesPool << 12) ^ (S->types << 14);
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

static void BuildStarters(void)
{
    u32 i;
    u8 slotTypes[3] = { TYPE_NONE, TYPE_NONE, TYPE_NONE };
    sStarters.key = StarterKey();
    if (S->starters == 1)
    {
        for (i = 0; i < 3; i++)
            sStarters.species[i] = S->customStarters[i] ? S->customStarters[i] : sVanillaStarters[i];
        return;
    }
    switch (S->starterTypes)
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
        slotTypes[0] = S->starterSingleType ? S->starterSingleType : RH_RandomMonType(RH_Hash(SALT_STARTER_TYPE, 1, 0));
        slotTypes[1] = slotTypes[2] = slotTypes[0];
        break;
    }
    for (i = 0; i < 3; i++)
    {
        struct RhFilter f = {0};
        u32 j;
        for (j = 0; j < i; j++)
            RH_FilterExclude(&f, sStarters.species[j]);
        if (S->starters == 3)
            f.threeStageBasic = TRUE;
        if (S->starters == 4)
            f.allowVariants = TRUE;
        if (S->starterNoLegends)
            f.legend = 1;
        if (S->starterBstMinOn)
            f.minBst = S->starterBstMin;
        if (S->starterBstMaxOn)
            f.maxBst = S->starterBstMax;
        f.type = slotTypes[i];
        f.monoType = S->starterNoDualTypes;
        sStarters.species[i] = SPECIES_NONE;
        if (S->starterTypes == 3 && i > 0)              // Unique: no type shared with the starters already chosen
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
// Static Pokemon
// ---------------------------------------------------------------------------
static bool32 IsStaticSpecies(u16 species)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhStaticSpecies); i++)
        if (sRhStaticSpecies[i] == species)
            return TRUE;
    return species == SPECIES_RAIKOU || species == SPECIES_ENTEI || species == SPECIES_SUICUNE;
}

static bool32 IsMainGameLegend(u16 species)
{
    return species == SPECIES_ARTICUNO || species == SPECIES_ZAPDOS || species == SPECIES_MOLTRES;
}

static bool32 CountsAsLegend(u16 species)
{
    return RH_IsLegendary(species) || (S->staticRandomize600 && RH_VanillaBST(species) >= 600);
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


enum Species RH_StaticSpecies(enum Species species)
{
    struct RhFilter f = {0};
    u16 result;
    if (!S->enabled || S->statics == 0 || InOaksLab() || !IsStaticSpecies(species))
        return species;
    switch (S->statics)
    {
    case 1:
        f.legend = CountsAsLegend(species) ? 2 : 1;
        if (S->staticRandomize600 && !RH_IsLegendary(species) && RH_VanillaBST(species) >= 600)
            f.legend = 0, f.minBst = 600;
        break;
    case 3:
        break;
    }
    if (S->staticLimitMainGameLegends && IsMainGameLegend(species))
        f.maxBst = RH_VanillaBST(species);
    result = RH_PickSpecies(&f, RH_Hash(SALT_STATIC, species, 0), S->statics == 3 ? species : SPECIES_NONE);
    return result ? result : species;
}

// "Fix Music": static battles use the legendary theme only when the new Pokemon is legendary.
bool32 RH_StaticUsesLegendMusic(u16 newSpecies, bool32 vanillaLegendMusic)
{
    if (!S->enabled || S->statics == 0 || !S->staticFixMusic)
        return vanillaLegendMusic;
    return RH_IsLegendary(newSpecies);
}

u8 RH_StaticLevel(u8 level)
{
    s32 l = level;
    if (!S->enabled)
        return level;
    if (S->statics && S->staticLevelModOn)
        l = RH_ApplyPercent(l, S->staticLevelMod);
    return min(max(l, 1), MAX_LEVEL);
}

// ---------------------------------------------------------------------------
// In-game trades
// ---------------------------------------------------------------------------
enum Species RH_TradeSpecies(u32 tradeId, enum Species species, bool32 requested)
{
    struct RhFilter f = {0};
    if (!S->enabled || S->trades == 0 || (requested && S->trades < 2))
        return species;
    f.legend = 1;
    return RH_PickSpecies(&f, RH_Hash(SALT_TRADE, tradeId, requested), SPECIES_NONE) ?: species;
}

void RH_ModifyTradeMon(struct Pokemon *mon, u32 tradeId)
{
    u32 i;
    if (!S->enabled || S->trades == 0)
        return;
    if (S->tradeNicknames)
    {
        u8 name[POKEMON_NAME_LENGTH + 1];
        StringCopy(name, sRhNicknames[RH_Hash(SALT_NICKNAME, tradeId, 0) % ARRAY_COUNT(sRhNicknames)]);
        SetMonData(mon, MON_DATA_NICKNAME, name);
    }
    if (S->tradeOTs)
    {
        u8 name[PLAYER_NAME_LENGTH + 1];
        u32 otId = RH_Hash(SALT_OT, tradeId, 1);
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
        u16 item = RH_RandomHeldItem(RH_Hash(SALT_HELD_ITEM, 0x7700 + tradeId, 0), TRUE, FALSE);
        u8 noMail = MAIL_NONE;
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
        SetMonData(mon, MON_DATA_MAIL, &noMail);
    }
}

// ---------------------------------------------------------------------------
// Wild Pokemon
// ---------------------------------------------------------------------------
static u8 AreaGroup(enum WildPokemonArea area)
{
    return area;                      // land / water / rocks / fishing / hidden
}

// A type shared by every Pokemon of the area (vanilla types), or TYPE_NONE.
static u8 AreaThemeType(const struct WildPokemonInfo *info, enum WildPokemonArea area)
{
    static const u8 sSlots[] = { [WILD_AREA_LAND] = 12, [WILD_AREA_WATER] = 5, [WILD_AREA_ROCKS] = 5, [WILD_AREA_FISHING] = 10, [WILD_AREA_HIDDEN] = 3 };
    u32 t, i, n = (area < ARRAY_COUNT(sSlots)) ? sSlots[area] : 0;
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

static EWRAM_DATA u8 sRelationStage = 0;
static bool32 ChainLongEnough(u16 species)
{
    return RH_SpeciesChain(species) >= sRelationStage;
}

// Unique small index for (zone, species) used by Catch Em' All.
static s32 CatchAllIndex(u16 species, enum WildPokemonArea area, u16 mapKey, u32 mapsec)
{
    u32 i;
    switch (S->wildZone)
    {
    case 0:
        return RH_PoolIndexOf(species);
    case 1:
        for (i = 0; i < RH_WILD_PAIR_COUNT; i++)
        {
            const struct RhWildPair *p = &sRhWildPairs[i];
            if (p->species != species || (S->wildSplitEncounterTypes && p->area != (area & 3)))
                continue;
            if (Overworld_GetMapHeaderByGroupAndId(p->map >> 8, p->map & 0xFF)->regionMapSectionId == mapsec)
                return i;
        }
        return -1;
    default:
        for (i = 0; i < RH_WILD_PAIR_COUNT; i++)
            if (sRhWildPairs[i].map == mapKey && sRhWildPairs[i].area == (area & 3) && sRhWildPairs[i].species == species)
                return i;
        return -1;
    }
}

static u16 CatchAllPick(s32 index, const struct RhFilter *f)
{
    u32 x = index, guard;
    for (guard = 0; guard < 4096; guard++)
    {
        x = RH_Permute(SALT_CATCH_ALL, x, RH_PoolCount());
        if (RH_FilterAccepts(f, x))
            return RH_PoolSpecies(x);
    }
    return SPECIES_NONE;
}

// The randomized species for a wild slot of any map (used for encounters and for "Use Local Pokemon").
enum Species RH_WildSpeciesAt(u8 mapGroup, u8 mapNum, const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level)
{
    struct RhFilter f = {0};
    enum Species species = info->wildPokemon[slot].species;
    u16 mapKey = (mapGroup << 8) | mapNum;
    u32 mapsec = Overworld_GetMapHeaderByGroupAndId(mapGroup, mapNum)->regionMapSectionId;
    u32 zone, speciesKey, hash;
    u16 similar = SPECIES_NONE, result;
    bool32 relations = FALSE;

    if (!S->enabled || !S->wild || species == SPECIES_NONE)
        return species;

    speciesKey = species;
    switch (S->wildZone)
    {
    case 0:  zone = 0; break;
    case 1:  zone = mapsec + (S->wildSplitEncounterTypes ? (AreaGroup(area) + 1) * 0x100 : 0); break;
    case 2:  zone = mapKey * 8 + area; break;
    case 3:  zone = mapKey * 8 + area; speciesKey = 0x8000 + slot; break;
    default: zone = Random32(); speciesKey = Random32(); break;
    }

    if (S->wildNoLegends)
        f.legend = 1;
    switch (S->wildTypeRestriction)
    {
    case 0:
        if (S->wildKeepThemes)
            f.type = AreaThemeType(info, area);
        break;
    case 1:
        f.type = RH_RandomMonType(RH_Hash(SALT_WILD_THEME, S->wildZone == 1 ? zone : mapKey * 8 + area, 0));
        break;
    case 2:
        f.primaryType = gSpeciesInfo[species].types[0];
        break;
    }
    switch (S->wildEvoRestriction)
    {
    case 0:
        relations = S->wildKeepRelations && S->wildZone <= 2;
        break;
    case 1:
        f.stage = 1;
        break;
    case 2:
        f.stage = RH_SpeciesStage(species);
        break;
    }
    if (S->wildSimilarStrength)
    {
        similar = species;
        if (S->wildBalanceLowLevel && level < 40)
            f.maxBst = 250 + level * 10;                  // no Mewtwo at level 5
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
        result = RH_PickSpecies(&g, RH_Hash(SALT_WILD, zone, RH_FamilyRoot(species)), similar ? RH_FamilyRoot(species) : SPECIES_NONE);
        if (result != SPECIES_NONE)
            return RH_EvolveTimes(result, stage - 1);
    }
    if (S->wildCatchEmAll && S->wildZone <= 2 && !f.megasOnly)
    {
        s32 idx = CatchAllIndex(species, area, mapKey, mapsec);
        if (idx >= 0)
        {
            result = CatchAllPick(idx, &f);
            if (result != SPECIES_NONE)
                return result;
        }
    }
    result = RH_PickSpecies(&f, hash, similar);
    return result ? result : species;
}

enum Species RH_ModifyWildSpecies(const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level)
{
    enum Species species = info->wildPokemon[slot].species;
    enum Species safari = RH_SafariSpecies(species, area);
    if (safari != species)
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

enum Item RH_WildHeldItem(enum Species species, bool32 rare, enum Item vanilla)
{
    u32 h;
    if (!S->enabled || !S->wildHeldItems)
        return vanilla;
    h = RH_Hash(SALT_WILD_ITEM, species, rare);
    if (rare && (h % 3) != 0)
        return RH_WildHeldItem(species, FALSE, vanilla); // most species hold one item type
    return RH_RandomHeldItem(h, S->wildBanBadItems, FALSE);
}
