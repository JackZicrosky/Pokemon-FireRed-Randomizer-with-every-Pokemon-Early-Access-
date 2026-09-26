// Randomizer core: settings, seeded hashing, the species pool and species picking.
#include "global.h"
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
    s->trainersEvolveLevel = 40;
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
    s->lowerCaseNames = TRUE;
    s->runWithoutShoes = FALSE;
}

void RH_ApplyPendingSettings(void)
{
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

// Hash of every setting: cache key for results that depend on many options.
u32 RH_SettingsHash(void)
{
    const u8 *b = (const u8 *)S;
    u32 i, h = 2166136261u;
    for (i = 0; i < sizeof(struct RhSettings); i++)
        h = (h ^ b[i]) * 16777619u;
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
    s32 v = value * (100 + percent) / 100;
    return v < 1 ? 1 : v;
}

// ---------------------------------------------------------------------------
// Species pool
// ---------------------------------------------------------------------------
const struct RhPoolMon *RH_PoolAt(u32 index)
{
    return &sRhPool[index];
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

static bool32 FilterOk(const struct RhPoolMon *m, const struct RhFilter *f)
{
    u32 i;
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
    if (f->minBst && m->bst < f->minBst)
        return FALSE;
    if (f->maxBst && m->bst > f->maxBst)
        return FALSE;
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
    if (f->extra != NULL && !f->extra(m->species))
        return FALSE;
    return TRUE;
}

bool32 RH_FilterAccepts(const struct RhFilter *f, u32 poolIndex)
{
    return poolIndex < RH_POOL_COUNT && FilterOk(&sRhPool[poolIndex], f);
}

u16 RH_PickWithFilter(const struct RhFilter *f, u32 hash)
{
    u32 i, count = 0, target;
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

// Picks a species. With "similarTo", tries +-10%, 20%, 35% BST windows first. When nothing matches, relaxes the
// type, stage, exclusion and legendary rules (in that order) rather than failing.
u16 RH_PickSpecies(struct RhFilter *f, u32 hash, u16 similarTo)
{
    return RH_PickSpeciesNearBst(f, hash, similarTo != SPECIES_NONE ? RH_VanillaBST(similarTo) : 0);
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
    if (f->stage || f->threeStageBasic || f->minBst || f->maxBst)
    {
        f->stage = 0;
        f->threeStageBasic = FALSE;
        f->minBst = f->maxBst = 0;
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

// ---------------------------------------------------------------------------
// Evolution helpers (use the possibly randomized evolution data)
// ---------------------------------------------------------------------------
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
