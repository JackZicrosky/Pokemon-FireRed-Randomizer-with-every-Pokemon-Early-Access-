// In-game randomizer (Universal Pokemon Randomizer style), driven by gSaveBlock3Ptr->rhSettings.
// Everything is computed on demand from the seed, so nothing large lives in RAM:
// the same seed + settings always produce the same game.
#include "global.h"
#include "data.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "item.h"
#include "move.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "safari_zone.h"
#include "string_util.h"
#include "rh.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/trainers.h"
#include "constants/maps.h"
#include "data/rh_randomizer_data.h"
#include "data/rh_randomizer_tables.h"

enum Species RH_SafariSpecies(enum Species species, enum WildPokemonArea area);

EWRAM_DATA struct RhSettings gRhPendingSettings = {0};

#define S (&gSaveBlock3Ptr->rhSettings)

// Salts so each feature draws from an independent random stream.
enum
{
    SALT_WILD = 1, SALT_WILD_AREA, SALT_WILD_THEME, SALT_TRAINER, SALT_TRAINER_THEME, SALT_STARTER,
    SALT_STATIC, SALT_TRADE, SALT_TYPE1, SALT_TYPE2, SALT_ABILITY, SALT_STATS, SALT_STAT_PERM,
    SALT_LEARNSET, SALT_EVO, SALT_TM, SALT_TM_COMPAT, SALT_TUTOR_COMPAT, SALT_MOVE_POWER, SALT_MOVE_ACC,
    SALT_MOVE_PP, SALT_MOVE_TYPE, SALT_MOVE_CAT, SALT_FIELD_ITEM, SALT_SHOP, SALT_HELD_ITEM, SALT_GLOBAL_PERM,
};

static const s8 sLevelMods[] = { -50, -40, -30, -20, -10, 0, 10, 20, 30, 40, 50 };

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

static u32 Hash(u32 salt, u32 a, u32 b)
{
    return Mix(S->seed ^ Mix(salt * 0x9E3779B1 + a * 0x85EBCA77 + b * 0xC2B2AE3D + 0x27D4EB2F));
}

// Keyed bijection on [0, n) (Feistel network + cycle walking). Lets "one-to-one" options stay unique
// without storing a table.
static u32 Permute(u32 salt, u32 x, u32 n)
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
            u32 t = l ^ (Hash(salt, r, i) & mask);
            l = r;
            r = t;
        }
        x = (l << half) | r;
    } while (x >= n);
    return x;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void RH_SetDefaultSettings(struct RhSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->version = RH_SETTINGS_VERSION;
    s->speciesPool = RH_POOL_ALL;
    s->baseStatsFollowEvos = TRUE;
    s->abilitiesFollowEvos = TRUE;
    s->banWonderGuard = TRUE;
    s->banTrapAbilities = TRUE;
    s->banBadAbilities = TRUE;
    s->banBrokenMoves = TRUE;
    s->rivalStarter = TRUE;
    s->trainerLevel = RH_LEVEL_MOD_DEFAULT;
    s->wildLevel = RH_LEVEL_MOD_DEFAULT;
    s->banBadItems = TRUE;
}

void RH_ApplyPendingSettings(void)
{
    gSaveBlock3Ptr->rhSettings = gRhPendingSettings;
    if (gSaveBlock3Ptr->rhSettings.seed == 0)
        gSaveBlock3Ptr->rhSettings.seed = 1;
}

// ---------------------------------------------------------------------------
// Species pool helpers
// ---------------------------------------------------------------------------
static bool32 PoolAllowed(const struct RhPoolMon *m)
{
    switch (S->speciesPool)
    {
    case RH_POOL_GEN1:     return m->gen == 1 && !m->variant;
    case RH_POOL_GEN1_3:   return m->gen <= 3 && !m->variant;
    case RH_POOL_ALL:      return !m->variant;
    default:               return TRUE;
    }
}

static s32 PoolIndexOf(u16 species)
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

static u16 FamilyRoot(u16 species)
{
    if (species < ARRAY_COUNT(sRhFamilyRoot) && sRhFamilyRoot[species] != SPECIES_NONE)
        return sRhFamilyRoot[species];
    return species;
}

static bool32 IsLegendarySpecies(u16 species)
{
    s32 i = PoolIndexOf(species);
    if (i >= 0)
        return sRhPool[i].legendary;
    return gSpeciesInfo[species].isRestrictedLegendary || gSpeciesInfo[species].isSubLegendary
        || gSpeciesInfo[species].isMythical || gSpeciesInfo[species].isUltraBeast || gSpeciesInfo[species].isParadox;
}

static u32 VanillaBST(u16 species)
{
    const struct SpeciesInfo *i = &gSpeciesInfo[SanitizeSpeciesId(species)];
    return i->baseHP + i->baseAttack + i->baseDefense + i->baseSpeed + i->baseSpAttack + i->baseSpDefense;
}

static bool32 SpeciesHasType(u16 species, u8 type)
{
    return GetSpeciesType(species, 0) == type || GetSpeciesType(species, 1) == type;
}

struct RhFilter
{
    u16 minBst, maxBst;       // 0 = no limit
    u8 type;                  // TYPE_NONE = any
    u8 legend;                // 0 any, 1 no legendaries, 2 only legendaries
    bool8 threeStageBasic;
    u16 exclude[3];
};

static bool32 FilterOk(const struct RhPoolMon *m, const struct RhFilter *f)
{
    u32 i;
    if (!PoolAllowed(m))
        return FALSE;
    if (f->legend == 1 && m->legendary)
        return FALSE;
    if (f->legend == 2 && !m->legendary)
        return FALSE;
    if (f->minBst && m->bst < f->minBst)
        return FALSE;
    if (f->maxBst && m->bst > f->maxBst)
        return FALSE;
    if (f->threeStageBasic && !(m->stage == 1 && m->chain == 3))
        return FALSE;
    for (i = 0; i < ARRAY_COUNT(f->exclude); i++)
        if (f->exclude[i] && f->exclude[i] == m->species)
            return FALSE;
    if (f->type != TYPE_NONE && !SpeciesHasType(m->species, f->type))
        return FALSE;
    return TRUE;
}

static u16 PickWithFilter(const struct RhFilter *f, u32 hash)
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

// Picks a species; if "similarTo" is set, tries +-10%, then 20%, 35% BST windows before giving up on strength.
static u16 PickSpecies(struct RhFilter *f, u32 hash, u16 similarTo)
{
    static const u8 sWindows[] = { 10, 20, 35 };
    u32 i;
    u16 result;
    if (similarTo != SPECIES_NONE)
    {
        u32 bst = VanillaBST(similarTo);
        for (i = 0; i < ARRAY_COUNT(sWindows); i++)
        {
            f->minBst = bst * (100 - sWindows[i]) / 100;
            f->maxBst = bst * (100 + sWindows[i]) / 100;
            result = PickWithFilter(f, hash);
            if (result != SPECIES_NONE)
                return result;
        }
        f->minBst = f->maxBst = 0;
    }
    result = PickWithFilter(f, hash);
    if (result == SPECIES_NONE && f->type != TYPE_NONE)
    {
        f->type = TYPE_NONE;
        result = PickWithFilter(f, hash);
    }
    if (result == SPECIES_NONE && f->legend)
    {
        f->legend = 0;
        result = PickWithFilter(f, hash);
    }
    return result;
}

static u32 TypeIndexOf(u8 type)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhTypes); i++)
        if (sRhTypes[i] == type)
            return i;
    return 0;
}

static u8 RandomMonType(u32 hash)
{
    return sRhTypes[hash % ARRAY_COUNT(sRhTypes)];
}

// ---------------------------------------------------------------------------
// Species traits: types / abilities / base stats
// ---------------------------------------------------------------------------
enum Type RH_SpeciesType(enum Species species, u32 slot, enum Type vanilla)
{
    const struct SpeciesInfo *info;
    u32 key;
    u8 t1, t2;
    if (!S->enabled || S->types == 0 || species == SPECIES_NONE || species >= NUM_SPECIES)
        return vanilla;
    info = &gSpeciesInfo[species];
    key = (S->types == 1) ? FamilyRoot(species) : species;
    t1 = RandomMonType(Hash(SALT_TYPE1, key, 0));
    if (slot == 0)
        return t1;
    if (info->types[0] == info->types[1])
        return t1;                                   // mono-typed stays mono-typed
    t2 = RandomMonType(Hash(SALT_TYPE2, key, 0));
    if (t2 == t1)
        t2 = sRhTypes[(TypeIndexOf(t1) + 1 + Hash(SALT_TYPE2, key, 1) % (ARRAY_COUNT(sRhTypes) - 1)) % ARRAY_COUNT(sRhTypes)];
    return t2;
}

static bool32 AbilityAllowed(u32 ability)
{
    switch (ability)
    {
    case ABILITY_NONE:
    // Form-specific / signature abilities that break on other species.
    case ABILITY_MULTITYPE: case ABILITY_STANCE_CHANGE: case ABILITY_SCHOOLING: case ABILITY_SHIELDS_DOWN:
    case ABILITY_DISGUISE: case ABILITY_BATTLE_BOND: case ABILITY_POWER_CONSTRUCT: case ABILITY_RKS_SYSTEM:
    case ABILITY_ZEN_MODE: case ABILITY_ICE_FACE: case ABILITY_GULP_MISSILE: case ABILITY_HUNGER_SWITCH:
    case ABILITY_ZERO_TO_HERO: case ABILITY_COMMANDER: case ABILITY_AS_ONE_ICE_RIDER: case ABILITY_AS_ONE_SHADOW_RIDER:
    case ABILITY_TERA_SHIFT: case ABILITY_TERAFORM_ZERO: case ABILITY_FORECAST: case ABILITY_FLOWER_GIFT:
    case ABILITY_EMBODY_ASPECT_TEAL_MASK: case ABILITY_EMBODY_ASPECT_HEARTHFLAME_MASK:
    case ABILITY_EMBODY_ASPECT_WELLSPRING_MASK: case ABILITY_EMBODY_ASPECT_CORNERSTONE_MASK:
    case ABILITY_POISON_PUPPETEER: case ABILITY_TERA_SHELL:
        return FALSE;
    case ABILITY_WONDER_GUARD:
        return !S->banWonderGuard;
    case ABILITY_ARENA_TRAP: case ABILITY_SHADOW_TAG: case ABILITY_MAGNET_PULL:
        return !S->banTrapAbilities;
    case ABILITY_TRUANT: case ABILITY_SLOW_START: case ABILITY_DEFEATIST: case ABILITY_KLUTZ: case ABILITY_STALL:
        return !S->banBadAbilities;
    }
    return ability < ABILITIES_COUNT;
}

enum Ability RH_SpeciesAbility(enum Species species, u32 slot, enum Ability vanilla)
{
    u32 key, h, tries;
    if (!S->enabled || S->abilities == 0 || vanilla == ABILITY_NONE || species >= NUM_SPECIES)
        return vanilla;
    key = S->abilitiesFollowEvos ? FamilyRoot(species) : species;
    for (tries = 0; tries < 16; tries++)
    {
        h = 1 + Hash(SALT_ABILITY, key, slot * 16 + tries) % (ABILITIES_COUNT - 1);
        if (AbilityAllowed(h))
            return h;
    }
    return vanilla;
}

u32 RH_SpeciesBaseStat(enum Species species, u32 stat, u32 vanilla)
{
    const struct SpeciesInfo *info;
    u8 v[NUM_STATS];
    u32 i, key, bst, total;
    if (!S->enabled || S->baseStats == 0 || species == SPECIES_NONE || species >= NUM_SPECIES || stat >= NUM_STATS)
        return vanilla;
    info = &gSpeciesInfo[species];
    if (info->baseHP == 1 && stat == STAT_HP)
        return 1;                                    // Shedinja
    v[STAT_HP] = info->baseHP; v[STAT_ATK] = info->baseAttack; v[STAT_DEF] = info->baseDefense;
    v[STAT_SPEED] = info->baseSpeed; v[STAT_SPATK] = info->baseSpAttack; v[STAT_SPDEF] = info->baseSpDefense;
    key = S->baseStatsFollowEvos ? FamilyRoot(species) : species;
    if (S->baseStats == 1)
    {
        // Shuffle: the same permutation of the 6 stats for the whole family.
        u8 perm[NUM_STATS] = {0, 1, 2, 3, 4, 5};
        for (i = NUM_STATS - 1; i > 0; i--)
        {
            u32 j = Hash(SALT_STAT_PERM, key, i) % (i + 1);
            u8 t = perm[i]; perm[i] = perm[j]; perm[j] = t;
        }
        if (info->baseHP == 1)
            return v[perm[stat]] == 1 ? v[STAT_ATK] : v[perm[stat]];
        return v[perm[stat]];
    }
    // Random: keep the base stat total, redistribute it with family-wide random weights.
    bst = 0;
    for (i = 0; i < NUM_STATS; i++)
        bst += v[i];
    total = 0;
    for (i = 0; i < NUM_STATS; i++)
        total += 20 + Hash(SALT_STATS, key, i) % 100;
    {
        u32 w = 20 + Hash(SALT_STATS, key, stat) % 100;
        u32 val = bst * w / total;
        if (val < 10) val = 10;
        if (val > 255) val = 255;
        return val;
    }
}

u8 RH_CatchRate(enum Species species, u8 vanilla)
{
    if (!S->enabled)
        return vanilla;
    switch (S->wildCatchRate)
    {
    case 1: return max(vanilla, 75);
    case 2: return max(vanilla, 150);
    case 3: return 255;
    }
    return vanilla;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------
static bool32 IsRandomizableMove(u32 move)
{
    return move != MOVE_NONE && move < MOVES_COUNT && move != MOVE_STRUGGLE;
}

u32 RH_MovePower(enum Move move, u32 vanilla)
{
    if (!S->enabled || !S->movePower || vanilla <= 1 || !IsRandomizableMove(move))
        return vanilla;
    return 20 + 5 * (Hash(SALT_MOVE_POWER, move, 0) % 25);          // 20..140
}

u32 RH_MoveAccuracy(enum Move move, u32 vanilla)
{
    u32 h;
    if (!S->enabled || !S->moveAccuracy || vanilla == 0 || !IsRandomizableMove(move))
        return vanilla;
    h = Hash(SALT_MOVE_ACC, move, 0);
    if (h % 2)
        return 100;
    return 60 + 5 * ((h >> 1) % 8);                                 // 60..95
}

u32 RH_MovePP(enum Move move, u32 vanilla)
{
    if (!S->enabled || !S->movePP || !IsRandomizableMove(move))
        return vanilla;
    return 5 * (1 + Hash(SALT_MOVE_PP, move, 0) % 8);               // 5..40
}

enum Type RH_MoveType(enum Move move, enum Type vanilla)
{
    if (!S->enabled || !S->moveType || !IsRandomizableMove(move) || vanilla == TYPE_MYSTERY || vanilla == TYPE_NONE)
        return vanilla;
    return RandomMonType(Hash(SALT_MOVE_TYPE, move, 0));
}

u32 RH_MoveCategory(enum Move move, u32 vanilla)
{
    if (!S->enabled || !S->moveCategory || !IsRandomizableMove(move) || vanilla == DAMAGE_CATEGORY_STATUS || vanilla == DAMAGE_CATEGORY_NONE)
        return vanilla;
    return (Hash(SALT_MOVE_CAT, move, 0) & 1) ? DAMAGE_CATEGORY_PHYSICAL : DAMAGE_CATEGORY_SPECIAL;
}

static s32 TypeIndex(u8 type)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhTypes); i++)
        if (sRhTypes[i] == type)
            return i;
    return -1;
}

static bool32 MoveOkForLearnset(const struct RhMove *m, bool32 needDamaging)
{
    if (S->banBrokenMoves && m->broken)
        return FALSE;
    if (needDamaging && !m->damaging)
        return FALSE;
    return TRUE;
}

// Picks a random move; optionally from one type, optionally damaging only.
static u16 PickMove(u32 hash, s32 typeIdx, bool32 needDamaging)
{
    u32 start = 0, end = RH_MOVE_COUNT, i, count = 0, target;
    if (typeIdx >= 0)
    {
        start = sRhMoveTypeStart[typeIdx];
        end = sRhMoveTypeStart[typeIdx + 1];
    }
    for (i = start; i < end; i++)
        if (MoveOkForLearnset(&sRhMoves[i], needDamaging))
            count++;
    if (count == 0)
        return (typeIdx >= 0) ? PickMove(hash, -1, needDamaging) : MOVE_TACKLE;
    target = hash % count;
    for (i = start; i < end; i++)
    {
        if (MoveOkForLearnset(&sRhMoves[i], needDamaging))
        {
            if (target == 0)
                return sRhMoves[i].move;
            target--;
        }
    }
    return MOVE_TACKLE;
}

#define LEARNSET_MAX 64
struct LearnsetCache { u16 species; u32 seed; struct LevelUpMove moves[LEARNSET_MAX + 1]; };
static EWRAM_DATA struct LearnsetCache sLearnsetCache[2] = {0};
static EWRAM_DATA u8 sLearnsetCacheNext = 0;

const struct LevelUpMove *RH_LevelUpLearnset(enum Species species, const struct LevelUpMove *vanilla)
{
    u32 i, j, n, tries;
    struct LearnsetCache *c;
    static const u8 sDamagingPct[] = { 0, 25, 50, 75 };
    if (!S->enabled || S->movesets == 0 || species == SPECIES_NONE)
        return vanilla;
    for (i = 0; i < ARRAY_COUNT(sLearnsetCache); i++)
        if (sLearnsetCache[i].species == species && sLearnsetCache[i].seed == S->seed)
            return sLearnsetCache[i].moves;
    c = &sLearnsetCache[sLearnsetCacheNext];
    sLearnsetCacheNext = (sLearnsetCacheNext + 1) % ARRAY_COUNT(sLearnsetCache);
    c->species = species;
    c->seed = S->seed;

    if (S->movesets == 3)
    {
        c->moves[0].move = MOVE_METRONOME;
        c->moves[0].level = 1;
        c->moves[1].move = LEVEL_UP_MOVE_END;
        c->moves[1].level = 0;
        return c->moves;
    }
    for (n = 0; n < LEARNSET_MAX && vanilla[n].move != LEVEL_UP_MOVE_END; n++)
    {
        u16 move = MOVE_TACKLE;
        for (tries = 0; tries < 8; tries++)
        {
            u32 h = Hash(SALT_LEARNSET, species, n * 8 + tries);
            bool32 damaging = (h % 100) < sDamagingPct[S->goodDamaging] || (n == 0 && tries < 4);
            s32 typeIdx = -1;
            if (S->movesets == 1 && ((h >> 8) % 2) == 0)
                typeIdx = TypeIndex(GetSpeciesType(species, (h >> 9) & 1));
            move = PickMove(h >> 10, typeIdx, damaging);
            for (j = 0; j < n; j++)
                if (c->moves[j].move == move)
                    break;
            if (j == n)
                break;
        }
        c->moves[n].move = move;
        c->moves[n].level = vanilla[n].level;
    }
    c->moves[n].move = LEVEL_UP_MOVE_END;
    c->moves[n].level = 0;
    return c->moves;
}

// TM moves: a keyed permutation over the move pool keeps every TM unique. HMs are never changed.
enum Move RH_TMMove(u32 index, enum Move vanilla)
{
    u32 x, guard;
    if (!S->enabled || !S->tmMoves || index >= NUM_TECHNICAL_MACHINES)
        return vanilla;
    x = index;
    for (guard = 0; guard < 64; guard++)
    {
        const struct RhMove *m;
        x = Permute(SALT_TM, x, RH_MOVE_COUNT);
        m = &sRhMoves[x];
        if (!m->broken && m->move != MOVE_CUT && m->move != MOVE_FLY && m->move != MOVE_SURF && m->move != MOVE_STRENGTH
         && m->move != MOVE_FLASH && m->move != MOVE_ROCK_SMASH && m->move != MOVE_WATERFALL && m->move != MOVE_DIVE)
            return m->move;
    }
    return vanilla;
}

static s32 TMIndexOfMove(enum Move move)
{
    u32 i;
    for (i = 0; i < NUM_ALL_MACHINES; i++)
        if (GetTMHMMoveId(i) == move)
            return i;
    return -1;
}

static bool32 RandomCompat(u32 salt, u32 mode, enum Species species, enum Move move, u32 key)
{
    u32 h = Hash(salt, FamilyRoot(species) * 7 + species, key) % 100;
    u8 type = GetMoveType(move);
    if (mode == 3)
        return TRUE;
    if (mode == 2)
        return h < 50;
    // prefer same type
    if (SpeciesHasType(species, type))
        return h < 90;
    if (type == TYPE_NORMAL)
        return h < 50;
    return h < 25;
}

bool32 RH_CanLearnTeachable(enum Species species, enum Move move, bool32 (*vanillaCheck)(enum Species, enum Move))
{
    s32 tm;
    if (!S->enabled || species == SPECIES_EGG || species == SPECIES_NONE)
        return vanillaCheck(species, move);
    tm = TMIndexOfMove(move);
    if (tm >= 0)
    {
        if (tm >= NUM_TECHNICAL_MACHINES)                       // HM
            return S->tmCompat == 3 ? TRUE : vanillaCheck(species, move);
        if (S->tmCompat == 0)
            return vanillaCheck(species, gTMHMItemMoveIds[tm].moveId);   // keep the slot's original compatibility
        return RandomCompat(SALT_TM_COMPAT, S->tmCompat, species, move, tm);
    }
    if (S->tutorCompat == 0)
        return vanillaCheck(species, move);
    return RandomCompat(SALT_TUTOR_COMPAT, S->tutorCompat, species, move, move);
}

// ---------------------------------------------------------------------------
// Evolutions
// ---------------------------------------------------------------------------
#define EVO_MAX 16
struct EvoCache { u16 species; u32 seed; struct Evolution evos[EVO_MAX + 1]; };
static EWRAM_DATA struct EvoCache sEvoCache[4] = {0};
static EWRAM_DATA u8 sEvoCacheNext = 0;

const struct Evolution *RH_Evolutions(enum Species species, const struct Evolution *vanilla)
{
    u32 i, n;
    struct EvoCache *c;
    if (!S->enabled || S->evolutions == 0 || species == SPECIES_NONE || vanilla == NULL || vanilla[0].method == EVOLUTIONS_END)
        return vanilla;
    for (i = 0; i < ARRAY_COUNT(sEvoCache); i++)
        if (sEvoCache[i].species == species && sEvoCache[i].seed == S->seed)
            return sEvoCache[i].evos;
    c = &sEvoCache[sEvoCacheNext];
    sEvoCacheNext = (sEvoCacheNext + 1) % ARRAY_COUNT(sEvoCache);
    c->species = species;
    c->seed = S->seed;
    for (n = 0; n < EVO_MAX && vanilla[n].method != EVOLUTIONS_END; n++)
    {
        struct RhFilter f = {0};
        u16 target;
        c->evos[n] = vanilla[n];
        if (vanilla[n].targetSpecies == SPECIES_NONE)
            continue;
        f.legend = IsLegendarySpecies(species) ? 0 : 1;
        f.exclude[0] = species;
        if (S->evoSameType)
            f.type = GetSpeciesType(species, Hash(SALT_EVO, species, n + 100) & 1);
        if (S->evolutions == 2)
            target = PickSpecies(&f, Hash(SALT_EVO, species, n), vanilla[n].targetSpecies);
        else
        {
            f.minBst = VanillaBST(species);                  // evolving should never make a Pokemon weaker
            target = PickSpecies(&f, Hash(SALT_EVO, species, n), SPECIES_NONE);
        }
        if (target != SPECIES_NONE)
            c->evos[n].targetSpecies = target;
    }
    c->evos[n].method = EVOLUTIONS_END;
    c->evos[n].targetSpecies = SPECIES_NONE;
    return c->evos;
}

static u16 EvolveOnce(u16 species)
{
    const struct Evolution *e = GetSpeciesEvolutions(species);
    u32 i;
    for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        if (e[i].targetSpecies != SPECIES_NONE && e[i].targetSpecies != species)
            return e[i].targetSpecies;
    return SPECIES_NONE;
}

static u16 EvolveTimes(u16 species, u32 times)
{
    while (times--)
    {
        u16 next = EvolveOnce(species);
        if (next == SPECIES_NONE)
            break;
        species = next;
    }
    return species;
}

// ---------------------------------------------------------------------------
// Starters, statics, trades
// ---------------------------------------------------------------------------
static const u16 sVanillaStarters[3] = { SPECIES_BULBASAUR, SPECIES_SQUIRTLE, SPECIES_CHARMANDER };

static u16 StarterFor(u32 idx)
{
    u16 chosen[3] = {0};
    u32 i;
    for (i = 0; i <= idx; i++)
    {
        struct RhFilter f = {0};
        f.exclude[0] = chosen[0];
        f.exclude[1] = chosen[1];
        if (S->starters == 2)
        {
            f.threeStageBasic = TRUE;
            f.legend = 1;
        }
        chosen[i] = PickSpecies(&f, Hash(SALT_STARTER, i, 0), SPECIES_NONE);
        if (chosen[i] == SPECIES_NONE)
            chosen[i] = sVanillaStarters[i];
    }
    return chosen[idx];
}

enum Species RH_StarterSpecies(enum Species vanilla)
{
    u32 i;
    if (!S->enabled || S->starters == 0)
        return vanilla;
    for (i = 0; i < 3; i++)
        if (sVanillaStarters[i] == vanilla)
            return StarterFor(i);
    return vanilla;
}

enum Species RH_StaticSpecies(enum Species species)
{
    u32 i;
    struct RhFilter f = {0};
    if (!S->enabled || S->statics == 0)
        return species;
    if (gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB)
     && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB))
        return species;                                   // starters are handled separately
    for (i = 0; i < ARRAY_COUNT(sRhStaticSpecies); i++)
        if (sRhStaticSpecies[i] == species)
            break;
    if (i == ARRAY_COUNT(sRhStaticSpecies)
     && species != SPECIES_RAIKOU && species != SPECIES_ENTEI && species != SPECIES_SUICUNE)
        return species;
    if (S->statics == 1)
        f.legend = IsLegendarySpecies(species) ? 2 : 1;
    return PickSpecies(&f, Hash(SALT_STATIC, species, 0), SPECIES_NONE) ?: species;
}

enum Species RH_TradeSpecies(u32 tradeId, enum Species species, bool32 requested)
{
    struct RhFilter f = {0};
    if (!S->enabled || S->trades == 0 || (requested && S->trades < 2))
        return species;
    f.legend = 1;
    return PickSpecies(&f, Hash(SALT_TRADE, tradeId, requested), SPECIES_NONE) ?: species;
}

// ---------------------------------------------------------------------------
// Wild Pokemon
// ---------------------------------------------------------------------------
static u16 CurrentMapKey(void)
{
    return (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
}

enum Species RH_ModifyWildSpecies(enum Species species, enum WildPokemonArea area)
{
    struct RhFilter f = {0};
    enum Species safari = RH_SafariSpecies(species, area);
    u32 key;
    u16 similar = SPECIES_NONE;
    if (safari != species)
        return safari;                                    // the Safari generation setting wins
    if (!S->enabled || S->wild == 0 || species == SPECIES_NONE)
        return species;
    if (S->wildNoLegends)
        f.legend = 1;
    if (S->wildSimilar)
        similar = species;
    if (S->wildTypeThemed)
        f.type = RandomMonType(Hash(SALT_WILD_THEME, CurrentMapKey(), area));

    switch (S->wild)
    {
    case 1: // random every encounter
        key = Random32();
        break;
    case 2: // area 1-to-1
        key = Hash(SALT_WILD_AREA, CurrentMapKey() * 8 + area, species);
        break;
    default: // global 1-to-1
    {
        s32 idx = PoolIndexOf(species);
        if (idx >= 0 && !S->wildSimilar && !S->wildTypeThemed && !S->wildNoLegends)
        {
            u32 x = idx, guard;
            for (guard = 0; guard < 64; guard++)
            {
                x = Permute(SALT_GLOBAL_PERM, x, RH_POOL_COUNT);
                if (PoolAllowed(&sRhPool[x]))
                    return sRhPool[x].species;
            }
        }
        key = Hash(SALT_WILD, species, 0);
        break;
    }
    }
    return PickSpecies(&f, key, similar) ?: species;
}

u8 RH_ModifyWildLevel(u8 level)
{
    s32 l;
    if (!S->enabled || S->wildLevel == RH_LEVEL_MOD_DEFAULT || S->wildLevel >= ARRAY_COUNT(sLevelMods))
        return level;
    l = level * (100 + sLevelMods[S->wildLevel]) / 100;
    return (l < 1) ? 1 : (l > MAX_LEVEL ? MAX_LEVEL : l);
}

// ---------------------------------------------------------------------------
// Trainers
// ---------------------------------------------------------------------------
static s32 TrainerIdOf(const struct Trainer *trainer)
{
    const struct Trainer *base = &gTrainers[GetCurrentDifficultyLevel()][0];
    if (trainer >= base && trainer < base + TRAINERS_COUNT)
        return trainer - base;
    base = &gTrainers[0][0];
    if (trainer >= base && trainer < base + TRAINERS_COUNT)
        return trainer - base;
    return -1;
}

static bool32 IsRivalClass(u32 trainerClass)
{
    return trainerClass == TRAINER_CLASS_RIVAL_EARLY_FRLG || trainerClass == TRAINER_CLASS_RIVAL_LATE_FRLG
        || trainerClass == TRAINER_CLASS_CHAMPION_FRLG;
}

// Returns 0-2 for Bulbasaur/Squirtle/Charmander families (with stage in *stage), or -1.
static s32 StarterFamily(u16 species, u32 *stage)
{
    static const u16 sLines[3][3] = {
        { SPECIES_BULBASAUR, SPECIES_IVYSAUR, SPECIES_VENUSAUR },
        { SPECIES_SQUIRTLE, SPECIES_WARTORTLE, SPECIES_BLASTOISE },
        { SPECIES_CHARMANDER, SPECIES_CHARMELEON, SPECIES_CHARIZARD },
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

static u8 TrainerThemeType(s32 trainerId)
{
    u32 i;
    if (trainerId >= 0)
        for (i = 0; i < ARRAY_COUNT(sRhTrainerThemes); i++)
            if (sRhTrainerThemes[i].trainer == trainerId)
                return sRhTrainerThemes[i].type;
    return TYPE_NONE;
}

static u16 RandomHeldItem(u32 hash)
{
    u32 tries;
    for (tries = 0; tries < 16; tries++)
    {
        const struct RhItem *it = &sRhItems[Hash(SALT_HELD_ITEM, hash, tries) % RH_ITEM_COUNT];
        if (GetItemHoldEffect(it->item) != HOLD_EFFECT_NONE && GetItemPocket(it->item) != POCKET_KEY_ITEMS && !it->junk)
            return it->item;
    }
    return ITEM_NONE;
}

void RH_ModifyTrainerMon(struct TrainerMon *mon, const struct Trainer *trainer, u32 slot)
{
    s32 trainerId;
    u16 newSpecies = mon->species;
    u32 stage;
    s32 family;
    if (!S->enabled)
        return;
    trainerId = TrainerIdOf(trainer);

    if (S->trainerLevel != RH_LEVEL_MOD_DEFAULT && S->trainerLevel < ARRAY_COUNT(sLevelMods))
    {
        s32 l = mon->lvl * (100 + sLevelMods[S->trainerLevel]) / 100;
        mon->lvl = (l < 1) ? 1 : (l > MAX_LEVEL ? MAX_LEVEL : l);
    }

    family = StarterFamily(mon->species, &stage);
    if (S->rivalStarter && family >= 0 && IsRivalClass(trainer->trainerClass) && S->starters != 0)
    {
        newSpecies = EvolveTimes(StarterFor(family), stage);
    }
    else if (S->trainers != 0)
    {
        struct RhFilter f = {0};
        u16 similar = SPECIES_NONE;
        u32 key = Hash(SALT_TRAINER, trainerId >= 0 ? trainerId : (u32)trainer, slot * 1024 + mon->species);
        if (S->trainerNoLegends)
            f.legend = 1;
        switch (S->trainers)
        {
        case 2: similar = mon->species; break;
        case 3: f.type = RandomMonType(Hash(SALT_TRAINER_THEME, trainerId, 0)); break;
        case 4:
            f.type = TrainerThemeType(trainerId);
            similar = mon->species;
            break;
        }
        newSpecies = PickSpecies(&f, key, similar) ?: mon->species;
    }

    if (S->trainerForceEvolved)
    {
        static const u8 sThresholds[] = { 0, 30, 40, 50 };
        if (mon->lvl >= sThresholds[S->trainerForceEvolved])
            newSpecies = EvolveTimes(newSpecies, 3);
    }

    if (newSpecies != mon->species)
    {
        u32 i;
        mon->species = newSpecies;
        for (i = 0; i < MAX_MON_MOVES; i++)
            mon->moves[i] = MOVE_NONE;           // fall back to the new species' level-up moves
        mon->ability = ABILITY_NONE;
    }
    if (S->trainerItems && mon->heldItem == ITEM_NONE)
        mon->heldItem = RandomHeldItem(Hash(SALT_HELD_ITEM, trainerId, slot));
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------
static bool32 ItemIsProtected(u16 item)
{
    if (item == ITEM_NONE || GetItemPocket(item) == POCKET_KEY_ITEMS)
        return TRUE;
    if (GetItemTMHMIndex(item) > NUM_TECHNICAL_MACHINES)           // HMs (index is 1-based)
        return TRUE;
    return FALSE;
}

static u16 RandomPoolItem(u32 hash)
{
    u32 tries;
    for (tries = 0; tries < 32; tries++)
    {
        const struct RhItem *it = &sRhItems[Hash(SALT_FIELD_ITEM, hash, tries) % RH_ITEM_COUNT];
        if (S->banBadItems && it->junk)
            continue;
        return it->item;
    }
    return ITEM_POTION;
}

static u16 RandomTM(u32 hash)
{
    return GetTMHMItemId(Hash(SALT_FIELD_ITEM, hash, 7) % NUM_TECHNICAL_MACHINES);
}

enum Item RH_FieldItem(enum Item item, u32 flag)
{
    u32 i, x, guard;
    if (!S->enabled || S->fieldItems == 0 || ItemIsProtected(item))
        return item;
    if (S->fieldItems == 2)
        return (GetItemTMHMIndex(item) != 0) ? RandomTM(flag) : RandomPoolItem(flag);
    // Shuffle: move the game's own items around with a keyed permutation.
    for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
        if (sRhFieldItems[i].flag == flag)
            break;
    if (i == RH_FIELD_ITEM_COUNT)
        return item;
    x = i;
    for (guard = 0; guard < 64; guard++)
    {
        x = Permute(SALT_FIELD_ITEM, x, RH_FIELD_ITEM_COUNT);
        if (!ItemIsProtected(sRhFieldItems[x].item))
            return sRhFieldItems[x].item;
    }
    return item;
}

static bool32 IsShopEssential(u16 item)
{
    switch (item)
    {
    case ITEM_POKE_BALL: case ITEM_GREAT_BALL: case ITEM_ULTRA_BALL:
    case ITEM_POTION: case ITEM_SUPER_POTION: case ITEM_HYPER_POTION: case ITEM_MAX_POTION: case ITEM_FULL_RESTORE:
    case ITEM_REVIVE: case ITEM_ANTIDOTE: case ITEM_PARALYZE_HEAL: case ITEM_AWAKENING: case ITEM_BURN_HEAL:
    case ITEM_ICE_HEAL: case ITEM_FULL_HEAL: case ITEM_REPEL: case ITEM_SUPER_REPEL: case ITEM_MAX_REPEL:
    case ITEM_ESCAPE_ROPE:
        return TRUE;
    }
    return FALSE;
}

enum Item RH_ShopItem(enum Item item, u32 mart, u32 slot)
{
    u32 tries;
    if (!S->enabled || S->shopItems == 0 || IsShopEssential(item) || ItemIsProtected(item))
        return item;
    for (tries = 0; tries < 16; tries++)
    {
        u16 it = RandomPoolItem(Hash(SALT_SHOP, mart * 256 + slot, tries));
        if (GetItemPrice(it) != 0)
            return it;
    }
    return item;
}

// ---------------------------------------------------------------------------
// Script specials
// ---------------------------------------------------------------------------
// Oak's lab: VAR_TEMP_2 = player's starter species, VAR_TEMP_3 = rival's.
void RH_RemapStarterVars(void)
{
    VarSet(VAR_TEMP_2, RH_StarterSpecies(VarGet(VAR_TEMP_2)));
    VarSet(VAR_TEMP_3, RH_StarterSpecies(VarGet(VAR_TEMP_3)));
}

u16 RH_StartersRandomized(void)
{
    return S->enabled && S->starters != 0;
}

// Std_FindItem: VAR_0x8000 = item, VAR_LAST_TALKED = item ball local id.
void RH_RandomizeItemBall(void)
{
    const struct ObjectEventTemplate *t = GetObjectEventTemplateByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    if (t != NULL && t->flagId != 0)
        gSpecialVar_0x8000 = RH_FieldItem(gSpecialVar_0x8000, t->flagId);
}
