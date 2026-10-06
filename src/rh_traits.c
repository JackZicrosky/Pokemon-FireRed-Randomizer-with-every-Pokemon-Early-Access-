// Pokemon Traits: base stats, EXP curves, types, abilities, evolutions (+ catch rate).
#include "global.h"
#include "malloc.h"
#include "constants/characters.h"
#include "pokemon.h"
#include "regions.h"
#include "rh_internal.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/species.h"
#include "data/rh_evo_levels.h"

// Alternate forms (Megas, Rotom appliances, Unown letters, battle forms...) follow their base form, like FVX's
// "Follow Mega Evolutions" / cosmetic forms. Regional forms are Pokemon of their own.
static bool32 IsMegaLike(const struct SpeciesInfo *info)
{
    return info->isMegaEvolution || info->isPrimalReversion || info->isUltraBurst;
}

static u16 FormBaseEx(u16 species, bool32 followMegas)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    u16 base = GET_BASE_SPECIES_ID(species);
    if (base == species || base == SPECIES_NONE || base >= NUM_SPECIES)
        return species;
    if (info->isAlolanForm || info->isGalarianForm || info->isHisuianForm || info->isPaldeanForm)
        return species;
    if (IsMegaLike(info) && !followMegas)
        return species;                                      // FVX: Megas get their own rolls unless "Follow Mega Evolutions"
    return base;
}

static u16 FormBase(u16 species)
{
    return FormBaseEx(species, TRUE);
}

// "Follow Evolutions" key: the line's root, as a base form (Basculegion evolves from a Basculin form).
static u16 FollowRoot(u16 species)
{
    return FormBase(RH_TraitRoot(species));
}

// ---------------------------------------------------------------------------
// Base stats
// ---------------------------------------------------------------------------
struct StatCache { u16 species; u32 key; u8 v[NUM_STATS]; };
static EWRAM_DATA struct StatCache sStatCache[6] = {0};
static EWRAM_DATA u8 sStatCacheNext = 0;

static u32 SettingsKeyStats(void)
{
    return S->seed ^ (S->enabled << 1) ^ (S->baseStats << 2) ^ (S->baseStatsFollowEvos << 4) ^ (S->baseStatsRandomAdded << 5)
         ^ (RH_StatsGen() << 8) ^ (S->bstMode << 12) ^ (S->bstChangePct << 14) ^ (S->bstFollowEvos << 22)
         ^ (S->bstSeparateLegends << 23) ^ (S->statsFollowMegas << 24) ^ (S->speciesPool << 25) ^ 0x1234;
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

// ---------------------------------------------------------------------------
// Base stat totals (FVX "Base Stat Totals": random buff/nerf %, shuffle, random). The stats keep their proportions.
// ---------------------------------------------------------------------------
struct BstTable { u32 key; u16 bst[NUM_SPECIES]; };
static EWRAM_DATA struct BstTable sBst = {0};

static u32 GenBST(u16 species)
{
    u8 v[NUM_STATS];
    GenStats(species, v);
    return Total(v);
}

static u32 ShuffleGroup(const struct RhPoolTraits *m)
{
    u32 g = 0;
    if (S->bstFollowEvos)
        g = m->chain;                                        // families are shuffled with families of the same length
    if (S->bstSeparateLegends)
        g = g * 2 + m->legendary;
    return g;
}

static u16 FirstVanillaEvolution(u16 species)
{
    const struct Evolution *e = GetSpeciesEvolutionsVanilla(species);
    u32 i;
    for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        if (e[i].method != EVO_NONE && e[i].targetSpecies != SPECIES_NONE && e[i].targetSpecies != species)
            return e[i].targetSpecies;
    return SPECIES_NONE;
}

static void ShuffleBsts(void)
{
    u32 g, i, n, k;
    u32 poolCount = RH_PoolCount();
    u16 *members = Alloc(poolCount * sizeof(u16));
    if (members == NULL)
        return;
    for (g = 0; g < 8; g++)
    {
        n = 0;
        for (i = 0; i < poolCount; i++)
        {
            struct RhPoolTraits m;
            RH_PoolTraits(i, &m);
            if (m.baseForm && ShuffleGroup(&m) == g && (!S->bstFollowEvos || m.stage == 1))
                members[n++] = m.species;
        }
        for (k = 0; k < n; k++)
        {
            u16 donor = members[RH_Permute(SALT_STATS + 0x55 + g, k, n)];
            u16 sp = members[k];
            sBst.bst[sp] = GenBST(donor);
            if (S->bstFollowEvos)
            {
                // evolutions take the BST of the donor family's evolution at the same stage
                u32 depth;
                for (depth = 0; depth < 2; depth++)
                {
                    const struct Evolution *e = GetSpeciesEvolutionsVanilla(sp);
                    u16 dn = FirstVanillaEvolution(donor);
                    u32 j;
                    if (dn == SPECIES_NONE || e == NULL)
                        break;
                    for (j = 0; e[j].method != EVOLUTIONS_END; j++)
                        if (e[j].method != EVO_NONE && e[j].targetSpecies != SPECIES_NONE && e[j].targetSpecies < NUM_SPECIES
                         && IsSpeciesEnabled(e[j].targetSpecies) && FormBaseEx(e[j].targetSpecies, FALSE) == e[j].targetSpecies)
                            sBst.bst[e[j].targetSpecies] = GenBST(dn);   // split evolutions all copy (FVX copySplitEvos)
                    sp = FirstVanillaEvolution(sp);
                    donor = dn;
                    if (sp == SPECIES_NONE)
                        break;
                }
            }
        }
    }
    Free(members);
}

static u32 BstKey(void)
{
    return SettingsKeyStats() ^ 0xB57;
}

static EWRAM_DATA bool8 sBstBuilding = FALSE;

static void BuildBsts(void)
{
    u32 s;
    sBstBuilding = TRUE;                                     // lookups made while building (species sanitizing
                                                             // reads base stats) get the plain totals
    for (s = 0; s < NUM_SPECIES; s++)
        sBst.bst[s] = IsSpeciesEnabled(s) ? GenBST(s) : 0;
    switch (S->bstMode)
    {
    case 1:     // random buff / nerf: each (family, with Follow Evolutions) gets one modifier
        for (s = 1; s < NUM_SPECIES; s++)
        {
            u32 pct = min(S->bstChangePct, 100), key, m;
            if (!IsSpeciesEnabled(s) || FormBaseEx(s, FALSE) != s)
                continue;
            key = S->bstFollowEvos ? FollowRoot(s) : s;
            m = 100 - pct + RH_Hash(SALT_STATS + 0x66, key, 0) % (2 * pct + 1);
            sBst.bst[s] = min(GenBST(s) * m / 100, 255 * NUM_STATS);
        }
        break;
    case 2:
        ShuffleBsts();
        break;
    case 3:     // completely random: between Sunkern (180) and Arceus (720)
        for (s = 1; s < NUM_SPECIES; s++)
            if (IsSpeciesEnabled(s) && FormBaseEx(s, FALSE) == s)
                sBst.bst[s] = 180 + RH_Hash(SALT_STATS + 0x77, s, 0) % (720 - 180);
        break;
    }
    // alternate forms: Megas get the base form's new total + 100, others keep their proportion to the base form
    for (s = 1; s < NUM_SPECIES; s++)
    {
        u16 base;
        const struct SpeciesInfo *info = &gSpeciesInfo[s];
        if (!IsSpeciesEnabled(s) || S->bstMode == 0)
            continue;
        base = FormBaseEx(s, TRUE);
        if (base == s || !IsSpeciesEnabled(base))
            continue;
        if (IsMegaLike(info))
            sBst.bst[s] = min(sBst.bst[base] + 100, 255 * NUM_STATS);
        else if (GenBST(base) != 0)
            sBst.bst[s] = min(sBst.bst[base] * GenBST(s) / GenBST(base), 255 * NUM_STATS);
    }
    sBst.key = BstKey();
    sBstBuilding = FALSE;
}

// The Pokemon's current base stat total (after "Base Stat Totals"; before any shuffle, which keeps the total).
u32 RH_SpeciesBST(u16 species)
{
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return 0;
    if (!S->enabled || S->bstMode == 0 || sBstBuilding)
        return RH_VanillaBST(species);
    if (sBst.key != BstKey())
        BuildBsts();
    return sBst.bst[species];
}

// Scales the stats to a new total, keeping their proportions (Shedinja keeps 1 HP).
static void ScaleToTotal(u8 *v, u32 target, u32 fixedMask)
{
    u32 i, total = 0, free = 0, given = 0;
    u32 fixedSum = 0;
    for (i = 0; i < NUM_STATS; i++)
    {
        total += v[i];
        if (fixedMask & (1u << i))
            fixedSum += v[i];
    }
    if (total == 0 || target == total)
        return;
    free = total - fixedSum;
    target = target > fixedSum ? target - fixedSum : 0;
    for (i = 0; i < NUM_STATS; i++)
    {
        u32 nv;
        if (fixedMask & (1u << i))
            continue;
        nv = free ? v[i] * target / free : 0;
        nv = max(1, min(nv, 255));
        given += nv;
        v[i] = nv;
    }
    for (i = 0; given < target && i < NUM_STATS * 64; i++)
    {
        u32 s = i % NUM_STATS;
        if ((fixedMask & (1u << s)) || v[s] == 255)
            continue;
        v[s]++;
        given++;
    }
}

static void ComputeStats(u16 species, u8 *out, u32 depth)
{
    u8 base[NUM_STATS];
    u32 key, i, fixed = 0;
    GenStats(species, base);
    if (base[STAT_HP] == 1)
        fixed = 1u << STAT_HP;                               // Shedinja keeps 1 HP
    if (S->enabled && S->bstMode != 0)
        ScaleToTotal(base, RH_SpeciesBST(species), fixed);
    memcpy(out, base, NUM_STATS);
    if (!S->enabled || S->baseStats == 0)
        return;
    key = FormBaseEx(species, S->statsFollowMegas);
    if (S->baseStatsFollowEvos)
        key = FollowRoot(key);
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
        if (S->enabled && S->bstMode != 0)
            ScaleToTotal(preBase, RH_SpeciesBST(preEvo), preBase[STAT_HP] == 1 ? 1u << STAT_HP : 0);
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
    if ((!S->enabled || (S->baseStats == 0 && S->bstMode == 0)) && RH_StatsGen() >= 9)
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
        species = GET_BASE_SPECIES_ID(species);              // forms keep their base form's curve (Hoopa, Megas...)
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
    if (FormBaseEx(species, S->typesFollowMegas) != species)
    {
        u16 base = FormBase(species);
        const struct SpeciesInfo *baseInfo = &gSpeciesInfo[base];
        t1 = RH_SpeciesType(base, 0, baseInfo->types[0]);
        if (info->types[0] == baseInfo->types[0] && info->types[1] == baseInfo->types[1])
            return RH_SpeciesType(base, slot, baseInfo->types[slot ? 1 : 0]);   // same typing as its base form
        if (slot == 0)
            return t1;
        if (info->types[0] == info->types[1] && !S->forceDualTypes)
            return t1;
        t2 = RH_RandomMonType(RH_Hash(SALT_TYPE2, species, 7));  // the form's typing differs: a new second type
        if (t2 == t1)
            t2 = gRhMonTypes[(RH_TypeIndexOf(t1) + 1 + RH_Hash(SALT_TYPE2, species, 8) % 17) % 18];
        return t2;
    }
    if (S->types == 1)
    {
        // Follow evolutions: the family shares its first type; evolutions that gain a second type in the base game
        // gain a (family-wide) random second type.
        key = FollowRoot(species);
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
    if (gAbilitiesInfo[ability].name[0] == CHAR_HYPHEN)
        return FALSE;                                        // unused placeholder slots ("-------")
    switch (ability)
    {
    case ABILITY_AURA_GUARD:                                 // not implemented in this engine
        return FALSE;
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
    if (FormBaseEx(species, S->abilitiesFollowMegas) != species)
    {
        // Battle forms keep their form abilities (Zen Mode...); cosmetic forms and (with "Follow Mega Evolutions")
        // Megas copy the base form's new abilities.
        if (gAbilitiesInfo[info->abilities[0]].cantBeSwapped || (vanilla != ABILITY_NONE && gAbilitiesInfo[vanilla].cantBeSwapped))
            return vanilla;
        return RH_SpeciesAbility(FormBase(species), slot, gSpeciesInfo[FormBase(species)].abilities[slot]);
    }
    if (vanilla == ABILITY_NONE && !(slot == 1 && S->ensureTwoAbilities))
        return ABILITY_NONE;
    key = S->abilitiesFollowEvos ? FollowRoot(species) : species;
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
    return S->enabled && (S->evolutions || S->evoChangeImpossible || S->evoMakeEasier || S->evoRemoveTimeBased || S->evoAdjustLevels);
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
    if (!sEvoRelaxed && sEvoMinBst && RH_SpeciesBST(species) < sEvoMinBst)
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
    sEvoMinBst = (S->evoForceGrowth && S->evolutions == 1) ? RH_SpeciesBST(species) + 1 : 0;
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

// Global No Convergence table (like FVX, which picks from the targets not used yet): every evolution of the
// allowed Pokemon gets its target once per settings, in species order, never a target that was already taken.
// Entries: source (11 bits) << 16 | vanilla entry (4 bits) << 11 | target (11 bits).
#define NC_MAX 1400
struct NoConvTable { u32 key; u16 count; u32 e[NC_MAX]; };
static EWRAM_DATA struct NoConvTable sNoConv = {0};
STATIC_ASSERT(NUM_SPECIES < 2048, NoConvSpeciesBits);

static bool32 RegionLocked(const struct Evolution *e);
static bool32 TargetInPool(u16 species);
static bool32 HasPoolSibling(const struct Evolution *vanilla, u16 target);

// Build-time snapshot of every pool Pokemon (the per-candidate getters are far too slow to call ~100k times).
struct NcMon { u16 species; u16 bst; u8 growth; u8 rem; u8 t1, t2; u8 allowed; u8 pad; };
struct NcCtx
{
    struct NcMon *mon;      // [pool count]
    u16 *order;             // pool indexes grouped by (growth, stages left), shuffled inside each group
    u16 start[6 * 4 + 1];   // group starts in order[]
    u16 len[6 * 4];         // Pokemon left in each group (taken targets are swapped out)
    u8 *used;               // targets already taken (bitset by species)
    u32 n;
};

#define NC_GROUP(g, r) ((g) * 4 + (r))
#define NC_USED(u, s) ((u)[(s) >> 3] & (1 << ((s) & 7)))

static bool32 NcSharesType(const struct NcMon *m, u8 a, u8 b)
{
    return m->t1 == a || m->t1 == b || m->t2 == a || m->t2 == b;
}

// A target for one evolution of "species" (original = its vanilla target, NONE for Every Level), never one that is
// used already. Passes: similar-strength windows, then any strength, then without Same Typing / Force Growth.
static u16 PickNoConv(struct NcCtx *x, u16 species, u16 original)
{
    static const u8 sWindows[] = { 10, 20, 35, 0, 0 };
    u8 growth = GetSpeciesGrowthRate(species), st1 = GetSpeciesType(species, 0), st2 = GetSpeciesType(species, 1);
    u32 srcBst = RH_SpeciesBST(species), pass, k, g, remLo, remHi;
    u32 bst = (S->evoSimilarStrength && S->evolutions == 1 && original != SPECIES_NONE) ? RH_SpeciesBST(original) : 0;
    if (growth >= 6)
        return SPECIES_NONE;
    if (S->evolutions == 1)
        remLo = remHi = min(Remaining(original), 3u);       // chains keep their length: they can't loop
    else
        remLo = 0, remHi = 3;
    for (pass = 0; pass < 5; pass++)
    {
        u32 window = sWindows[pass];
        bool32 relaxed = (pass == 4);                       // last resort: no Same Typing / Force Growth
        if (pass < 3 && bst == 0)
            continue;
        for (g = NC_GROUP(growth, remLo); g <= NC_GROUP(growth, remHi); g++)
        {
            u32 lo = x->start[g], cnt = x->len[g], off;
            if (cnt == 0)
                continue;
            off = RH_Hash(SALT_EVO, species, original + pass) % cnt;
            for (k = 0; k < cnt; k++)
            {
                u32 pos = lo + (off + k) % cnt;
                const struct NcMon *m = &x->mon[x->order[pos]];
                if (!m->allowed || m->species == species || NC_USED(x->used, m->species))
                    continue;
                if (window && (m->bst * 100 < bst * (100 - window) || m->bst * 100 > bst * (100 + window)))
                    continue;
                if (!relaxed && S->evoSameTyping && !NcSharesType(m, st1, st2))
                    continue;
                if (!relaxed && S->evoForceGrowth && S->evolutions == 1 && m->bst <= srcBst)
                    continue;
                if (S->evoForceChange && IsVanillaTarget(species, m->species))
                    continue;
                // taken: swap it out of its group so later searches don't walk over it
                {
                    u16 taken = m->species;
                    u16 tmp = x->order[lo + cnt - 1];
                    x->order[lo + cnt - 1] = (u16)(m - x->mon);
                    x->order[pos] = tmp;
                    x->len[g]--;
                    return taken;
                }
            }
        }
    }
    return SPECIES_NONE;
}

static void BuildNoConv(void)
{
    struct NcCtx x;
    u32 p, k, j, g;
    u16 count[6 * 4] = {0};
    sNoConv.count = 0;
    sNoConv.key = SettingsKeyEvos();
    x.n = RH_PoolCount();
    x.mon = Alloc(x.n * sizeof(struct NcMon));
    x.order = Alloc(x.n * sizeof(u16));
    x.used = AllocZeroed((NUM_SPECIES + 7) / 8);
    if (x.mon == NULL || x.order == NULL || x.used == NULL)
        goto done;
    for (p = 0; p < x.n; p++)
    {
        struct NcMon *m = &x.mon[p];
        m->species = RH_PoolSpecies(p);
        m->allowed = RH_PoolAllowed(p);
        m->bst = RH_SpeciesBST(m->species);
        m->growth = GetSpeciesGrowthRate(m->species);
        m->rem = min(Remaining(m->species), 3u);
        m->t1 = GetSpeciesType(m->species, 0);
        m->t2 = GetSpeciesType(m->species, 1);
        if (m->growth < 6)
            count[NC_GROUP(m->growth, m->rem)]++;
    }
    x.start[0] = 0;
    for (g = 0; g < 6 * 4; g++)
    {
        x.start[g + 1] = x.start[g] + count[g];
        x.len[g] = count[g];
    }
    memset(count, 0, sizeof(count));
    for (p = 0; p < x.n; p++)
        if (x.mon[p].growth < 6)
        {
            g = NC_GROUP(x.mon[p].growth, x.mon[p].rem);
            x.order[x.start[g] + count[g]++] = p;
        }
    // shuffle each group (keyed)
    for (g = 0; g < 6 * 4; g++)
    {
        u32 lo = x.start[g], cnt = x.start[g + 1] - lo;
        for (k = cnt; k > 1; k--)
        {
            u32 r = RH_Hash(SALT_EVO + 0x77, g, k) % k;
            u16 t = x.order[lo + k - 1];
            x.order[lo + k - 1] = x.order[lo + r];
            x.order[lo + r] = t;
        }
    }
    for (p = 0; p < x.n && sNoConv.count < NC_MAX; p++)
    {
        u16 sp = x.mon[p].species;
        const struct Evolution *v;
        if (!x.mon[p].allowed)
            continue;
        if (S->evolutions == 2)
        {
            u16 t = PickNoConv(&x, sp, SPECIES_NONE);
            if (t != SPECIES_NONE)
                x.used[t >> 3] |= 1 << (t & 7);
            sNoConv.e[sNoConv.count++] = (sp << 16) | t;
            continue;
        }
        v = GetSpeciesEvolutionsVanilla(sp);
        for (k = 0; v != NULL && k < 16 && v[k].method != EVOLUTIONS_END && sNoConv.count < NC_MAX; k++)
        {
            u16 t;
            bool32 shared = FALSE;
            if (v[k].method == EVO_NONE || v[k].targetSpecies == SPECIES_NONE || RegionLocked(&v[k]))
                continue;
            if (!TargetInPool(v[k].targetSpecies) && HasPoolSibling(v, v[k].targetSpecies))
                continue;
            for (j = 0; j < k; j++)
                shared |= (v[j].targetSpecies == v[k].targetSpecies);
            if (shared)
                continue;                                    // entries to the same Pokemon share one target
            t = PickNoConv(&x, sp, v[k].targetSpecies);
            if (t != SPECIES_NONE)
                x.used[t >> 3] |= 1 << (t & 7);
            sNoConv.e[sNoConv.count++] = (sp << 16) | (k << 11) | t;
        }
    }
done:
    if (x.mon != NULL)
        Free(x.mon);
    if (x.order != NULL)
        Free(x.order);
    if (x.used != NULL)
        Free(x.used);
}

// TRUE if the table has the entry; *target = SPECIES_NONE when no free target fitted.
static bool32 NoConvLookup(u16 species, u32 entry, u16 *target)
{
    s32 lo = 0, hi;
    if (sNoConv.key != SettingsKeyEvos())
        BuildNoConv();
    hi = sNoConv.count - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        u32 e = sNoConv.e[mid];
        u32 s = e >> 16;
        if (s == species)
        {
            // few entries per species: scan around mid
            while (mid > 0 && (sNoConv.e[mid - 1] >> 16) == species)
                mid--;
            for (; mid < sNoConv.count && (sNoConv.e[mid] >> 16) == species; mid++)
            {
                if (S->evolutions == 2 || ((sNoConv.e[mid] >> 11) & 0xF) == entry)
                {
                    *target = sNoConv.e[mid] & 0x7FF;
                    return TRUE;
                }
            }
            return FALSE;
        }
        if (s < species)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return FALSE;
}

static u16 RandomEvoTarget(u16 species, u16 original, u32 n, const u16 *chosen, u32 chosenCount)
{
    struct RhFilter f = {0};
    u32 i;
    if (S->evoNoConvergence)
    {
        u16 t;
        if (NoConvLookup(species, n, &t) && t != SPECIES_NONE)
            return t;
    }
    SetEvoContext(species, original);
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

// FVX estimated evolution level (findEvolutionLevel over all level-up evolutions, precomputed by BST pair) with its
// post-processing: at least 25% above the previous evolution, at most 80% of the next one.
static u32 RawEstimate(u16 from, u16 to)
{
    s32 i = ((s32)RH_SpeciesBST(from) - RH_EVO_BST_LO + RH_EVO_BST_STEP / 2) / RH_EVO_BST_STEP;
    s32 j = ((s32)RH_SpeciesBST(to) - RH_EVO_BST_LO + RH_EVO_BST_STEP / 2) / RH_EVO_BST_STEP;
    i = max(0, min(i, RH_EVO_BST_N - 1));
    j = max(0, min(j, RH_EVO_BST_N - 1));
    return sRhEvoLevelEstimate[i][j];
}

u32 RH_EstimateEvoLevel(u16 from, u16 to)
{
    u32 est = RawEstimate(from, to);
    u16 pre = RH_PreEvo(from), next = FirstVanillaEvolution(to);
    if (pre != SPECIES_NONE)
        est = max(est, (RawEstimate(pre, from) * 5 + 3) / 4);
    if (next != SPECIES_NONE)
        est = min(est, (RawEstimate(to, next) * 4 + 4) / 5);
    return max(2, min(est, 100));
}

static EWRAM_DATA u16 sAdjustFrom = 0;                        // the Pokemon whose evolutions are being adjusted

static u32 ImpossibleLevel(u16 target)
{
    return S->evoEstimatedLevels ? RH_EstimateEvoLevel(sAdjustFrom, target) : 37;
}

static bool32 ConditionImpossible(const struct EvolutionParam *p)
{
    switch (p->condition)
    {
    case IF_IN_MAP: case IF_IN_MAPSEC: case IF_MIN_BEAUTY: case IF_MIN_COOLNESS: case IF_MIN_SMARTNESS:
    case IF_MIN_TOUGHNESS: case IF_MIN_CUTENESS: case IF_TRADE_PARTNER_SPECIES: case IF_DEFEAT_X_WITH_ITEMS:
        return TRUE;
    case IF_KNOWS_MOVE: case IF_KNOWS_MOVE_TYPE: case IF_USED_MOVE_X_TIMES:
        return S->movesets != 0;                             // the move may never be learned
    case IF_BAG_ITEM_COUNT:
        return TRUE;                                         // Gimmighoul's 999 coins
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

    if (S->evoChangeImpossible && e->method != EVO_TRADE && HasImpossibleCondition(&all[k]))
    {
        // Another, possible entry already leads to the same Pokemon (Eevee's Leafeon/Glaceon stones,
        // Magnezone's Thunder Stone...): just drop this one. Not for trades: their Linking Cord entry stays, and
        // the trade itself becomes a level-up evolution (FVX).
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
    // "Adjust Evolution Levels" (FVX): level-up evolutions happen at a level that fits the two Pokemon
    if (S->evoAdjustLevels && (e->method == EVO_LEVEL || e->method == EVO_LEVEL_BATTLE_ONLY) && e->param > 1)
        e->param = RH_EstimateEvoLevel(sAdjustFrom, e->targetSpecies);
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

// An entry that can never trigger in Kanto (Alolan Raichu, Hisuian Typhlosion...).
static bool32 RegionLocked(const struct Evolution *e)
{
    u32 i;
    for (i = 0; e->params != NULL && e->params[i].condition != CONDITIONS_END && i < EVO_PARAMS; i++)
    {
        if (e->params[i].condition == IF_REGION && e->params[i].arg1 != GetCurrentRegion())
            return TRUE;
        if (e->params[i].condition == IF_NOT_REGION && e->params[i].arg1 == GetCurrentRegion())
            return TRUE;
    }
    return FALSE;
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
        sAdjustFrom = species;
        for (n = 0; n < EVO_MAX && vanilla[n].method != EVOLUTIONS_END; n++)
        {
            c->evos[n] = vanilla[n];
            chosen[n] = SPECIES_NONE;
            if (S->evolutions == 1 && RegionLocked(&vanilla[n]))
            {
                c->evos[n].method = EVO_NONE;                // never happens here: don't waste a random target on it
                continue;
            }
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
