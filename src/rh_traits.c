// Pokemon Traits: base stats, EXP curves, types, abilities, evolutions (+ catch rate).
#include "global.h"
#include "constants/characters.h"
#include "pokemon.h"
#include "rh_internal.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/species.h"

// ---------------------------------------------------------------------------
// Base stats
// ---------------------------------------------------------------------------
struct StatCache { u16 species; u32 key; u8 v[NUM_STATS]; };
static EWRAM_DATA struct StatCache sStatCache[6] = {0};
static EWRAM_DATA u8 sStatCacheNext = 0;

static u32 SettingsKeyStats(void)
{
    return S->seed ^ (S->enabled << 1) ^ (S->baseStats << 2) ^ (S->baseStatsFollowEvos << 4) ^ (S->baseStatsRandomAdded << 5)
         ^ (RH_StatsGen() << 8) ^ 0x1234;
}

// Base stats of the chosen generation (gen 9 = the current data).
static void GenStats(u16 species, u8 *v)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    const u8 *old = RH_GenBaseStats(species, RH_StatsGen());
    if (old != NULL)
    {
        memcpy(v, old, NUM_STATS);
        return;
    }
    v[STAT_HP] = info->baseHP;
    v[STAT_ATK] = info->baseAttack;
    v[STAT_DEF] = info->baseDefense;
    v[STAT_SPEED] = info->baseSpeed;
    v[STAT_SPATK] = info->baseSpAttack;
    v[STAT_SPDEF] = info->baseSpDefense;
}

static u32 Total(const u8 *v)
{
    u32 i, t = 0;
    for (i = 0; i < NUM_STATS; i++)
        t += v[i];
    return t;
}

// Splits "amount" over the stats with random weights; stats in "fixed" are skipped.
static void Distribute(u32 amount, u32 key, u32 salt2, u8 *out, const u8 *add, u32 fixedMask)
{
    u32 w[NUM_STATS], total = 0, given = 0, i;
    for (i = 0; i < NUM_STATS; i++)
    {
        w[i] = (fixedMask & (1u << i)) ? 0 : 20 + RH_Hash(SALT_STATS, key, i + salt2 * 8) % 100;
        total += w[i];
    }
    for (i = 0; i < NUM_STATS; i++)
    {
        u32 share = total ? amount * w[i] / total : 0;
        u32 val = (add ? add[i] : 0) + share;
        given += share;
        out[i] = min(val, 255);
        if (!(fixedMask & (1u << i)) && out[i] < 5)
            out[i] = 5;
    }
    // hand out the rounding remainder
    for (i = 0; given < amount && i < NUM_STATS * 4; i++)
    {
        u32 s = RH_Hash(SALT_STATS, key, 100 + i) % NUM_STATS;
        if ((fixedMask & (1u << s)) || out[s] == 255)
            continue;
        out[s]++;
        given++;
    }
}

static void ComputeStats(u16 species, u8 *out, u32 depth)
{
    u8 base[NUM_STATS];
    u32 key, i, fixed = 0;
    GenStats(species, base);
    memcpy(out, base, NUM_STATS);
    if (!S->enabled || S->baseStats == 0)
        return;
    if (base[STAT_HP] == 1)
        fixed = 1u << STAT_HP;                               // Shedinja keeps 1 HP
    key = S->baseStatsFollowEvos ? RH_FamilyRoot(species) : species;
    if (S->baseStats == 1)
    {
        u8 perm[NUM_STATS] = {0, 1, 2, 3, 4, 5};
        for (i = NUM_STATS - 1; i > 0; i--)
        {
            u32 j = RH_Hash(SALT_STAT_PERM, key, i) % (i + 1);
            u8 t = perm[i]; perm[i] = perm[j]; perm[j] = t;
        }
        for (i = 0; i < NUM_STATS; i++)
            out[i] = base[perm[i]];
        if (fixed)
        {
            // keep HP at 1 and put the displaced value where HP's slot went
            for (i = 0; i < NUM_STATS; i++)
                if (perm[i] == STAT_HP)
                    out[i] = base[perm[STAT_HP]];
            out[STAT_HP] = 1;
        }
        return;
    }
    // Random: keep the base stat total.
    if (S->baseStatsFollowEvos && S->baseStatsRandomAdded && depth < 3 && RH_PreEvo(species) != SPECIES_NONE)
    {
        u8 pre[NUM_STATS];
        u8 preBase[NUM_STATS];
        u16 preEvo = RH_PreEvo(species);
        s32 diff;
        ComputeStats(preEvo, pre, depth + 1);
        GenStats(preEvo, preBase);
        diff = (s32)Total(base) - (s32)Total(preBase);
        if (diff > 0)
        {
            if (fixed)
                pre[STAT_HP] = 1;
            Distribute(diff, species, 1, out, pre, fixed);
            if (fixed)
                out[STAT_HP] = 1;
            return;
        }
    }
    if (fixed)
    {
        Distribute(Total(base) - 1, key, 0, out, NULL, fixed);
        out[STAT_HP] = 1;
        return;
    }
    Distribute(Total(base), key, 0, out, NULL, 0);
}

u32 RH_SpeciesBaseStat(enum Species species, u32 stat, u32 vanilla)
{
    u32 i, key;
    struct StatCache *c;
    if (species == SPECIES_NONE || species >= NUM_SPECIES || stat >= NUM_STATS)
        return vanilla;
    if ((!S->enabled || S->baseStats == 0) && RH_StatsGen() >= 9)
        return vanilla;
    key = SettingsKeyStats();
    for (i = 0; i < ARRAY_COUNT(sStatCache); i++)
        if (sStatCache[i].species == species && sStatCache[i].key == key)
            return sStatCache[i].v[stat];
    c = &sStatCache[sStatCacheNext];
    sStatCacheNext = (sStatCacheNext + 1) % ARRAY_COUNT(sStatCache);
    ComputeStats(species, c->v, 0);
    c->species = species;
    c->key = key;
    return c->v[stat];
}

// ---------------------------------------------------------------------------
// EXP curves
// ---------------------------------------------------------------------------
static const u8 sCurveOfSetting[] = { 0, GROWTH_MEDIUM_FAST, GROWTH_MEDIUM_SLOW, GROWTH_FAST, GROWTH_SLOW, GROWTH_ERRATIC, GROWTH_FLUCTUATING };

enum GrowthRate RH_SpeciesGrowthRate(enum Species species, enum GrowthRate vanilla)
{
    if (!S->enabled || S->expCurve == 0 || S->expCurve >= ARRAY_COUNT(sCurveOfSetting))
        return vanilla;
    switch (S->expCurveWho)
    {
    case 0:
        if (RH_IsLegendary(species))
            return GROWTH_SLOW;
        break;
    case 1:
        if (RH_IsLegendary(species) && RH_VanillaBST(species) > 600)
            return GROWTH_SLOW;
        break;
    }
    return sCurveOfSetting[S->expCurve];
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
enum Type RH_SpeciesType(enum Species species, u32 slot, enum Type vanilla)
{
    const struct SpeciesInfo *info;
    u32 key;
    u8 t1, t2;
    bool32 dual;
    if (!S->enabled || S->types == 0 || species == SPECIES_NONE || species >= NUM_SPECIES)
        return vanilla;
    info = &gSpeciesInfo[species];
    if (S->types == 1)
    {
        // Follow evolutions: the family shares its first type; evolutions that gain a second type in the base game
        // gain a (family-wide) random second type.
        key = RH_FamilyRoot(species);
    }
    else
    {
        key = species;
    }
    t1 = RH_RandomMonType(RH_Hash(SALT_TYPE1, key, 0));
    dual = S->forceDualTypes || info->types[0] != info->types[1];
    if (slot == 0 || !dual)
        return t1;
    t2 = RH_RandomMonType(RH_Hash(SALT_TYPE2, key, 0));
    if (t2 == t1)
        t2 = gRhMonTypes[(RH_TypeIndexOf(t1) + 1 + RH_Hash(SALT_TYPE2, key, 1) % 17) % 18];
    return t2;
}

// ---------------------------------------------------------------------------
// Abilities
// ---------------------------------------------------------------------------
// Abilities with the same effect: only the first of each group is picked directly; the group's members are then
// chosen evenly ("Combine Duplicate Abilities").
static const u16 sDuplicateGroups[][4] = {
    { ABILITY_INSOMNIA, ABILITY_VITAL_SPIRIT },
    { ABILITY_CLEAR_BODY, ABILITY_WHITE_SMOKE, ABILITY_FULL_METAL_BODY },
    { ABILITY_HUGE_POWER, ABILITY_PURE_POWER },
    { ABILITY_BATTLE_ARMOR, ABILITY_SHELL_ARMOR },
    { ABILITY_CLOUD_NINE, ABILITY_AIR_LOCK },
    { ABILITY_FILTER, ABILITY_SOLID_ROCK, ABILITY_PRISM_ARMOR },
    { ABILITY_ROUGH_SKIN, ABILITY_IRON_BARBS },
    { ABILITY_MOLD_BREAKER, ABILITY_TURBOBLAZE, ABILITY_TERAVOLT },
    { ABILITY_WIMP_OUT, ABILITY_EMERGENCY_EXIT },
    { ABILITY_QUEENLY_MAJESTY, ABILITY_DAZZLING, ABILITY_ARMOR_TAIL },
    { ABILITY_GOOEY, ABILITY_TANGLING_HAIR },
    { ABILITY_RECEIVER, ABILITY_POWER_OF_ALCHEMY },
    { ABILITY_MULTISCALE, ABILITY_SHADOW_SHIELD },
};

static bool32 IsNonFirstDuplicate(u32 ability)
{
    u32 i, j;
    for (i = 0; i < ARRAY_COUNT(sDuplicateGroups); i++)
        for (j = 1; j < 4; j++)
            if (sDuplicateGroups[i][j] == ability)
                return TRUE;
    return FALSE;
}

static u32 ExpandDuplicate(u32 ability, u32 hash)
{
    u32 i, n;
    for (i = 0; i < ARRAY_COUNT(sDuplicateGroups); i++)
    {
        if (sDuplicateGroups[i][0] != ability)
            continue;
        for (n = 1; n < 4 && sDuplicateGroups[i][n] != ABILITY_NONE; n++)
            ;
        return sDuplicateGroups[i][hash % n];
    }
    return ability;
}

static bool32 AbilityAllowed(u32 ability)
{
    if (ability == ABILITY_NONE || ability >= ABILITIES_COUNT)
        return FALSE;
    switch (ability)
    {
    case ABILITY_WONDER_GUARD:
        return S->allowWonderGuard;
    case ABILITY_ARENA_TRAP: case ABILITY_SHADOW_TAG: case ABILITY_MAGNET_PULL:
        return !S->banTrapAbilities;
    case ABILITY_DEFEATIST: case ABILITY_SLOW_START: case ABILITY_TRUANT: case ABILITY_KLUTZ: case ABILITY_STALL:
        return !S->banNegativeAbilities;
    case ABILITY_MINUS: case ABILITY_PLUS: case ABILITY_ANTICIPATION: case ABILITY_FOREWARN: case ABILITY_FRISK:
    case ABILITY_HONEY_GATHER: case ABILITY_AURA_BREAK: case ABILITY_RECEIVER: case ABILITY_POWER_OF_ALCHEMY:
        return !S->banBadAbilities;
    case ABILITY_FRIEND_GUARD: case ABILITY_HEALER: case ABILITY_TELEPATHY: case ABILITY_SYMBIOSIS: case ABILITY_BATTERY:
        return !S->banBadAbilities || (S->battleStyle == 2 && S->battleStyleDoubles);
    case ABILITY_FORECAST: case ABILITY_FLOWER_GIFT: case ABILITY_ZEN_MODE: case ABILITY_ICE_FACE: case ABILITY_HUNGER_SWITCH:
    case ABILITY_GULP_MISSILE: case ABILITY_COMMANDER: case ABILITY_ZERO_TO_HERO: case ABILITY_POWER_CONSTRUCT:
    case ABILITY_SCHOOLING: case ABILITY_SHIELDS_DOWN: case ABILITY_DISGUISE: case ABILITY_BATTLE_BOND:
    case ABILITY_TERA_SHIFT: case ABILITY_TERA_SHELL: case ABILITY_TERAFORM_ZERO:
        return FALSE;
    }
    if (gAbilitiesInfo[ability].cantBeSwapped)
        return FALSE;                                        // form / signature abilities (Multitype, Stance Change...)
    if (S->combineDuplicateAbilities && IsNonFirstDuplicate(ability))
        return FALSE;
    return TRUE;
}

static u32 RandomAbility(u32 key, u32 slot)
{
    u32 tries;
    for (tries = 0; tries < 64; tries++)
    {
        u32 h = RH_Hash(SALT_ABILITY, key, slot * 64 + tries);
        u32 a = 1 + h % (ABILITIES_COUNT - 1);
        if (AbilityAllowed(a))
            return S->combineDuplicateAbilities ? ExpandDuplicate(a, h >> 16) : a;
    }
    return ABILITY_RUN_AWAY;
}

enum Ability RH_SpeciesAbility(enum Species species, u32 slot, enum Ability vanilla)
{
    const struct SpeciesInfo *info;
    u32 key, a, other;
    if (!S->enabled || S->abilities == 0 || species >= NUM_SPECIES || species == SPECIES_NONE || slot > 2)
        return vanilla;
    info = &gSpeciesInfo[species];
    if (info->abilities[0] == ABILITY_WONDER_GUARD)
        return vanilla;                                      // Shedinja keeps Wonder Guard (FVX)
    if (vanilla == ABILITY_NONE && !(slot == 1 && S->ensureTwoAbilities))
        return ABILITY_NONE;
    key = S->abilitiesFollowEvos ? RH_FamilyRoot(species) : species;
    a = RandomAbility(key, slot);
    // keep the slots different from each other
    for (other = 0; other < 3; other++)
    {
        u32 tries = 0;
        if (other == slot)
            continue;
        if (other > slot)
            break;
        while (tries++ < 8 && a == RandomAbility(key, other))
            a = RandomAbility(key, slot + 3 * tries);
    }
    return a;
}

// ---------------------------------------------------------------------------
// Evolutions
// ---------------------------------------------------------------------------
#define EVO_MAX 12
#define EVO_PARAMS 4
struct EvoCache
{
    u16 species;
    u32 key;
    struct Evolution evos[EVO_MAX + 1];
    struct EvolutionParam params[EVO_MAX][EVO_PARAMS];
};
static EWRAM_DATA struct EvoCache sEvoCache[12] = {0};
static EWRAM_DATA u8 sEvoCacheNext = 0;
static EWRAM_DATA u16 sEvoSource = 0;                          // species being evolved (for filter callbacks)
static EWRAM_DATA u8 sEvoGrowth = 0;

static u32 SettingsKeyEvos(void)
{
    return S->seed ^ (S->evolutions << 1) ^ (S->evoSimilarStrength << 3) ^ (S->evoSameTyping << 4) ^ (S->evoLimitThreeStages << 5)
         ^ (S->evoNoConvergence << 6) ^ (S->evoForceChange << 7) ^ (S->evoForceGrowth << 8) ^ (S->evoChangeImpossible << 9)
         ^ (S->evoMakeEasier << 10) ^ (S->evoEstimatedLevels << 17) ^ (S->evoRemoveTimeBased << 18) ^ (S->types << 19)
         ^ (S->expCurve << 21) ^ (S->movesets << 24) ^ (S->speciesPool << 26) ^ (S->enabled << 29);
}

static bool32 EvosActive(void)
{
    return S->enabled && (S->evolutions || S->evoChangeImpossible || S->evoMakeEasier || S->evoRemoveTimeBased);
}

static u32 Remaining(u16 species)
{
    u32 st = RH_SpeciesStage(species), ch = RH_SpeciesChain(species);
    return ch > st ? ch - st : 0;
}

static bool32 SameGrowth(u16 species)
{
    return GetSpeciesGrowthRate(species) == sEvoGrowth;
}

static bool32 EvoTargetOk(u16 species)
{
    if (!SameGrowth(species))
        return FALSE;
    if (S->evoSameTyping && !RH_SpeciesHasType(species, GetSpeciesType(sEvoSource, 0)) && !RH_SpeciesHasType(species, GetSpeciesType(sEvoSource, 1)))
        return FALSE;
    return TRUE;
}

// Class used by "No Convergence": a keyed bijection within each class keeps every target unique.
static u32 ConvergenceClass(u16 species)
{
    u32 c = GetSpeciesGrowthRate(species);
    if (S->evoLimitThreeStages)
        c |= Remaining(species) << 4;
    c |= RH_IsLegendary(species) << 6;
    if (S->evoSimilarStrength || S->evoForceGrowth)
        c |= (RH_VanillaBST(species) / 75) << 8;
    if (S->evoSameTyping)
        c |= GetSpeciesType(species, 0) << 16;
    return c;
}

static u16 NoConvergenceTarget(u16 original)
{
    s32 x = RH_PoolIndexOf(original);
    u32 cls, guard;
    if (x < 0)
        return SPECIES_NONE;
    cls = ConvergenceClass(original);
    for (guard = 0; guard < 4096; guard++)
    {
        x = RH_Permute(SALT_EVO, x, RH_PoolCount());
        if (RH_PoolAllowed(x) && ConvergenceClass(RH_PoolSpecies(x)) == cls)
        {
            u16 target = RH_PoolSpecies(x);
            if (target == original && S->evoForceChange)
                continue;
            return target;
        }
    }
    return SPECIES_NONE;
}

static u16 RandomEvoTarget(u16 species, u16 original, u32 n, const u16 *chosen, u32 chosenCount)
{
    struct RhFilter f = {0};
    u32 i;
    if (S->evoNoConvergence && original != SPECIES_NONE)
    {
        u16 t = NoConvergenceTarget(original);
        if (t != SPECIES_NONE)
            return t;
    }
    sEvoSource = species;
    sEvoGrowth = GetSpeciesGrowthRate(species);
    f.extra = EvoTargetOk;
    f.legend = (RH_IsLegendary(species) || (original && RH_IsLegendary(original))) ? 0 : 1;
    RH_FilterExclude(&f, species);
    for (i = 0; i < chosenCount; i++)
        RH_FilterExclude(&f, chosen[i]);
    if (S->evoForceChange && original != SPECIES_NONE)
        RH_FilterExclude(&f, original);
    if (S->evoForceGrowth)
        f.minBst = RH_VanillaBST(species) + 1;
    return RH_PickSpecies(&f, RH_Hash(SALT_EVO, species, n), S->evoSimilarStrength ? (original ? original : species) : SPECIES_NONE);
}

// "Limit to three stages" needs the remaining-stage count of the candidate; checked via a second predicate.
static EWRAM_DATA u8 sEvoRemaining = 0;
static bool32 EvoTargetOkRemaining(u16 species)
{
    return EvoTargetOk(species) && Remaining(species) == sEvoRemaining;
}

static u16 RandomEvoTargetLimited(u16 species, u16 original, u32 n, const u16 *chosen, u32 chosenCount)
{
    struct RhFilter f = {0};
    u32 i;
    u16 result;
    if (!S->evoLimitThreeStages || S->evoNoConvergence || original == SPECIES_NONE)
        return RandomEvoTarget(species, original, n, chosen, chosenCount);
    sEvoSource = species;
    sEvoGrowth = GetSpeciesGrowthRate(species);
    sEvoRemaining = Remaining(original);
    f.extra = EvoTargetOkRemaining;
    f.legend = (RH_IsLegendary(species) || RH_IsLegendary(original)) ? 0 : 1;
    RH_FilterExclude(&f, species);
    for (i = 0; i < chosenCount; i++)
        RH_FilterExclude(&f, chosen[i]);
    if (S->evoForceChange)
        RH_FilterExclude(&f, original);
    if (S->evoForceGrowth)
        f.minBst = RH_VanillaBST(species) + 1;
    result = RH_PickSpecies(&f, RH_Hash(SALT_EVO, species, n), S->evoSimilarStrength ? original : SPECIES_NONE);
    return result;
}

static u32 EstimatedLevel(u16 target)
{
    s32 bst = RH_VanillaBST(target);
    s32 level = 18 + (bst - 400) * 138 / 1000;
    if (level < 10) level = 10;
    if (level > 55) level = 55;
    return level;
}

static u32 ImpossibleLevel(u16 target)
{
    return S->evoEstimatedLevels ? EstimatedLevel(target) : 37;
}

static bool32 ConditionImpossible(const struct EvolutionParam *p)
{
    switch (p->condition)
    {
    case IF_IN_MAP: case IF_IN_MAPSEC: case IF_MIN_BEAUTY: case IF_MIN_COOLNESS: case IF_MIN_SMARTNESS:
    case IF_MIN_TOUGHNESS: case IF_MIN_CUTENESS: case IF_TRADE_PARTNER_SPECIES: case IF_BAG_ITEM_COUNT:
    case IF_DEFEAT_X_WITH_ITEMS:
        return TRUE;
    case IF_KNOWS_MOVE: case IF_KNOWS_MOVE_TYPE:
        return S->movesets != 0;                             // the move may never be learned
    default:
        return FALSE;
    }
}

static bool32 IsTimeCondition(u16 c)
{
    return c == IF_TIME || c == IF_NOT_TIME;
}

static u16 StoneForTime(const struct EvolutionParam *params)
{
    u32 i;
    for (i = 0; params != NULL && params[i].condition != CONDITIONS_END && i < EVO_PARAMS; i++)
    {
        if (params[i].condition == IF_TIME)
        {
            switch (params[i].arg1)
            {
            case TIME_NIGHT:   return ITEM_MOON_STONE;
            case TIME_EVENING: return ITEM_DUSK_STONE;
            default:           return ITEM_SUN_STONE;
            }
        }
        if (params[i].condition == IF_NOT_TIME)
            return ITEM_MOON_STONE;
    }
    return ITEM_SUN_STONE;
}

static bool32 HasTimeCondition(const struct EvolutionParam *params)
{
    u32 i;
    for (i = 0; params != NULL && params[i].condition != CONDITIONS_END && i < EVO_PARAMS; i++)
        if (IsTimeCondition(params[i].condition))
            return TRUE;
    return FALSE;
}

// Rewrites one evolution entry for "Change Impossible Evolutions", "Make Evolutions Easier" and
// "Remove Time-Based Evolutions". *timeSplitSeen tracks split time evolutions (Eevee, Rockruff...).
static void AdjustEvolution(u16 species, struct Evolution *e, struct EvolutionParam *buf, u32 *timeSplitSeen)
{
    const struct EvolutionParam *src = e->params;
    u32 i, n = 0;
    bool32 toLevel = FALSE;
    u16 toItem = ITEM_NONE;

    if (e->method == EVO_NONE || e->targetSpecies == SPECIES_NONE)
        return;
    for (i = 0; src != NULL && src[i].condition != CONDITIONS_END && n < EVO_PARAMS - 1; i++)
    {
        struct EvolutionParam p = src[i];
        if (S->evoChangeImpossible && ConditionImpossible(&p))
        {
            toLevel = TRUE;
            continue;
        }
        if (S->evoMakeEasier)
        {
            if (p.condition == IF_MIN_FRIENDSHIP && p.arg1 > 160)
                p.arg1 = 160;
            if (p.condition == IF_SPECIES_IN_PARTY || p.condition == IF_TYPE_IN_PARTY)
            {
                e->method = EVO_LEVEL;
                e->param = 35;
                continue;
            }
        }
        if (S->evoRemoveTimeBased && IsTimeCondition(p.condition))
            continue;
        if (S->evoChangeImpossible && e->method == EVO_TRADE && p.condition == IF_HOLD_ITEM)
        {
            toItem = p.arg1;                                 // trade holding X -> use X like a stone
            continue;
        }
        buf[n++] = p;
    }
    buf[n].condition = CONDITIONS_END;

    if (S->evoRemoveTimeBased && HasTimeCondition(src))
    {
        // the first time-based evolution keeps its method, later split ones become stone evolutions
        if ((*timeSplitSeen)++ > 0)
        {
            e->method = EVO_ITEM;
            e->param = StoneForTime(src);
            n = 0;
            buf[0].condition = CONDITIONS_END;
        }
    }
    if (S->evoChangeImpossible)
    {
        switch (e->method)
        {
        case EVO_TRADE:
            if (toItem != ITEM_NONE)
            {
                e->method = EVO_ITEM;
                e->param = toItem;
            }
            else
            {
                toLevel = TRUE;
            }
            break;
        case EVO_SCRIPT_TRIGGER:
        case EVO_SPIN:
            toLevel = TRUE;
            break;
        }
        if (toLevel)
        {
            e->method = EVO_LEVEL;
            e->param = ImpossibleLevel(e->targetSpecies);
        }
    }
    if (S->evoMakeEasier && S->evoMakeEasier < 55 && (e->method == EVO_LEVEL || e->method == EVO_LEVEL_BATTLE_ONLY))
    {
        u32 cap = S->evoMakeEasier;
        if (Remaining(e->targetSpecies) >= 1)
            cap = cap * 3 / 4;                               // three-stage lines reach the middle stage by 75%
        if (e->param > cap)
            e->param = cap;
    }
    e->params = (n > 0) ? buf : NULL;
}

const struct Evolution *RH_Evolutions(enum Species species, const struct Evolution *vanilla)
{
    u32 i, n, key, timeSplit = 0;
    struct EvoCache *c;
    u16 chosen[EVO_MAX];
    if (!EvosActive() || species == SPECIES_NONE || species >= NUM_SPECIES)
        return vanilla;
    if (S->evolutions != 2 && (vanilla == NULL || vanilla[0].method == EVOLUTIONS_END))
        return vanilla;
    key = SettingsKeyEvos();
    for (i = 0; i < ARRAY_COUNT(sEvoCache); i++)
        if (sEvoCache[i].species == species && sEvoCache[i].key == key)
            return sEvoCache[i].evos;
    c = &sEvoCache[sEvoCacheNext];
    sEvoCacheNext = (sEvoCacheNext + 1) % ARRAY_COUNT(sEvoCache);
    c->species = SPECIES_NONE;                               // invalid while being rebuilt

    if (S->evolutions == 2)
    {
        // Random Every Level: one level-1 evolution into a random Pokemon.
        u16 target = RandomEvoTarget(species, SPECIES_NONE, 0, NULL, 0);
        n = 0;
        if (target != SPECIES_NONE)
        {
            c->evos[0].method = EVO_LEVEL;
            c->evos[0].param = 1;
            c->evos[0].targetSpecies = target;
            c->evos[0].params = NULL;
            n = 1;
        }
    }
    else
    {
        for (n = 0; n < EVO_MAX && vanilla[n].method != EVOLUTIONS_END; n++)
        {
            c->evos[n] = vanilla[n];
            chosen[n] = SPECIES_NONE;
            if (S->evolutions == 1 && vanilla[n].method != EVO_NONE && vanilla[n].targetSpecies != SPECIES_NONE)
            {
                u16 target = RandomEvoTargetLimited(species, vanilla[n].targetSpecies, n, chosen, n);
                if (target != SPECIES_NONE)
                    c->evos[n].targetSpecies = target;
                chosen[n] = c->evos[n].targetSpecies;
            }
            AdjustEvolution(species, &c->evos[n], c->params[n], &timeSplit);
        }
    }
    c->evos[n].method = EVOLUTIONS_END;
    c->evos[n].param = 0;
    c->evos[n].targetSpecies = SPECIES_NONE;
    c->evos[n].params = NULL;
    c->species = species;
    c->key = key;
    return c->evos;
}

// ---------------------------------------------------------------------------
// Catch rate ("Set Minimum Catch Rate", FVX levels)
// ---------------------------------------------------------------------------
u8 RH_CatchRate(enum Species species, u8 vanilla)
{
    static const u8 sNormal[] = { 0, 75, 128, 200, 255, 255 };
    static const u8 sLegend[] = { 0, 37, 64, 100, 255, 255 };
    u32 level;
    if (!S->wildCatchRateOn)
        return vanilla;
    level = min(S->wildCatchRate, 5);
    return max(vanilla, RH_IsLegendary(species) ? sLegend[level] : sNormal[level]);
}

bool32 RH_GuaranteedCatch(void)
{
    return S->wildCatchRateOn && S->wildCatchRate >= 5;
}
