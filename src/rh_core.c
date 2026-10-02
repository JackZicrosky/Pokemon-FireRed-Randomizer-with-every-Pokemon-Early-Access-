// Randomizer core: settings, seeded hashing, the species pool and species picking.
#include "global.h"
#include "main.h"
#include "item.h"
#include "mail.h"
#include "pokemon.h"
#include "random.h"
#include "string_util.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/species.h"
#include "constants/hold_effects.h"
#include "data/rh_randomizer_data.h"
#include "data/rh_gen_data.h"

EWRAM_DATA struct RhSettings gRhPendingSettings = {0};

const u8 gRhMonTypes[18] = {
    TYPE_NORMAL, TYPE_FIGHTING, TYPE_FLYING, TYPE_POISON, TYPE_GROUND, TYPE_ROCK, TYPE_BUG, TYPE_GHOST, TYPE_STEEL,
    TYPE_FIRE, TYPE_WATER, TYPE_GRASS, TYPE_ELECTRIC, TYPE_PSYCHIC, TYPE_ICE, TYPE_DRAGON, TYPE_DARK, TYPE_FAIRY,
};

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void RH_SetDefaultSettings(struct RhSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->version = RH_SETTINGS_VERSION;
    s->enabled = TRUE;
    s->mechanicsGen = 9;
    s->speciesPool = RH_POOL_ALL;
    s->baseStatsFollowEvos = TRUE;
    s->abilitiesFollowEvos = TRUE;
    s->banTrapAbilities = TRUE;
    s->banNegativeAbilities = TRUE;
    s->updateTypeChart = TRUE;
    s->starterBstMin = 300;
    s->starterBstMax = 350;
    s->staticLevelMod = 0;
    s->guaranteedLevel1Moves = 4;
    s->movesetNoGameBreaking = TRUE;
    s->movesetGoodDamaging = 50;
    s->noEarlyWonderGuard = TRUE;
    s->trainersEvolveLevel = 55;
    s->wildZone = 2;
    s->wildCatchRate = 1;
    s->wildMegas = TRUE;
    s->tmNoGameBreaking = TRUE;
    s->tmKeepFieldMoves = TRUE;
    s->tmGoodDamaging = 50;
    s->tutorNoGameBreaking = TRUE;
    s->tutorKeepFieldMoves = TRUE;
    s->tutorGoodDamaging = 50;
    s->fieldBanBad = TRUE;
    s->shopBanBad = TRUE;
    s->pickupBanBad = TRUE;
    s->evoMakeEasier = 0;
    s->lowerCaseNames = FALSE;
    s->runWithoutShoes = FALSE;
    s->randomIntroMon = TRUE;
    s->bstChangePct = 20;
    s->bstFollowEvos = TRUE;
    s->statsFollowMegas = TRUE;
    s->typesFollowMegas = TRUE;
    s->abilitiesFollowMegas = TRUE;
}

void RH_ApplyPendingSettings(void)
{
    RH_InvalidateSettingsHash();
    gSaveBlock3Ptr->rhSettings = gRhPendingSettings;
    if (gSaveBlock3Ptr->rhSettings.version != RH_SETTINGS_VERSION)
    {
        // Quickstart / debug new games never saw the settings screen.
        RH_SetDefaultSettings(&gSaveBlock3Ptr->rhSettings);
        gSaveBlock3Ptr->rhSettings.seed = 1;
    }
    if (gSaveBlock3Ptr->rhSettings.seed == 0)
        gSaveBlock3Ptr->rhSettings.seed = 1;
}

// ---------------------------------------------------------------------------
// Hashing / permutations
// ---------------------------------------------------------------------------
static u32 Mix(u32 h)
{
    h ^= h >> 16; h *= 0x7FEB352D;
    h ^= h >> 15; h *= 0x846CA68B;
    h ^= h >> 16;
    return h;
}

// Hash of every setting: cache key for results that depend on many options. It is looked up by hot code (filters
// run it per candidate), so it's worked out at most once per frame; RH_InvalidateSettingsHash forces a recount.
static EWRAM_DATA u32 sSettingsHashFrame = 0;
static EWRAM_DATA u32 sSettingsHash = 0;
static EWRAM_DATA bool8 sSettingsHashValid = FALSE;

void RH_InvalidateSettingsHash(void)
{
    sSettingsHashValid = FALSE;
}

u32 RH_SettingsHash(void)
{
    const u8 *b = (const u8 *)S;
    u32 i, h = 2166136261u;
    if (sSettingsHashValid && sSettingsHashFrame == gMain.vblankCounter1)
        return sSettingsHash;
    for (i = 0; i < sizeof(struct RhSettings); i++)
        h = (h ^ b[i]) * 16777619u;
    sSettingsHash = h;
    sSettingsHashFrame = gMain.vblankCounter1;
    sSettingsHashValid = TRUE;
    return h;
}

u32 RH_Hash(u32 salt, u32 a, u32 b)
{
    return Mix(S->seed ^ Mix(salt * 0x9E3779B1 + a * 0x85EBCA77 + b * 0xC2B2AE3D + 0x27D4EB2F));
}

// Keyed bijection on [0, n): a Feistel network plus cycle walking. Lets one-to-one options stay unique
// without storing a table.
u32 RH_Permute(u32 salt, u32 x, u32 n)
{
    u32 bits = 1, half, mask, i, l, r;
    if (n <= 1)
        return 0;
    while ((1u << bits) < n)
        bits++;
    if (bits & 1)
        bits++;
    half = bits / 2;
    mask = (1u << half) - 1;
    do
    {
        l = x >> half;
        r = x & mask;
        for (i = 0; i < 4; i++)
        {
            u32 t = l ^ (RH_Hash(salt, r, i) & mask);
            l = r;
            r = t;
        }
        x = (l << half) | r;
    } while (x >= n);
    return x;
}

s32 RH_ApplyPercent(s32 value, s16 percent)
{
    s32 v = (value * (100 + percent) + 50) / 100;          // rounded (FVX)
    return v < 1 ? 1 : v;
}

// ---------------------------------------------------------------------------
// Species pool
// ---------------------------------------------------------------------------
const struct RhPoolMon *RH_PoolAt(u32 index)
{
    return &sRhPool[index];
}

// Pool entry details for other modules (the pool struct is private to this file).
void RH_PoolTraits(u32 index, struct RhPoolTraits *out)
{
    const struct RhPoolMon *m = &sRhPool[index];
    out->species = m->species;
    out->stage = m->stage;
    out->chain = m->chain;
    out->legendary = m->legendary;
    out->baseForm = !m->variant && !m->mega;
}

u16 RH_PoolSpecies(u32 index)
{
    return index < RH_POOL_COUNT ? sRhPool[index].species : SPECIES_NONE;
}

u32 RH_PoolCount(void)
{
    return RH_POOL_COUNT;
}

static bool32 PoolAllowedMon(const struct RhPoolMon *m)
{
    if (m->mega)
        return FALSE;
    switch (S->speciesPool)
    {
    case RH_POOL_GEN1:     return m->gen == 1 && !m->variant;
    case RH_POOL_GEN1_3:   return m->gen <= 3 && !m->variant;
    case RH_POOL_ALL:      return !m->variant;
    default:               return TRUE;
    }
}

bool32 RH_PoolAllowed(u32 index)
{
    return index < RH_POOL_COUNT && PoolAllowedMon(&sRhPool[index]);
}

s32 RH_PoolIndexOf(u16 species)
{
    s32 lo = 0, hi = RH_POOL_COUNT - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sRhPool[mid].species == species)
            return mid;
        if (sRhPool[mid].species < species)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

u16 RH_PreEvo(u16 species)
{
    if (species < ARRAY_COUNT(sRhPreEvo))
        return sRhPreEvo[species];
    return SPECIES_NONE;
}

// Number of allowed Pokemon with each type (current types), by type index. Cached: counting the pool is far
// too slow to do per pick.
struct TypeCountCache { u32 key; u16 count[18]; };
static EWRAM_DATA struct TypeCountCache sTypeCounts = {0};

u32 RH_TypeCount(u32 typeIndex)
{
    u32 i, key = RH_SettingsHash() | 1;
    if (sTypeCounts.key != key)
    {
        memset(sTypeCounts.count, 0, sizeof(sTypeCounts.count));
        for (i = 0; i < RH_POOL_COUNT; i++)
        {
            u16 sp;
            if (!RH_PoolAllowed(i))
                continue;
            sp = RH_PoolSpecies(i);
            sTypeCounts.count[RH_TypeIndexOf(GetSpeciesType(sp, 0))]++;
            if (GetSpeciesType(sp, 1) != GetSpeciesType(sp, 0))
                sTypeCounts.count[RH_TypeIndexOf(GetSpeciesType(sp, 1))]++;
        }
        sTypeCounts.key = key;
    }
    return typeIndex < 18 ? sTypeCounts.count[typeIndex] : 0;
}

// A random type that at least "minCount" allowed Pokemon have (any type if none does).
u8 RH_RandomPopulatedType(u32 hash, u32 minCount)
{
    u32 i, start = hash % 18;
    for (i = 0; i < 18; i++)
        if (RH_TypeCount((start + i) % 18) >= minCount)
            return gRhMonTypes[(start + i) % 18];
    return gRhMonTypes[start];
}

// Older-generation data (only entries that differ from the current data are stored).
const u8 *RH_GenBaseStats(u16 species, u32 gen)
{
    s32 lo = 0, hi;
    if (gen < 1 || gen > 8)
        return NULL;
    hi = sRhGenStatsCount[gen] - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        const struct RhGenStats *t = &sRhGenStats[gen][mid];
        if (t->species == species)
            return t->stats;
        if (t->species < species)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

bool32 RH_GenMoveData(u16 move, u32 gen, struct RhMoveData *out)
{
    s32 lo = 0, hi;
    if (gen < 1 || gen > 8)
        return FALSE;
    hi = sRhGenMovesCount[gen] - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        const struct RhGenMove *t = &sRhGenMoves[gen][mid];
        if (t->move == move)
        {
            out->power = t->power;
            out->accuracy = t->accuracy;
            out->pp = t->pp;
            out->type = t->type;
            out->category = t->category;
            return TRUE;
        }
        if (t->move < move)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return FALSE;
}

u32 RH_StatsGen(void)
{
    if (S->enabled && S->updateBaseStatsGen)
        return S->updateBaseStatsGen;
    return S->mechanicsGen ? S->mechanicsGen : 9;
}

u32 RH_MovesGen(void)
{
    if (S->enabled && S->updateMovesGen)
        return S->updateMovesGen;
    return S->mechanicsGen ? S->mechanicsGen : 9;
}

// Different Pokemon (forms count once) this species evolves into in the base game, among the allowed Pokemon.
static u32 DistinctVanillaTargets(u16 species)
{
    const struct Evolution *e = GetSpeciesEvolutionsVanilla(species);
    u16 seen[8];
    u32 i, j, n = 0;
    for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END && n < ARRAY_COUNT(seen); i++)
    {
        u16 base;
        s32 idx;
        if (e[i].method == EVO_NONE || e[i].targetSpecies == SPECIES_NONE)
            continue;
        idx = RH_PoolIndexOf(e[i].targetSpecies);
        if (idx < 0 || !RH_PoolAllowed(idx))
            continue;
        base = GET_BASE_SPECIES_ID(e[i].targetSpecies);
        for (j = 0; j < n && seen[j] != base; j++)
            ;
        if (j == n)
            seen[n++] = base;
    }
    return n;
}

// Root for "Follow Evolutions" traits: like the family root, but a split evolution (Eeveelutions, Bellossom,
// Slowking, Gallade, Shedinja...) starts its own branch, as in FVX. Memoized: it's used by every type lookup.
struct TraitRootMemo { u16 species; u16 root; u8 pool; };
static EWRAM_DATA struct TraitRootMemo sTraitRootMemo[64] = {0};

u16 RH_TraitRoot(u16 species)
{
    struct TraitRootMemo *m = &sTraitRootMemo[species & 63];
    u16 s = species;
    u32 g;
    if (m->species == species && m->pool == S->speciesPool + 1)
        return m->root;
    for (g = 0; g < 3; g++)
    {
        u16 pre = RH_PreEvo(s);
        if (pre == SPECIES_NONE || DistinctVanillaTargets(pre) > 1)
            break;
        s = pre;
    }
    m->species = species;
    m->root = s;
    m->pool = S->speciesPool + 1;
    return s;
}

u16 RH_FamilyRoot(u16 species)
{
    if (species < ARRAY_COUNT(sRhFamilyRoot) && sRhFamilyRoot[species] != SPECIES_NONE)
        return sRhFamilyRoot[species];
    return species;
}

bool32 RH_IsLegendary(u16 species)
{
    s32 i = RH_PoolIndexOf(species);
    if (i >= 0)
        return sRhPool[i].legendary;
    species = SanitizeSpeciesId(species);
    return gSpeciesInfo[species].isRestrictedLegendary || gSpeciesInfo[species].isSubLegendary
        || gSpeciesInfo[species].isMythical || gSpeciesInfo[species].isUltraBeast || gSpeciesInfo[species].isParadox;
}

u32 RH_VanillaBST(u16 species)
{
    const struct SpeciesInfo *i = &gSpeciesInfo[SanitizeSpeciesId(species)];
    return i->baseHP + i->baseAttack + i->baseDefense + i->baseSpeed + i->baseSpAttack + i->baseSpDefense;
}

u32 RH_SpeciesStage(u16 species)
{
    s32 i = RH_PoolIndexOf(species);
    return i >= 0 ? sRhPool[i].stage : 1;
}

u32 RH_SpeciesChain(u16 species)
{
    s32 i = RH_PoolIndexOf(species);
    return i >= 0 ? sRhPool[i].chain : 1;
}

bool32 RH_SpeciesHasType(u16 species, u8 type)
{
    return GetSpeciesType(species, 0) == type || GetSpeciesType(species, 1) == type;
}

u32 RH_TypeIndexOf(u8 type)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(gRhMonTypes); i++)
        if (gRhMonTypes[i] == type)
            return i;
    return 0;
}

u8 RH_RandomMonType(u32 hash)
{
    return gRhMonTypes[hash % ARRAY_COUNT(gRhMonTypes)];
}

void RH_FilterExclude(struct RhFilter *f, u16 species)
{
    if (species != SPECIES_NONE && f->excludeCount < RH_MAX_EXCLUDE)
        f->exclude[f->excludeCount++] = species;
}

// ---------------------------------------------------------------------------
// Scatterbug / Spewpa / Vivillon: their 20 wing patterns count as ONE Pokemon each in the pool (owner's rule).
// A pick that lands on the line's base form rolls one of the patterns when forms are in play ("All + Forms"
// pool or "Allow Alternate Formes"); the patterns are never picked on their own.
// ---------------------------------------------------------------------------
#define VIVILLON_PATTERNS 20
static const u16 sVivillonLines[3][VIVILLON_PATTERNS] = {
    {
    SPECIES_SCATTERBUG_ICY_SNOW, SPECIES_SCATTERBUG_POLAR, SPECIES_SCATTERBUG_TUNDRA, SPECIES_SCATTERBUG_CONTINENTAL,
    SPECIES_SCATTERBUG_GARDEN, SPECIES_SCATTERBUG_ELEGANT, SPECIES_SCATTERBUG_MEADOW, SPECIES_SCATTERBUG_MODERN,
    SPECIES_SCATTERBUG_MARINE, SPECIES_SCATTERBUG_ARCHIPELAGO, SPECIES_SCATTERBUG_HIGH_PLAINS, SPECIES_SCATTERBUG_SANDSTORM,
    SPECIES_SCATTERBUG_RIVER, SPECIES_SCATTERBUG_MONSOON, SPECIES_SCATTERBUG_SAVANNA, SPECIES_SCATTERBUG_SUN,
    SPECIES_SCATTERBUG_OCEAN, SPECIES_SCATTERBUG_JUNGLE, SPECIES_SCATTERBUG_FANCY, SPECIES_SCATTERBUG_POKEBALL,
    },
    {
    SPECIES_SPEWPA_ICY_SNOW, SPECIES_SPEWPA_POLAR, SPECIES_SPEWPA_TUNDRA, SPECIES_SPEWPA_CONTINENTAL,
    SPECIES_SPEWPA_GARDEN, SPECIES_SPEWPA_ELEGANT, SPECIES_SPEWPA_MEADOW, SPECIES_SPEWPA_MODERN,
    SPECIES_SPEWPA_MARINE, SPECIES_SPEWPA_ARCHIPELAGO, SPECIES_SPEWPA_HIGH_PLAINS, SPECIES_SPEWPA_SANDSTORM,
    SPECIES_SPEWPA_RIVER, SPECIES_SPEWPA_MONSOON, SPECIES_SPEWPA_SAVANNA, SPECIES_SPEWPA_SUN,
    SPECIES_SPEWPA_OCEAN, SPECIES_SPEWPA_JUNGLE, SPECIES_SPEWPA_FANCY, SPECIES_SPEWPA_POKEBALL,
    },
    {
    SPECIES_VIVILLON_ICY_SNOW, SPECIES_VIVILLON_POLAR, SPECIES_VIVILLON_TUNDRA, SPECIES_VIVILLON_CONTINENTAL,
    SPECIES_VIVILLON_GARDEN, SPECIES_VIVILLON_ELEGANT, SPECIES_VIVILLON_MEADOW, SPECIES_VIVILLON_MODERN,
    SPECIES_VIVILLON_MARINE, SPECIES_VIVILLON_ARCHIPELAGO, SPECIES_VIVILLON_HIGH_PLAINS, SPECIES_VIVILLON_SANDSTORM,
    SPECIES_VIVILLON_RIVER, SPECIES_VIVILLON_MONSOON, SPECIES_VIVILLON_SAVANNA, SPECIES_VIVILLON_SUN,
    SPECIES_VIVILLON_OCEAN, SPECIES_VIVILLON_JUNGLE, SPECIES_VIVILLON_FANCY, SPECIES_VIVILLON_POKEBALL,
    },
};

static bool32 IsVivillonPattern(u16 species)
{
    u32 s, p;
    for (s = 0; s < 3; s++)
        for (p = 1; p < VIVILLON_PATTERNS; p++)
            if (sVivillonLines[s][p] == species)
                return TRUE;
    return FALSE;
}

static u16 RollVivillonPattern(u16 species, const struct RhFilter *f, u32 hash)
{
    u32 s;
    if (species == SPECIES_NONE || !(f->allowVariants || S->speciesPool == RH_POOL_ALL_FORMS))
        return species;
    for (s = 0; s < 3; s++)
        if (sVivillonLines[s][0] == species)
            return sVivillonLines[s][RH_Hash(hash, species, 0x717) % VIVILLON_PATTERNS];
    return species;
}

static bool32 FilterOk(const struct RhPoolMon *m, const struct RhFilter *f)
{
    u32 i;
    if (m->variant && IsVivillonPattern(m->species))
        return FALSE;
    if (m->mega)
    {
        if (!f->allowMegas)
            return FALSE;
        if (!f->allowVariants && S->speciesPool < RH_POOL_ALL && m->gen > (S->speciesPool == RH_POOL_GEN1 ? 1 : 3))
            return FALSE;
    }
    else if (f->allowMegas && f->megasOnly)
    {
        return FALSE;
    }
    else if (f->allowVariants && m->variant)
    {
        // regional / alternate forms allowed regardless of the pool setting (still limited by generation)
        if (S->speciesPool == RH_POOL_GEN1 && m->gen != 1)
            return FALSE;
        if (S->speciesPool == RH_POOL_GEN1_3 && m->gen > 3)
            return FALSE;
    }
    else if (!PoolAllowedMon(m))
    {
        return FALSE;
    }
    if (f->legend == 1 && m->legendary)
        return FALSE;
    if (f->legend == 2 && !m->legendary)
        return FALSE;
    if ((f->minBst || f->maxBst) && S->enabled && S->bstMode != 0)
    {
        u32 bst = RH_SpeciesBST(m->species);                 // "Base Stat Totals" changed it
        if ((f->minBst && bst < f->minBst) || (f->maxBst && bst > f->maxBst))
            return FALSE;
    }
    else
    {
        if (f->minBst && m->bst < f->minBst)
            return FALSE;
        if (f->maxBst && m->bst > f->maxBst)
            return FALSE;
    }
    if (f->stage && m->stage != f->stage)
        return FALSE;
    if (f->threeStageBasic && !(m->stage == 1 && m->chain == 3))
        return FALSE;
    for (i = 0; i < f->excludeCount; i++)
        if (f->exclude[i] == m->species)
            return FALSE;
    if (f->monoType && GetSpeciesType(m->species, 0) != GetSpeciesType(m->species, 1))
        return FALSE;
    if (f->primaryType != TYPE_NONE && GetSpeciesType(m->species, 0) != f->primaryType)
        return FALSE;
    if (f->type != TYPE_NONE && !RH_SpeciesHasType(m->species, f->type))
        return FALSE;
    if (f->legalAtLevel && !RH_IsLegalEvolutionAtLevel(m->species, f->legalAtLevel))
        return FALSE;
    if (f->extra != NULL && !f->extra(m->species))
        return FALSE;
    if (f->noLeagueReserved && RH_IsLeagueReserved(m->species))
        return FALSE;
    return TRUE;
}

bool32 RH_FilterAccepts(const struct RhFilter *f, u32 poolIndex)
{
    return poolIndex < RH_POOL_COUNT && FilterOk(&sRhPool[poolIndex], f);
}

// One pass collects the accepted Pokemon (the filter is the expensive part); nested picks (a filter callback that
// picks itself) fall back to counting twice.
static EWRAM_DATA u16 sPickBuffer[RH_POOL_COUNT] = {0};
static EWRAM_DATA u8 sPickDepth = 0;

// Ranked picking (see RH_PickSpeciesNearBstRanked): the pick is the rank-th accepted Pokemon in a keyed order.
static EWRAM_DATA bool8 sRankedPick = FALSE;
static EWRAM_DATA u32 sRankedSalt = 0;

static u16 PickWithFilter(const struct RhFilter *f, u32 hash)
{
    u32 i, count = 0, target;
    if (sRankedPick && sPickDepth == 0)
    {
        u16 result = SPECIES_NONE;
        sPickDepth++;
        for (i = 0; i < RH_POOL_COUNT; i++)
            if (FilterOk(&sRhPool[i], f))
                sPickBuffer[count++] = i;
        if (count != 0)
            result = sRhPool[sPickBuffer[RH_Permute(sRankedSalt, hash % count, count)]].species;
        sPickDepth--;
        return result;
    }
    // Random candidates first: uniform among the accepted ones, and far cheaper than scanning the whole pool
    // (each filter check costs ~20 us on the GBA). Only strict filters get to the full scan.
    for (i = 0; i < 48; i++)
    {
        u32 k = RH_Hash(hash, i, 0x5EED) % RH_POOL_COUNT;
        if (FilterOk(&sRhPool[k], f))
            return sRhPool[k].species;
    }
    if (sPickDepth == 0)
    {
        u16 result = SPECIES_NONE;
        sPickDepth++;
        for (i = 0; i < RH_POOL_COUNT; i++)
            if (FilterOk(&sRhPool[i], f))
                sPickBuffer[count++] = i;
        if (count != 0)
            result = sRhPool[sPickBuffer[hash % count]].species;
        sPickDepth--;
        return result;
    }
    for (i = 0; i < RH_POOL_COUNT; i++)
        if (FilterOk(&sRhPool[i], f))
            count++;
    if (count == 0)
        return SPECIES_NONE;
    target = hash % count;
    for (i = 0; i < RH_POOL_COUNT; i++)
    {
        if (FilterOk(&sRhPool[i], f))
        {
            if (target == 0)
                return sRhPool[i].species;
            target--;
        }
    }
    return SPECIES_NONE;
}

// The rank-th allowed Pokemon in a keyed order (a bijection on the accepted ones): different ranks give different
// Pokemon until every accepted one has been used. Used by "even distribution" and Catch Em' All.
static u16 PickRanked(const struct RhFilter *f, u32 rank, u32 salt)
{
    u32 i, count = 0;
    u16 result = SPECIES_NONE;
    if (sPickDepth != 0)
    {
        u32 k;
        for (i = 0; i < RH_POOL_COUNT; i++)
            count += FilterOk(&sRhPool[i], f);
        if (count == 0)
            return SPECIES_NONE;
        k = RH_Permute(salt, rank % count, count);
        for (i = 0; i < RH_POOL_COUNT; i++)
            if (FilterOk(&sRhPool[i], f) && k-- == 0)
                return sRhPool[i].species;
        return SPECIES_NONE;
    }
    sPickDepth++;
    for (i = 0; i < RH_POOL_COUNT; i++)
        if (FilterOk(&sRhPool[i], f))
            sPickBuffer[count++] = i;
    if (count != 0)
        result = sRhPool[sPickBuffer[RH_Permute(salt, rank % count, count)]].species;
    sPickDepth--;
    return result;
}

u16 RH_PickWithFilter(const struct RhFilter *f, u32 hash)
{
    return RollVivillonPattern(PickWithFilter(f, hash), f, hash);
}

u16 RH_PickRanked(const struct RhFilter *f, u32 rank, u32 salt)
{
    return RollVivillonPattern(PickRanked(f, rank, salt), f, rank ^ salt);
}

bool32 RH_DebugIsVivillonPattern(u16 species) { return IsVivillonPattern(species); }

// Picks a species. With "similarTo", tries +-10%, 20%, 35% BST windows first. When nothing matches, relaxes the
// type, stage, exclusion and legendary rules (in that order) rather than failing.
u16 RH_PickSpecies(struct RhFilter *f, u32 hash, u16 similarTo)
{
    return RH_PickSpeciesNearBst(f, hash, similarTo != SPECIES_NONE ? RH_SpeciesBST(similarTo) : 0);
}

u16 RH_PickSpeciesNearBst(struct RhFilter *f, u32 hash, u32 bst)
{
    static const u8 sWindows[] = { 10, 20, 35 };
    u32 i;
    u16 result;
    u16 keepMin = f->minBst, keepMax = f->maxBst;
    if (bst != 0)
    {
        for (i = 0; i < ARRAY_COUNT(sWindows); i++)
        {
            f->minBst = max(keepMin, bst * (100 - sWindows[i]) / 100);
            f->maxBst = keepMax ? min(keepMax, bst * (100 + sWindows[i]) / 100) : bst * (100 + sWindows[i]) / 100;
            result = RH_PickWithFilter(f, hash);
            if (result != SPECIES_NONE)
                return result;
        }
        f->minBst = keepMin;
        f->maxBst = keepMax;
    }
    if ((result = RH_PickWithFilter(f, hash)) != SPECIES_NONE)
        return result;
    if (f->type != TYPE_NONE || f->primaryType != TYPE_NONE || f->monoType)
    {
        f->type = f->primaryType = TYPE_NONE;
        f->monoType = FALSE;
        if ((result = RH_PickWithFilter(f, hash)) != SPECIES_NONE)
            return result;
    }
    if (f->stage || f->threeStageBasic || f->minBst || f->maxBst || f->legalAtLevel)
    {
        f->stage = 0;
        f->threeStageBasic = FALSE;
        f->minBst = f->maxBst = 0;
        f->legalAtLevel = 0;
        if ((result = RH_PickWithFilter(f, hash)) != SPECIES_NONE)
            return result;
    }
    f->excludeCount = 0;
    f->extra = NULL;
    f->megasOnly = FALSE;
    if ((result = RH_PickWithFilter(f, hash)) != SPECIES_NONE)
        return result;
    f->legend = 0;
    return RH_PickWithFilter(f, hash);
}

// Like RH_PickSpeciesNearBst, but different ranks with the same filter and salt never give the same Pokemon
// (wild 1-to-1 zones: the Pokemon of one zone all get different replacements, as in FVX).
u16 RH_PickSpeciesNearBstRanked(struct RhFilter *f, u32 rank, u32 salt, u32 bst)
{
    u16 result;
    sRankedPick = TRUE;
    sRankedSalt = salt;
    result = RH_PickSpeciesNearBst(f, rank, bst);
    sRankedPick = FALSE;
    return result;
}

// ---------------------------------------------------------------------------
// Evolution helpers (use the possibly randomized evolution data)
// ---------------------------------------------------------------------------
// FVX isLegalEvolutionAtLevel: could this Pokemon already have evolved at this level? Level-up evolutions use their
// level; other methods an estimated level from the target's base stat total.
bool32 RH_IsLegalEvolutionAtLevel(u16 species, u32 level)
{
    u32 depth;
    for (depth = 0; depth < 3; depth++)
    {
        u16 pre = RH_PreEvo(species);
        const struct Evolution *e;
        u32 i, need = 0;
        if (pre == SPECIES_NONE)
            return TRUE;
        e = GetSpeciesEvolutions(pre);
        for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        {
            if (e[i].targetSpecies != species)
                continue;
            if (e[i].method == EVO_LEVEL || e[i].method == EVO_LEVEL_BATTLE_ONLY)
                need = e[i].param;
            else
            {
                need = RH_EstimateEvoLevel(pre, species);    // FVX: estimated evolution level
            }
            break;
        }
        if (need > level)
            return FALSE;
        species = pre;
    }
    return TRUE;
}

u16 RH_EvolveOnce(u16 species, u32 salt)
{
    const struct Evolution *e = GetSpeciesEvolutions(species);
    u32 i, n = 0, pick;
    for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        if (e[i].targetSpecies != SPECIES_NONE && e[i].targetSpecies != species && e[i].method != EVO_NONE)
            n++;
    if (n == 0)
        return SPECIES_NONE;
    pick = salt % n;
    for (i = 0; e[i].method != EVOLUTIONS_END; i++)
        if (e[i].targetSpecies != SPECIES_NONE && e[i].targetSpecies != species && e[i].method != EVO_NONE)
            if (pick-- == 0)
                return e[i].targetSpecies;
    return SPECIES_NONE;
}

u16 RH_EvolveTimes(u16 species, u32 times)
{
    while (times--)
    {
        u16 next = RH_EvolveOnce(species, RH_Hash(SALT_EVO, species, times));
        if (next == SPECIES_NONE)
            break;
        species = next;
    }
    return species;
}

u16 RH_FullyEvolve(u16 species)
{
    return RH_EvolveTimes(species, 3);
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------
bool32 RH_ItemBanned(u16 item)
{
    if (GetItemPocket(item) == POCKET_KEY_ITEMS)
        return TRUE;                                         // e.g. the reusable Escape Rope: never a random item
    return item == ITEM_LUCKY_EGG && S->banLuckyEgg;
}

bool32 RH_ItemIsBad(u16 item)
{
    enum Pocket pocket = GetItemPocket(item);
    if (ItemIsMail(item))
        return TRUE;
    if (pocket == POCKET_BERRIES && item != ITEM_LUM_BERRY && item != ITEM_SITRUS_BERRY)
        return TRUE;
    switch (item)
    {
    case ITEM_TINY_MUSHROOM: case ITEM_PRETTY_FEATHER: case ITEM_HONEY: case ITEM_BLACK_FLUTE: case ITEM_WHITE_FLUTE:
    case ITEM_BLUE_FLUTE: case ITEM_YELLOW_FLUTE: case ITEM_RED_FLUTE: case ITEM_FRESH_WATER: case ITEM_SODA_POP:
    case ITEM_BERRY_JUICE: case ITEM_LAGGING_TAIL: case ITEM_IRON_BALL: case ITEM_RING_TARGET: case ITEM_STICKY_BARB:
    case ITEM_FULL_INCENSE: case ITEM_CLEANSE_TAG: case ITEM_SMOKE_BALL: case ITEM_SOOTHE_BELL: case ITEM_POKE_DOLL:
    case ITEM_FLUFFY_TAIL: case ITEM_POKE_TOY: case ITEM_HEART_SCALE: case ITEM_SHOAL_SALT: case ITEM_SHOAL_SHELL:
    case ITEM_RED_SHARD: case ITEM_BLUE_SHARD: case ITEM_YELLOW_SHARD: case ITEM_GREEN_SHARD:
        return TRUE;
    }
    return FALSE;
}

bool32 RH_ItemIsOverpowered(u16 item)
{
    switch (item)
    {
    case ITEM_LUCKY_EGG: case ITEM_RARE_CANDY: case ITEM_MASTER_BALL: case ITEM_EXP_CANDY_XL: case ITEM_ABILITY_PATCH:
    case ITEM_BIG_NUGGET: case ITEM_COMET_SHARD: case ITEM_RELIC_GOLD: case ITEM_PEARL_STRING: case ITEM_BALM_MUSHROOM:
    case ITEM_SACRED_ASH: case ITEM_MAX_HONEY: case ITEM_AMULET_COIN: case ITEM_GOLD_BOTTLE_CAP:
        return TRUE;
    }
    return GetItemPrice(item) > 10000 && GetItemPocket(item) == POCKET_ITEMS && GetItemHoldEffect(item) == HOLD_EFFECT_NONE;
}

static bool32 IsConsumableHeldItem(u16 item)
{
    enum HoldEffect h = GetItemHoldEffect(item);
    if (GetItemPocket(item) == POCKET_BERRIES)
        return TRUE;
    switch (h)
    {
    case HOLD_EFFECT_FOCUS_SASH: case HOLD_EFFECT_GEMS: case HOLD_EFFECT_WHITE_HERB: case HOLD_EFFECT_MENTAL_HERB:
    case HOLD_EFFECT_POWER_HERB: case HOLD_EFFECT_AIR_BALLOON: case HOLD_EFFECT_WEAKNESS_POLICY:
    case HOLD_EFFECT_EJECT_BUTTON: case HOLD_EFFECT_RED_CARD: case HOLD_EFFECT_ABSORB_BULB: case HOLD_EFFECT_CELL_BATTERY:
    case HOLD_EFFECT_THROAT_SPRAY: case HOLD_EFFECT_ROOM_SERVICE: case HOLD_EFFECT_BLUNDER_POLICY:
    case HOLD_EFFECT_EJECT_PACK: case HOLD_EFFECT_ADRENALINE_ORB: case HOLD_EFFECT_TERRAIN_SEED: case HOLD_EFFECT_BOOSTER_ENERGY:
    case HOLD_EFFECT_MIRROR_HERB:
        return TRUE;
    default:
        return FALSE;
    }
}

// Random item a Pokemon can usefully hold (no mega stones / z-crystals / key items).
u16 RH_RandomHeldItem(u32 hash, bool32 banBad, bool32 consumableOnly)
{
    u32 tries;
    for (tries = 0; tries < 64; tries++)
    {
        u16 item = 1 + RH_Hash(SALT_HELD_ITEM, hash, tries) % (ITEMS_COUNT - 1);
        enum HoldEffect h = GetItemHoldEffect(item);
        if (h == HOLD_EFFECT_NONE || h == HOLD_EFFECT_MEGA_STONE || h == HOLD_EFFECT_Z_CRYSTAL || GetItemPocket(item) == POCKET_KEY_ITEMS)
            continue;
        if (GetItemPrice(item) == 0 || RH_ItemBanned(item) || (banBad && RH_ItemIsBad(item)))
            continue;
        if (consumableOnly && !IsConsumableHeldItem(item))
            continue;
        return item;
    }
    return ITEM_ORAN_BERRY;
}
