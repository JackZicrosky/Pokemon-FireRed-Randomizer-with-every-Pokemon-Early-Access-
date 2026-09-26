// Pokemon Traits: base stats, EXP curves, types, abilities, evolutions (+ catch rate).
#include "global.h"
#include "malloc.h"
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

// Splits "amount" over the stats with random weights on top of "add"; stats in "fixed" are skipped.
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
    key = S->baseStatsFollowEvos ? RH_TraitRoot(species) : species;
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
    // FVX: HP at least 20, other stats at least 10, the rest of the total split randomly.
    {
        static const u8 sFloor[NUM_STATS] = { 20, 10, 10, 10, 10, 10 };
        u8 floor[NUM_STATS];
        u32 reserved = 0, total = Total(base);
        memcpy(floor, sFloor, NUM_STATS);
        if (fixed)
            floor[STAT_HP] = 1;
        for (i = 0; i < NUM_STATS; i++)
            reserved += floor[i];
        Distribute(total > reserved ? total - reserved : 0, key, 0, out, floor, fixed);
        if (fixed)
            out[STAT_HP] = 1;
    }
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
static u16 FinalStage(u16 species)
{
    u32 g;
    for (g = 0; g < 3; g++)
    {
        const struct Evolution *e = GetSpeciesEvolutionsVanilla(species);
        if (e == NULL || e[0].method == EVOLUTIONS_END || e[0].targetSpecies == SPECIES_NONE)
            break;
        species = e[0].targetSpecies;
    }
    return species;
}

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
        if (RH_IsLegendary(species) && RH_VanillaBST(FinalStage(species)) > 600)
            return GROWTH_SLOW;                              // judged by the final stage (Cosmog -> Solgaleo)
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
        key = RH_TraitRoot(species);
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
    { ABILITY_PROTEAN, ABILITY_LIBERO },
    { ABILITY_PROPELLER_TAIL, ABILITY_STALWART },
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
    key = S->abilitiesFollowEvos ? RH_TraitRoot(species) : species;
    {
        // all three slots are rolled together so they always differ from each other
        u16 abil[3];
        u32 s, t;
        for (s = 0; s <= slot; s++)
        {
            abil[s] = RandomAbility(key, s);
            for (t = 1; t < 12; t++)
            {
                bool32 clash = FALSE;
                for (other = 0; other < s; other++)
                    clash |= (abil[s] == abil[other]);
                if (!clash)
                    break;
                abil[s] = RandomAbility(key, s + 3 * t);
            }
        }
        a = abil[slot];
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

static u32 SettingsKeyEvos(void)
{
    return RH_SettingsHash() | 1;
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

// Evolutions only ever go "down" this ranking, so random evolutions can never loop (FVX never creates cycles).
static u32 EvoRank(u16 species)
{
    // stages left first, then a keyed order (a plain hash: this runs for every candidate of every pick)
    return (Remaining(species) << 28) | ((RH_Hash(SALT_EVO_RANK, species, 0) & 0xFFFF) << 12) | (species & 0xFFF);
}

// Context of the evolution being randomized (read by the filter callbacks).
static EWRAM_DATA u16 sEvoSource = 0;
static EWRAM_DATA u8 sEvoGrowth = 0;
static EWRAM_DATA u32 sEvoSourceRank = 0;
static EWRAM_DATA u16 sEvoMinBst = 0;
static EWRAM_DATA s8 sEvoRemaining = 0;

static EWRAM_DATA bool8 sEvoRelaxed = FALSE;                  // No Convergence last resort: skip typing / growth

static bool32 EvoTargetOk(u16 species)
{
    if (GetSpeciesGrowthRate(species) != sEvoGrowth)
        return FALSE;                                        // FVX: evolutions keep the EXP curve
    if (!sEvoRelaxed && S->evoSameTyping && !RH_SpeciesHasType(species, GetSpeciesType(sEvoSource, 0)) && !RH_SpeciesHasType(species, GetSpeciesType(sEvoSource, 1)))
        return FALSE;
    if (!sEvoRelaxed && sEvoMinBst && RH_VanillaBST(species) < sEvoMinBst)
        return FALSE;
    if (sEvoRemaining >= 0 && (s32)Remaining(species) != sEvoRemaining)
        return FALSE;
    if (S->evolutions == 1 && EvoRank(species) >= sEvoSourceRank)
        return FALSE;
    return TRUE;
}

static void SetEvoContext(u16 species, u16 original)
{
    sEvoSource = species;
    sEvoGrowth = GetSpeciesGrowthRate(species);
    sEvoSourceRank = EvoRank(species);
    // Similar Strength / Limit to Three Stages / Force Growth only apply to "Random" (not "Every Level"), as in FVX
    sEvoMinBst = (S->evoForceGrowth && S->evolutions == 1) ? RH_VanillaBST(species) + 1 : 0;
    sEvoRemaining = (S->evoLimitThreeStages && S->evolutions == 1 && original != SPECIES_NONE) ? (s8)Remaining(original) : -1;
}

static void ExcludeVanillaTargets(struct RhFilter *f, u16 species)
{
    const struct Evolution *v = GetSpeciesEvolutionsVanilla(species);
    u32 i;
    for (i = 0; v != NULL && v[i].method != EVOLUTIONS_END; i++)
        RH_FilterExclude(f, v[i].targetSpecies);
}

static bool32 IsVanillaTarget(u16 species, u16 target)
{
    const struct Evolution *v = GetSpeciesEvolutionsVanilla(species);
    u32 i;
    for (i = 0; v != NULL && v[i].method != EVOLUTIONS_END; i++)
        if (v[i].targetSpecies == target)
            return TRUE;
    return FALSE;
}

// "No Convergence": no two evolutions share a target. Candidates are grouped by what doesn't depend on the source
// (EXP curve, and the stages left with "Limit to Three Stages"). The original targets of a group are numbered
// 0..m-1, and target number idx may only use candidate positions idx, idx+m, idx+2m... of a keyed order of the
// group, so different original targets can never land on the same Pokemon, even when some candidates fail the
// other rules. (Every Level: every Pokemon of the group is a source, so the map is a bijection.)
#ifndef RELEASE
s32 gRhDebugNoConv[4];
#endif

static bool32 InEvoGroup(u16 sp)
{
    return GetSpeciesGrowthRate(sp) == sEvoGrowth && (sEvoRemaining < 0 || (s32)Remaining(sp) == sEvoRemaining);
}

static bool32 IsGroupSource(u16 sp)
{
    u16 pre;
    if (S->evolutions == 2)
        return InEvoGroup(sp);
    pre = RH_PreEvo(sp);                                     // an original evolution target of this group
    return pre != SPECIES_NONE && GetSpeciesGrowthRate(pre) == sEvoGrowth
        && (sEvoRemaining < 0 || (s32)Remaining(sp) == sEvoRemaining);
}

static bool32 NoConvergenceOk(u16 species, u16 target)
{
    if (target == SPECIES_NONE || target == species)
        return FALSE;
    if (S->evoForceChange && IsVanillaTarget(species, target))
        return FALSE;
    return EvoTargetOk(target);
}

static u16 NoConvergenceTarget(u16 species, u16 original)
{
    struct RhFilter any = {0};
    u16 v = (S->evolutions == 2) ? species : original;
    u32 p, n = RH_PoolCount(), m = 0, c = 0, attempt, pos;
    s32 idx = -1;
    u16 *cand, result = SPECIES_NONE;
    // Random: the new target has as many stages left as the original one. That keeps every chain going "down"
    // (no loops without the ranking rule, which would reject half of the few positions a target may use).
    if (S->evolutions == 1)
        sEvoRemaining = Remaining(original);
    cand = AllocUnchecked(n * sizeof(u16));
    if (cand == NULL)
        return SPECIES_NONE;
    for (p = 0; p < n; p++)
    {
        u16 sp;
        if (!RH_FilterAccepts(&any, p))
            continue;
        sp = RH_PoolSpecies(p);
        if (sp == v)
            idx = m;
        if (IsGroupSource(sp))
            m++;
        if (InEvoGroup(sp))
            cand[c++] = sp;
    }
#ifndef RELEASE
    gRhDebugNoConv[0] = idx; gRhDebugNoConv[1] = m; gRhDebugNoConv[2] = c;
#endif
    if (S->evolutions == 2 && idx >= 0 && c > 1)
    {
        // Every Level: the group in a keyed order is one big cycle, each Pokemon evolving into the next one
        // (a bijection without fixed points)
        u32 j;
        for (j = 0; j < c && RH_Permute(SALT_EVO, j, c) != (u32)idx; j++)
            ;
        for (attempt = 1; attempt < c && result == SPECIES_NONE && attempt < 4; attempt++)
        {
            u16 t = cand[RH_Permute(SALT_EVO, (j + attempt) % c, c)];
            if (NoConvergenceOk(species, t))
                result = t;
        }
    }
    else if (idx >= 0 && m != 0 && c != 0)
    {
        // this target's own positions: idx, idx + m, idx + 2m...
        for (attempt = 0; idx + attempt * m < c && result == SPECIES_NONE; attempt++)
        {
            u16 t = cand[RH_Permute(SALT_EVO, idx + attempt * m, c)];
            if (NoConvergenceOk(species, t))
                result = t;
        }
        // none fits the other rules: spare positions (m and up are only second tries of other targets), each
        // target starting somewhere else so they don't all take the same one
        for (attempt = 0; c > m && attempt < min(c - m, 128u) && result == SPECIES_NONE; attempt++)
        {
            u16 t;
            pos = m + (idx * 37 + attempt) % (c - m);
            t = cand[RH_Permute(SALT_EVO, pos, c)];
            if (NoConvergenceOk(species, t))
                result = t;
        }
    }
    Free(cand);
    return result;
}

static u16 RandomEvoTarget(u16 species, u16 original, u32 n, const u16 *chosen, u32 chosenCount)
{
    struct RhFilter f = {0};
    u32 i;
    SetEvoContext(species, original);
    if (S->evoNoConvergence)
    {
        u16 t = NoConvergenceTarget(species, original);
        if (t != SPECIES_NONE)
            return t;
        SetEvoContext(species, original);                    // rare: no free position fits -> a normal pick
    }
    f.extra = EvoTargetOk;
    RH_FilterExclude(&f, species);
    for (i = 0; i < chosenCount; i++)
        RH_FilterExclude(&f, chosen[i]);
    if (S->evoForceChange)
        ExcludeVanillaTargets(&f, species);
    {
        u16 t = RH_PickSpecies(&f, RH_Hash(SALT_EVO, species, n), (S->evoSimilarStrength && S->evolutions == 1 && original) ? original : SPECIES_NONE);
        // RH_PickSpecies relaxes its rules when nothing fits; never accept a pick that could create a loop
        if (t == species || (t != SPECIES_NONE && S->evolutions == 1 && EvoRank(t) >= sEvoSourceRank))
            return SPECIES_NONE;
        if (t != SPECIES_NONE && !EvoTargetOk(t))
            return SPECIES_NONE;                             // the pick had to drop the rules: keep the original
        return t;
    }
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
    case IF_MIN_TOUGHNESS: case IF_MIN_CUTENESS: case IF_TRADE_PARTNER_SPECIES: case IF_DEFEAT_X_WITH_ITEMS:
        return TRUE;
    case IF_KNOWS_MOVE: case IF_KNOWS_MOVE_TYPE:
        return S->movesets != 0;                             // the move may never be learned
    default:
        return FALSE;
    }
}

static bool32 HasImpossibleCondition(const struct Evolution *e)
{
    u32 i;
    for (i = 0; e->params != NULL && e->params[i].condition != CONDITIONS_END && i < EVO_PARAMS; i++)
        if (ConditionImpossible(&e->params[i]))
            return TRUE;
    return e->method == EVO_TRADE || e->method == EVO_SCRIPT_TRIGGER || e->method == EVO_SPIN;
}

enum { TIME_KIND_NONE, TIME_KIND_DAY, TIME_KIND_NIGHT, TIME_KIND_DUSK };

static u32 TimeKind(const struct EvolutionParam *params)
{
    u32 i;
    for (i = 0; params != NULL && params[i].condition != CONDITIONS_END && i < EVO_PARAMS; i++)
    {
        if (params[i].condition == IF_TIME)
        {
            if (params[i].arg1 == TIME_EVENING)
                return TIME_KIND_DUSK;
            return params[i].arg1 == TIME_NIGHT ? TIME_KIND_NIGHT : TIME_KIND_DAY;
        }
        if (params[i].condition == IF_NOT_TIME)
            return params[i].arg1 == TIME_NIGHT ? TIME_KIND_DAY : TIME_KIND_NIGHT;
    }
    return TIME_KIND_NONE;
}

static bool32 IsTimeCondition(u16 c)
{
    return c == IF_TIME || c == IF_NOT_TIME;
}

// Rewrites one evolution entry for "Change Impossible Evolutions", "Make Evolutions Easier" and
// "Remove Time-Based Evolutions" (FVX rules). "all" is the species' vanilla evolution list, k this entry's index.
static void AdjustEvolution(struct Evolution *e, struct EvolutionParam *buf, const struct Evolution *all, u32 k)
{
    const struct EvolutionParam *src = all[k].params;
    u32 i, n = 0, timeKind = TimeKind(src);
    bool32 toLevel = FALSE, toStone = FALSE;
    u16 toItem = ITEM_NONE;

    if (e->method == EVO_NONE || e->targetSpecies == SPECIES_NONE)
        return;

    if (S->evoChangeImpossible && HasImpossibleCondition(&all[k]))
    {
        // Another, possible entry already leads to the same Pokemon (Eevee's Leafeon/Glaceon stones,
        // Magnezone's Thunder Stone...): just drop this one.
        for (i = 0; all[i].method != EVOLUTIONS_END; i++)
        {
            if (i != k && all[i].targetSpecies == all[k].targetSpecies && !HasImpossibleCondition(&all[i]))
            {
                e->method = EVO_NONE;
                return;
            }
        }
    }

    // Time-based: dusk -> Dusk Stone; a day/night pair (Espeon/Umbreon...) -> Sun/Moon Stone; otherwise the time
    // condition is simply dropped.
    if (S->evoRemoveTimeBased && timeKind != TIME_KIND_NONE)
    {
        if (timeKind == TIME_KIND_DUSK)
        {
            toStone = TRUE;
            toItem = ITEM_DUSK_STONE;
        }
        else
        {
            for (i = 0; all[i].method != EVOLUTIONS_END; i++)
            {
                u32 other = TimeKind(all[i].params);
                if (i != k && all[i].targetSpecies != all[k].targetSpecies
                 && ((timeKind == TIME_KIND_DAY && other == TIME_KIND_NIGHT) || (timeKind == TIME_KIND_NIGHT && other == TIME_KIND_DAY)))
                {
                    toStone = TRUE;
                    toItem = (timeKind == TIME_KIND_DAY) ? ITEM_SUN_STONE : ITEM_MOON_STONE;
                    break;
                }
            }
        }
    }

    for (i = 0; !toStone && src != NULL && src[i].condition != CONDITIONS_END && n < EVO_PARAMS - 1; i++)
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
            if (p.condition == IF_SPECIES_IN_PARTY)
            {
                toLevel = TRUE;                              // Mantyke: needs a Remoraid in the party
                continue;
            }
            if (p.condition == IF_TYPE_IN_PARTY)
                continue;                                    // Pancham: just drop the party condition
        }
        if (S->evoRemoveTimeBased && IsTimeCondition(p.condition))
            continue;
        if (S->evoChangeImpossible && e->method == EVO_TRADE && p.condition == IF_HOLD_ITEM)
        {
            toItem = p.arg1;                                 // trade holding X -> use X from the bag like a stone
            continue;
        }
        buf[n++] = p;
    }
    buf[n].condition = CONDITIONS_END;

    if (toStone)
    {
        e->method = EVO_ITEM;
        e->param = toItem;
        n = 0;
    }
    else if (S->evoChangeImpossible || S->evoMakeEasier)
    {
        if (S->evoChangeImpossible)
        {
            switch (e->method)
            {
            case EVO_TRADE:
                if (toItem != ITEM_NONE && RH_EvoItemsForSale())
                {
                    e->method = EVO_ITEM;                    // the item is sold by the evolution sellers
                    e->param = toItem;
                    toLevel = FALSE;
                }
                else if (toItem == ITEM_KINGS_ROCK && all[k].targetSpecies == SPECIES_SLOWKING)
                {
                    e->method = EVO_ITEM;                    // FVX (Gen 3): Water Stone
                    e->param = ITEM_WATER_STONE;
                    toLevel = FALSE;
                }
                else if (toItem == ITEM_DEEP_SEA_SCALE)
                {
                    e->method = EVO_ITEM;                    // FVX (Gen 3): Gorebyss by Water Stone
                    e->param = ITEM_WATER_STONE;
                    toLevel = FALSE;
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

static bool32 TargetInPool(u16 species)
{
    s32 i = RH_PoolIndexOf(species);
    return i >= 0 && RH_PoolAllowed(i);
}

static bool32 HasPoolSibling(const struct Evolution *vanilla, u16 target)
{
    u32 i;
    for (i = 0; vanilla[i].method != EVOLUTIONS_END; i++)
        if (vanilla[i].method != EVO_NONE && vanilla[i].targetSpecies != target && TargetInPool(vanilla[i].targetSpecies))
            return TRUE;
    return FALSE;
}

const struct Evolution *RH_Evolutions(enum Species species, const struct Evolution *vanilla)
{
    u32 i, j, n, key;
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
            if (S->evolutions == 1 && vanilla[n].method != EVO_NONE && vanilla[n].targetSpecies != SPECIES_NONE
             && !TargetInPool(vanilla[n].targetSpecies) && HasPoolSibling(vanilla, vanilla[n].targetSpecies))
            {
                c->evos[n].method = EVO_NONE;                // a form outside the pool (Alolan Raichu...): its sibling's entry stays
                chosen[n] = SPECIES_NONE;
                continue;
            }
            if (S->evolutions == 1 && vanilla[n].method != EVO_NONE && vanilla[n].targetSpecies != SPECIES_NONE)
            {
                u16 target = SPECIES_NONE;
                // entries that lead to the same Pokemon (Feebas, Magneton...) keep sharing one target
                for (j = 0; j < n; j++)
                    if (vanilla[j].targetSpecies == vanilla[n].targetSpecies && chosen[j] != SPECIES_NONE)
                        target = chosen[j];
                if (target == SPECIES_NONE)
                    target = RandomEvoTarget(species, vanilla[n].targetSpecies, n, chosen, n);
                if (target != SPECIES_NONE)
                    c->evos[n].targetSpecies = target;
                else if (EvoRank(vanilla[n].targetSpecies) >= EvoRank(species))
                    c->evos[n].method = EVO_NONE;            // keeping the original could create a loop
                chosen[n] = c->evos[n].targetSpecies;
            }
            AdjustEvolution(&c->evos[n], c->params[n], vanilla, n);
        }
        // Shedinja-style entries name the Pokemon they split from: follow its new target.
        for (i = 0; i < n; i++)
        {
            if (c->evos[i].method != EVO_SPLIT_FROM_EVO)
                continue;
            for (j = 0; j < n; j++)
                if (j != i && vanilla[j].targetSpecies == vanilla[i].param)
                    c->evos[i].param = c->evos[j].targetSpecies;
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
    if (!S->enabled || !S->wildCatchRateOn)
        return vanilla;
    level = min(S->wildCatchRate, 5);
    return max(vanilla, RH_IsLegendary(species) ? sLegend[level] : sNormal[level]);
}

bool32 RH_GuaranteedCatch(void)
{
    return S->enabled && S->wildCatchRateOn && S->wildCatchRate >= 5;
}
