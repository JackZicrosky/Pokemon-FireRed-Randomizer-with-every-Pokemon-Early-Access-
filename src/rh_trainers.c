// Foe Pokemon: trainer teams, movesets, items, battle style, names.
#include "global.h"
#include "constants/characters.h"
#include "battle_util.h"
#include "data.h"
#include "difficulty.h"
#include "item.h"
#include "move.h"
#include "overworld.h"
#include "pokemon.h"
#include "string_util.h"
#include "random.h"
#include "trainer_util.h"
#include "wild_encounter.h"
#include "rh_internal.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/trainers.h"
#include "data/rh_randomizer_tables.h"
#include "data/rh_names.h"

enum Species RH_WildSpeciesAt(u8 mapGroup, u8 mapNum, const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level);
u16 RH_StarterForSlot(u32 slot);
s32 RH_StarterFamily(u16 species, u32 *stage);
bool32 RH_IsGoodDamagingMove(u16 move);
bool32 RH_IsDamagingMove(u16 move);

enum { TIER_BOSS, TIER_IMPORTANT, TIER_REGULAR };

// ---------------------------------------------------------------------------
// Trainer identification
// ---------------------------------------------------------------------------
static s32 TrainerIdOf(const struct Trainer *trainer)
{
    u32 d;
    for (d = 0; d < DIFFICULTY_COUNT; d++)
    {
        const struct Trainer *base = &gTrainers[d][0];
        if (trainer >= base && trainer < base + TRAINERS_COUNT)
            return trainer - base;
    }
    return -1;
}

static bool32 IsRivalClass(u32 trainerClass)
{
    return trainerClass == TRAINER_CLASS_RIVAL_EARLY_FRLG || trainerClass == TRAINER_CLASS_RIVAL_LATE_FRLG
        || trainerClass == TRAINER_CLASS_CHAMPION_FRLG;
}

static bool32 IsLeagueClass(u32 trainerClass)
{
    return trainerClass == TRAINER_CLASS_ELITE_FOUR_FRLG || trainerClass == TRAINER_CLASS_CHAMPION_FRLG
        || trainerClass == TRAINER_CLASS_ELITE_FOUR || trainerClass == TRAINER_CLASS_CHAMPION;
}

static u32 TierOf(u32 trainerClass)
{
    switch (trainerClass)
    {
    case TRAINER_CLASS_LEADER_FRLG: case TRAINER_CLASS_ELITE_FOUR_FRLG: case TRAINER_CLASS_CHAMPION_FRLG:
    case TRAINER_CLASS_LEADER: case TRAINER_CLASS_ELITE_FOUR: case TRAINER_CLASS_CHAMPION:
        return TIER_BOSS;
    case TRAINER_CLASS_RIVAL_EARLY_FRLG: case TRAINER_CLASS_RIVAL_LATE_FRLG: case TRAINER_CLASS_BOSS_FRLG:
        return TIER_IMPORTANT;
    default:
        return TIER_REGULAR;
    }
}

static u8 GymTheme(s32 trainerId)
{
    u32 i;
    if (trainerId >= 0)
        for (i = 0; i < ARRAY_COUNT(sRhTrainerThemes); i++)
            if (sRhTrainerThemes[i].trainer == trainerId)
                return sRhTrainerThemes[i].type;
    return TYPE_NONE;
}

// A type shared by every Pokemon of the trainer's vanilla team (TYPE_NONE if none / only one Pokemon).
static u8 TeamTheme(const struct Trainer *trainer)
{
    u32 t, i;
    if (trainer->partySize < 2)
        return TYPE_NONE;
    for (t = 0; t < 2; t++)
    {
        u8 type = gSpeciesInfo[trainer->party[0].species].types[t];
        for (i = 1; i < trainer->partySize; i++)
        {
            const struct SpeciesInfo *si = &gSpeciesInfo[trainer->party[i].species];
            if (si->types[0] != type && si->types[1] != type)
                break;
        }
        if (i == trainer->partySize)
            return type;
    }
    return TYPE_NONE;
}

// "Weight Types by # of Pokemon": themes pick types in proportion to how many Pokemon have them.
static u8 RandomTheme(u32 hash)
{
    u32 counts[18] = {0}, total = 0, i, r;
    if (!S->trainerWeightTypes)
        return RH_RandomMonType(hash);
    for (i = 0; i < RH_PoolCount(); i++)
    {
        u16 sp;
        if (!RH_PoolAllowed(i))
            continue;
        sp = RH_PoolSpecies(i);
        counts[RH_TypeIndexOf(GetSpeciesType(sp, 0))]++;
        if (GetSpeciesType(sp, 1) != GetSpeciesType(sp, 0))
            counts[RH_TypeIndexOf(GetSpeciesType(sp, 1))]++;
    }
    for (i = 0; i < 18; i++)
        total += counts[i];
    if (total == 0)
        return RH_RandomMonType(hash);
    r = hash % total;
    for (i = 0; i < 18; i++)
    {
        if (r < counts[i])
            return gRhMonTypes[i];
        r -= counts[i];
    }
    return gRhMonTypes[0];
}

// ---------------------------------------------------------------------------
// League-unique Pokemon: a few species reserved for the Elite Four and Champion.
// ---------------------------------------------------------------------------
#define MAX_RESERVED 10
struct ReservedCache { u32 key; u8 count; u16 species[MAX_RESERVED]; };
static EWRAM_DATA struct ReservedCache sReserved = {0};

static bool32 GoodLeagueMon(u16 species)
{
    return RH_VanillaBST(species) >= 480 && RH_SpeciesStage(species) == RH_SpeciesChain(species);
}

static void BuildReserved(void)
{
    u32 i;
    sReserved.key = S->seed ^ (S->leagueUnique << 28) ^ (S->speciesPool << 24) ^ 0x4C;
    sReserved.count = 0;
    for (i = 0; i < 5 * S->leagueUnique && i < MAX_RESERVED; i++)
    {
        struct RhFilter f = {0};
        u32 j;
        u16 sp;
        f.legend = 1;
        f.extra = GoodLeagueMon;
        for (j = 0; j < sReserved.count; j++)
            RH_FilterExclude(&f, sReserved.species[j]);
        sp = RH_PickWithFilter(&f, RH_Hash(SALT_LEAGUE, i, 0));
        if (sp != SPECIES_NONE)
            sReserved.species[sReserved.count++] = sp;
    }
}

static void EnsureReserved(void)
{
    if (sReserved.key != (S->seed ^ (S->leagueUnique << 28) ^ (S->speciesPool << 24) ^ 0x4C))
        BuildReserved();
}

bool32 RH_IsLeagueReserved(u16 species)
{
    u32 i;
    if (!S->enabled || !S->leagueUnique)
        return FALSE;
    EnsureReserved();
    for (i = 0; i < sReserved.count; i++)
        if (sReserved.species[i] == species)
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Team context (diverse types / avoid duplicates)
// ---------------------------------------------------------------------------
static EWRAM_DATA const struct Trainer *sTeamTrainer = NULL;
static EWRAM_DATA u16 sTeam[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sTeamCount = 0;
static EWRAM_DATA u8 sDiverse = 0;

static bool32 TeamPredicate(u16 species)
{
    u32 i;
    if (RH_IsLeagueReserved(species))
        return FALSE;
    if (sDiverse)
        for (i = 0; i < sTeamCount; i++)
            if (GetSpeciesType(sTeam[i], 0) == GetSpeciesType(species, 0))
                return FALSE;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Evolving trainer Pokemon to their level
// ---------------------------------------------------------------------------
static u16 EvolveForLevel(u16 species, u32 level, u32 salt)
{
    u32 i, guard;
    u32 fullBy = S->trainersEvolveOn ? S->trainersEvolveLevel : 0;
    for (guard = 0; guard < 3; guard++)
    {
        const struct Evolution *e = GetSpeciesEvolutions(species);
        u16 next = SPECIES_NONE;
        u32 n = 0, pick;
        bool32 ok = FALSE;
        for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
            if (e[i].method != EVO_NONE && e[i].targetSpecies != SPECIES_NONE && e[i].targetSpecies != species)
                n++;
        if (n == 0)
            break;
        pick = RH_Hash(SALT_EVO_LEVEL, species, salt) % n;
        for (i = 0; e[i].method != EVOLUTIONS_END; i++)
        {
            if (e[i].method == EVO_NONE || e[i].targetSpecies == SPECIES_NONE || e[i].targetSpecies == species)
                continue;
            if (pick-- == 0)
            {
                next = e[i].targetSpecies;
                if (e[i].method == EVO_LEVEL || e[i].method == EVO_LEVEL_BATTLE_ONLY)
                    ok = level >= e[i].param && e[i].param > 1;
                else
                    ok = fullBy && level >= fullBy * 3 / 4 + (RH_SpeciesChain(next) > RH_SpeciesStage(next) ? 0 : fullBy / 8);
                if (fullBy && level >= fullBy)
                    ok = TRUE;
                break;
            }
        }
        if (!ok || next == SPECIES_NONE)
            break;
        species = next;
    }
    return species;
}

// ---------------------------------------------------------------------------
// Use Local Pokemon
// ---------------------------------------------------------------------------
static const struct WildPokemonHeader *FindWildHeader(u8 group, u8 num)
{
    u32 i;
    for (i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); i++)
        if (gWildMonHeaders[i].mapGroup == group && gWildMonHeaders[i].mapNum == num)
            return &gWildMonHeaders[i];
    return NULL;
}

static u16 LocalSpecies(u32 hash, u32 level)
{
    const struct WildPokemonHeader *h = FindWildHeader(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum);
    u8 group = gSaveBlock1Ptr->location.mapGroup, num = gSaveBlock1Ptr->location.mapNum;
    const struct WildPokemonInfo *infos[2];
    static const u8 sCounts[2] = { 12, 5 };
    static const u8 sAreas[2] = { WILD_AREA_LAND, WILD_AREA_WATER };
    u32 total = 0, r, i;
    if (h == NULL)
    {
        group = gSaveBlock1Ptr->escapeWarp.mapGroup;
        num = gSaveBlock1Ptr->escapeWarp.mapNum;
        h = FindWildHeader(group, num);
    }
    if (h == NULL)
        return SPECIES_NONE;
    infos[0] = h->encounterTypes[0].landMonsInfo;
    infos[1] = h->encounterTypes[0].waterMonsInfo;
    for (i = 0; i < 2; i++)
        if (infos[i] != NULL)
            total += sCounts[i];
    if (total == 0)
        return SPECIES_NONE;
    r = hash % total;
    for (i = 0; i < 2; i++)
    {
        if (infos[i] == NULL)
            continue;
        if (r < sCounts[i])
            return EvolveForLevel(RH_WildSpeciesAt(group, num, infos[i], r, sAreas[i], level), level, 7);
        r -= sCounts[i];
    }
    return SPECIES_NONE;
}

// ---------------------------------------------------------------------------
// Better movesets / held items
// ---------------------------------------------------------------------------
static bool32 CanUseMove(u16 species, u16 move, u32 level)
{
    const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(species);
    u32 i;
    for (i = 0; l != NULL && l[i].move != LEVEL_UP_MOVE_END; i++)
        if (l[i].move == move && l[i].level <= level)
            return TRUE;
    return CanLearnTeachableMove(species, move);
}

static const u16 sSetupMoves[] = {
    MOVE_SWORDS_DANCE, MOVE_NASTY_PLOT, MOVE_CALM_MIND, MOVE_DRAGON_DANCE, MOVE_BULK_UP, MOVE_QUIVER_DANCE,
    MOVE_RECOVER, MOVE_ROOST, MOVE_SOFT_BOILED, MOVE_TOXIC, MOVE_THUNDER_WAVE, MOVE_WILL_O_WISP, MOVE_SPORE,
    MOVE_SLEEP_POWDER, MOVE_LEECH_SEED, MOVE_PROTECT, MOVE_SUBSTITUTE, MOVE_STEALTH_ROCK, MOVE_AGILITY,
};

static void BetterMoveset(struct TrainerMon *mon)
{
    u16 best[3] = {0};
    u32 score[3] = {0};
    u8 bestType[3] = {TYPE_NONE, TYPE_NONE, TYPE_NONE};
    u16 status = MOVE_NONE;
    u32 i, j, k, n = 0;
    const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(mon->species);
    u16 candidates[80];
    u32 count = 0;

    // candidates: level-up moves known by now + all TMs/tutors it can learn
    for (i = 0; l != NULL && l[i].move != LEVEL_UP_MOVE_END && count < ARRAY_COUNT(candidates); i++)
        if (l[i].level <= mon->lvl)
            candidates[count++] = l[i].move;
    for (i = 1; i <= NUM_TECHNICAL_MACHINES && count < ARRAY_COUNT(candidates); i++)
    {
        u16 m = GetTMHMMoveId(i);
        if (CanLearnTeachableMove(mon->species, m))
            candidates[count++] = m;
    }
    for (i = 0; i < count; i++)
    {
        u16 m = candidates[i];
        if (RH_IsDamagingMove(m))
        {
            u32 acc = GetMoveAccuracy(m) ? GetMoveAccuracy(m) : 100;
            u32 s = GetMovePower(m) * max(1, gMovesInfo[m].strikeCount) * acc / 100;
            u8 type = GetMoveType(m);
            if (RH_SpeciesHasType(mon->species, type))
                s = s * 3 / 2;
            if (IsExplosionMove(m) || m == MOVE_HYPER_BEAM || m == MOVE_GIGA_IMPACT)
                s /= 3;
            // keep the best move per type, up to 3 types
            for (j = 0; j < 3; j++)
            {
                if (bestType[j] == type)
                {
                    if (s > score[j])
                        best[j] = m, score[j] = s;
                    break;
                }
            }
            if (j == 3)
            {
                u32 worst = 0;
                for (k = 1; k < 3; k++)
                    if (score[k] < score[worst])
                        worst = k;
                if (s > score[worst])
                    best[worst] = m, score[worst] = s, bestType[worst] = type;
            }
        }
        else if (status == MOVE_NONE)
        {
            for (j = 0; j < ARRAY_COUNT(sSetupMoves); j++)
                if (sSetupMoves[j] == m)
                    status = m;
        }
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
        mon->moves[i] = MOVE_NONE;
    for (i = 0; i < 3; i++)
        if (best[i] != MOVE_NONE)
            mon->moves[n++] = best[i];
    if (status != MOVE_NONE)
        mon->moves[n++] = status;
    // top up with remaining level-up moves if needed
    for (i = count; i > 0 && n < MAX_MON_MOVES; i--)
    {
        u16 m = candidates[i - 1];
        for (j = 0; j < n; j++)
            if (mon->moves[j] == m)
                break;
        if (j == n && m != MOVE_NONE)
            mon->moves[n++] = m;
    }
    if (n == 0)
        mon->moves[0] = MOVE_NONE;                  // fall back to the default moveset
}

static const u16 sTypeBoostItems[] = {
    [TYPE_NORMAL] = ITEM_SILK_SCARF, [TYPE_FIGHTING] = ITEM_BLACK_BELT, [TYPE_FLYING] = ITEM_SHARP_BEAK,
    [TYPE_POISON] = ITEM_POISON_BARB, [TYPE_GROUND] = ITEM_SOFT_SAND, [TYPE_ROCK] = ITEM_HARD_STONE,
    [TYPE_BUG] = ITEM_SILVER_POWDER, [TYPE_GHOST] = ITEM_SPELL_TAG, [TYPE_STEEL] = ITEM_METAL_COAT,
    [TYPE_FIRE] = ITEM_CHARCOAL, [TYPE_WATER] = ITEM_MYSTIC_WATER, [TYPE_GRASS] = ITEM_MIRACLE_SEED,
    [TYPE_ELECTRIC] = ITEM_MAGNET, [TYPE_PSYCHIC] = ITEM_TWISTED_SPOON, [TYPE_ICE] = ITEM_NEVER_MELT_ICE,
    [TYPE_DRAGON] = ITEM_DRAGON_FANG, [TYPE_DARK] = ITEM_BLACK_GLASSES, [TYPE_FAIRY] = ITEM_FAIRY_FEATHER,
};

static u16 SensibleItem(const struct TrainerMon *mon, u32 hash)
{
    u16 sp = mon->species;
    u32 atk = GetSpeciesBaseAttack(sp), spa = GetSpeciesBaseSpAttack(sp), spe = GetSpeciesBaseSpeed(sp);
    u32 def = GetSpeciesBaseDefense(sp) + GetSpeciesBaseSpDefense(sp);
    u8 type = GetSpeciesType(sp, hash & 1);
    if (S->heldConsumableOnly)
    {
        static const u16 sConsumables[] = { ITEM_SITRUS_BERRY, ITEM_LUM_BERRY, ITEM_FOCUS_SASH, ITEM_WHITE_HERB };
        if (def < 150 && spe > 90)
            return ITEM_FOCUS_SASH;
        return sConsumables[hash % ARRAY_COUNT(sConsumables)];
    }
    if (RH_EvolveOnce(sp, 0) != SPECIES_NONE)
        return ITEM_EVIOLITE;
    switch (hash % 6)
    {
    case 0: return ITEM_LEFTOVERS;
    case 1: return ITEM_LIFE_ORB;
    case 2: return atk > spa ? ITEM_CHOICE_BAND : ITEM_CHOICE_SPECS;
    case 3: return (def < 150 && spe > 90) ? ITEM_FOCUS_SASH : ITEM_SITRUS_BERRY;
    case 4: return spe > 70 ? ITEM_CHOICE_SCARF : ITEM_ASSAULT_VEST;
    default:
        if (type < ARRAY_COUNT(sTypeBoostItems) && sTypeBoostItems[type] != ITEM_NONE)
            return sTypeBoostItems[type];
        return ITEM_LEFTOVERS;
    }
}

// ---------------------------------------------------------------------------
// Species choice
// ---------------------------------------------------------------------------
static u16 PickEven(const struct RhFilter *f, u32 index)
{
    u32 x = index % RH_PoolCount(), guard;
    for (guard = 0; guard < 4096; guard++)
    {
        x = RH_Permute(SALT_TRAINER, x, RH_PoolCount());
        if (RH_FilterAccepts(f, x))
            return RH_PoolSpecies(x);
    }
    return SPECIES_NONE;
}

static bool32 HasOnlyWonderGuard(u16 species)
{
    return GetSpeciesAbility(species, 0) == ABILITY_WONDER_GUARD
        && (GetSpeciesAbility(species, 1) == ABILITY_WONDER_GUARD || GetSpeciesAbility(species, 1) == ABILITY_NONE)
        && (GetSpeciesAbility(species, 2) == ABILITY_WONDER_GUARD || GetSpeciesAbility(species, 2) == ABILITY_NONE);
}

static EWRAM_DATA u8 sNoWonderGuard = 0;
static bool32 TeamPredicateWG(u16 species)
{
    if (sNoWonderGuard && HasOnlyWonderGuard(species))
        return FALSE;
    return TeamPredicate(species);
}

static u16 ChooseSpecies(const struct TrainerMon *mon, const struct Trainer *trainer, s32 trainerId, u32 slot, u32 tier)
{
    struct RhFilter f = {0};
    u16 similar = SPECIES_NONE, result = SPECIES_NONE;
    u32 key = (trainerId >= 0 ? trainerId : 0xFFF) * 8 + slot;
    u32 cls = trainer->trainerClass;
    u8 theme = TYPE_NONE;
    u32 i;

    if (S->trainerNoLegends)
        f.legend = 1;
    if (S->trainerAvoidDuplicates)
        for (i = 0; i < sTeamCount; i++)
            RH_FilterExclude(&f, sTeam[i]);
    sDiverse = S->diverseTypes[tier];
    sNoWonderGuard = S->noEarlyWonderGuard && mon->lvl < 20;
    f.extra = TeamPredicateWG;
    if (S->trainerSimilarStrength)
        similar = mon->species;

    switch (S->trainers)
    {
    case 3:     // type themed: every trainer gets a theme
        theme = RandomTheme(RH_Hash(SALT_TRAINER_THEME, trainerId, 0));
        if (GymTheme(trainerId) != TYPE_NONE)   // gym trainers share their leader's theme
            theme = RandomTheme(RH_Hash(SALT_TRAINER_THEME, 0x100 + GymTheme(trainerId), 0));
        break;
    case 4:     // themes for gyms and the Elite Four only
        if (GymTheme(trainerId) != TYPE_NONE)
            theme = RandomTheme(RH_Hash(SALT_TRAINER_THEME, 0x100 + GymTheme(trainerId), 0));
        break;
    case 5:     // keep type-themed trainers' themes
    case 6:
        theme = GymTheme(trainerId);
        if (theme == TYPE_NONE)
            theme = TeamTheme(trainer);
        if (theme == TYPE_NONE && S->trainers == 6)
            f.primaryType = gSpeciesInfo[mon->species].types[0];   // "or primary": keep each Pokemon's primary type
        break;
    }
    f.type = theme;

    // The rival keeps his team: each vanilla family maps to one replacement, evolved to the current level.
    if (S->rivalCarriesTeam && IsRivalClass(cls))
    {
        struct RhFilter g = f;
        u32 stage = RH_SpeciesStage(mon->species);
        g.stage = 1;
        g.excludeCount = 0;
        result = RH_PickSpecies(&g, RH_Hash(SALT_RIVAL, RH_FamilyRoot(mon->species), 0), similar ? RH_FamilyRoot(mon->species) : SPECIES_NONE);
        if (result != SPECIES_NONE)
        {
            u16 evolved = EvolveForLevel(result, mon->lvl, 0);
            if (!S->trainersEvolveOn && stage > 1)
                evolved = RH_EvolveTimes(result, stage - 1);
            return evolved;
        }
    }
    if (S->trainerLocalPokemon && tier == TIER_REGULAR)
    {
        result = LocalSpecies(RH_Hash(SALT_TRAINER, key, 1), mon->lvl);
        if (result != SPECIES_NONE && TeamPredicate(result))
            return result;
    }
    if (S->trainers == 2)
    {
        result = PickEven(&f, key);
        if (result != SPECIES_NONE)
            return result;
    }
    return RH_PickSpecies(&f, RH_Hash(SALT_TRAINER, key, 0), similar);
}

// ---------------------------------------------------------------------------
// Hook: called for every trainer Pokemon before it is created.
// ---------------------------------------------------------------------------
void RH_ModifyTrainerMon(struct TrainerMon *mon, const struct Trainer *trainer, u32 slot)
{
    s32 trainerId;
    u32 tier, stage;
    s32 starterSlot;
    u16 newSpecies = mon->species;
    bool32 changed = FALSE;
    if (!S->enabled)
        return;
    trainerId = TrainerIdOf(trainer);
    tier = TierOf(trainer->trainerClass);
    if (sTeamTrainer != trainer || slot == 0)
    {
        sTeamTrainer = trainer;
        sTeamCount = 0;
    }

    if (S->trainerLevelModOn)
        mon->lvl = min(max(RH_ApplyPercent(mon->lvl, S->trainerLevelMod), 1), MAX_LEVEL);

    starterSlot = RH_StarterFamily(mon->species, &stage);
    if (starterSlot >= 0 && IsRivalClass(trainer->trainerClass) && (S->starters != 0 || S->trainers != 0))
    {
        // The rival always uses the starter the player didn't pick, evolved like the original.
        newSpecies = RH_StarterForSlot(starterSlot);
        newSpecies = S->trainersEvolveOn ? EvolveForLevel(newSpecies, mon->lvl, 0) : RH_EvolveTimes(newSpecies, stage);
        changed = newSpecies != mon->species;
    }
    else if (IsLeagueClass(trainer->trainerClass) && S->leagueUnique && slot < S->leagueUnique && trainerId >= 0)
    {
        u32 idx, member;
        EnsureReserved();
        switch (trainer->trainerPic)
        {
        case TRAINER_PIC_ELITE_FOUR_LORELEI_FRLG: member = 0; break;
        case TRAINER_PIC_ELITE_FOUR_BRUNO_FRLG:   member = 1; break;
        case TRAINER_PIC_ELITE_FOUR_AGATHA_FRLG:  member = 2; break;
        case TRAINER_PIC_ELITE_FOUR_LANCE_FRLG:   member = 3; break;
        default:                                  member = 4; break;
        }
        idx = member * S->leagueUnique + slot;
        if (idx < sReserved.count)
        {
            newSpecies = sReserved.species[idx];
            changed = TRUE;
        }
    }
    if (!changed && S->trainers != 0)
    {
        newSpecies = ChooseSpecies(mon, trainer, trainerId, slot, tier);
        if (newSpecies == SPECIES_NONE)
            newSpecies = mon->species;
        if (S->trainersEvolveOn && !IsRivalClass(trainer->trainerClass))
            newSpecies = EvolveForLevel(newSpecies, mon->lvl, slot);
        changed = TRUE;
    }
    else if (!changed && S->trainersEvolveOn)
    {
        newSpecies = EvolveForLevel(mon->species, mon->lvl, slot);
    }

    if (newSpecies != mon->species)
    {
        u32 i;
        mon->species = newSpecies;
        for (i = 0; i < MAX_MON_MOVES; i++)
            mon->moves[i] = MOVE_NONE;               // default moves for the new species
        mon->ability = ABILITY_NONE;
        mon->nickname = NULL;
    }
    if (S->abilities && mon->ability != ABILITY_NONE)
        mon->ability = ABILITY_NONE;                 // hand-picked abilities don't exist on randomized species
    if (S->noEarlyWonderGuard && mon->lvl < 20
     && (GetSpeciesAbility(mon->species, 0) == ABILITY_WONDER_GUARD || GetSpeciesAbility(mon->species, 1) == ABILITY_WONDER_GUARD
      || GetSpeciesAbility(mon->species, 2) == ABILITY_WONDER_GUARD))
    {
        u32 a;
        for (a = 0; a < NUM_ABILITY_SLOTS; a++)
        {
            u32 ab = GetSpeciesAbility(mon->species, a);
            if (ab != ABILITY_NONE && ab != ABILITY_WONDER_GUARD)
            {
                mon->ability = ab;
                break;
            }
        }
    }
    if (S->movesets == 3)
    {
        u32 i;
        mon->moves[0] = MOVE_METRONOME;
        for (i = 1; i < MAX_MON_MOVES; i++)
            mon->moves[i] = MOVE_NONE;
    }
    else if (S->betterMovesets[tier])
    {
        BetterMoveset(mon);
    }
    else if (S->movesets != 0)
    {
        u32 i;
        for (i = 0; i < MAX_MON_MOVES; i++)
            mon->moves[i] = MOVE_NONE;               // hand-picked moves may not exist in the random learnset
    }
    if (S->heldItemsFor[tier])
    {
        bool32 give = TRUE;
        if (S->heldHighestOnly)
        {
            u32 i;
            for (i = 0; i < trainer->partySize; i++)
                if (trainer->party[i].lvl > trainer->party[slot < trainer->partySize ? slot : 0].lvl)
                    give = FALSE;
            if (slot >= trainer->partySize)
                give = FALSE;
        }
        if (give)
        {
            u32 h = RH_Hash(SALT_HELD_ITEM, trainerId, slot);
            mon->heldItem = S->heldSensible ? SensibleItem(mon, h) : RH_RandomHeldItem(h, TRUE, S->heldConsumableOnly);
        }
    }
    if (sTeamCount < PARTY_SIZE)
        sTeam[sTeamCount++] = mon->species;
}

// Adds "Additional Pokemon" (and a second Pokemon when a single-Pokemon trainer must fight a double battle).
void RH_FinishTrainerParty(struct Pokemon *party, const struct Trainer *trainer, u32 *count, struct TrainerGenerator *gen)
{
    s32 trainerId;
    u32 tier, want, i;
    if (!S->enabled)
        return;
    trainerId = TrainerIdOf(trainer);
    if (trainerId < 0 || *count == 0)
        return;
    tier = TierOf(trainer->trainerClass);
    want = *count + S->additionalMons[tier];
    if (RH_TrainerBattleType(trainerId, trainer->battleType) == TRAINER_BATTLE_TYPE_DOUBLES && want < 2)
        want = 2;
    if (want > PARTY_SIZE)
        want = PARTY_SIZE;
    for (i = *count; i < want; i++)
    {
        struct TrainerMon extra = trainer->party[(i - 1) % trainer->partySize];
        u32 j;
        extra.species = RH_PickSpecies(&(struct RhFilter){ .legend = S->trainerNoLegends ? 1 : 0 },
                                       RH_Hash(SALT_EXTRA_MON, trainerId, i), trainer->party[(i - 1) % trainer->partySize].species);
        extra.nickname = NULL;
        extra.heldItem = ITEM_NONE;
        extra.ability = ABILITY_NONE;
        for (j = 0; j < MAX_MON_MOVES; j++)
            extra.moves[j] = MOVE_NONE;
        GenerateMonFromTrainerMon(&party[i], &extra, gen);
    }
    *count = want;
}

// ---------------------------------------------------------------------------
// Battle style / names
// ---------------------------------------------------------------------------
enum TrainerBattleType RH_TrainerBattleType(u16 trainerId, enum TrainerBattleType vanilla)
{
    if (!S->enabled || S->battleStyle == 0 || trainerId >= TRAINERS_COUNT)
        return vanilla;
    if (S->battleStyle == 2)
        return S->battleStyleDoubles ? TRAINER_BATTLE_TYPE_DOUBLES : TRAINER_BATTLE_TYPE_SINGLES;
    return (RH_Hash(SALT_BATTLE_STYLE, trainerId, 0) & 1) ? TRAINER_BATTLE_TYPE_DOUBLES : TRAINER_BATTLE_TYPE_SINGLES;
}

static bool32 KeepsName(u32 trainerClass)
{
    return TierOf(trainerClass) != TIER_REGULAR;     // leaders, Elite Four, rival, Giovanni keep their names
}

const u8 *RH_TrainerName(u16 trainerId, const u8 *vanilla)
{
    const struct Trainer *t;
    if (!S->enabled || !S->randomTrainerNames || trainerId >= TRAINERS_COUNT)
        return vanilla;
    t = GetTrainerStructFromId(trainerId);
    if (KeepsName(t->trainerClass) || vanilla[0] == EOS)
        return vanilla;
    return sRhTrainerNames[RH_Hash(SALT_TRAINER_NAME, trainerId, 0) % ARRAY_COUNT(sRhTrainerNames)];
}

const u8 *RH_TrainerClassName(u32 trainerClass, const u8 *vanilla)
{
    if (!S->enabled || !S->randomTrainerClassNames || KeepsName(trainerClass))
        return vanilla;
    return sRhTrainerClassNames[RH_Hash(SALT_TRAINER_CLASS, trainerClass, 0) % ARRAY_COUNT(sRhTrainerClassNames)];
}
