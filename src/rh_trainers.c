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


// FVX tags for FRLG: Boss = Gym Leaders, Giovanni, Elite Four, Champion; Important = the rival's battles.
static u32 TierOf(u32 trainerClass)
{
    switch (trainerClass)
    {
    case TRAINER_CLASS_LEADER_FRLG: case TRAINER_CLASS_ELITE_FOUR_FRLG: case TRAINER_CLASS_CHAMPION_FRLG:
    case TRAINER_CLASS_LEADER: case TRAINER_CLASS_ELITE_FOUR: case TRAINER_CLASS_CHAMPION:
    case TRAINER_CLASS_BOSS_FRLG:
        return TIER_BOSS;
    case TRAINER_CLASS_RIVAL_EARLY_FRLG: case TRAINER_CLASS_RIVAL_LATE_FRLG:
        return TIER_IMPORTANT;
    default:
        return TIER_REGULAR;
    }
}

// FVX never changes the first rival battle's style / size / items (Oak's tutorial battle).
static bool32 IsFirstRivalBattle(s32 trainerId)
{
    return trainerId == TRAINER_RIVAL_OAKS_LAB_SQUIRTLE || trainerId == TRAINER_RIVAL_OAKS_LAB_BULBASAUR
        || trainerId == TRAINER_RIVAL_OAKS_LAB_CHARMANDER;
}

// The main-game Pokemon League (for "Pokemon League Has Unique Pokemon"; the rematches are post-game).
static const u16 sLeagueMembers[] = {
    TRAINER_ELITE_FOUR_LORELEI, TRAINER_ELITE_FOUR_BRUNO, TRAINER_ELITE_FOUR_AGATHA, TRAINER_ELITE_FOUR_LANCE,
    TRAINER_CHAMPION_FIRST_SQUIRTLE,
};

static s32 LeagueMemberOf(s32 trainerId)
{
    u32 i;
    if (trainerId == TRAINER_CHAMPION_FIRST_BULBASAUR || trainerId == TRAINER_CHAMPION_FIRST_CHARMANDER)
        return 4;
    // the post-game rematches are the same people: they use their unique Pokemon too
    if (trainerId >= TRAINER_ELITE_FOUR_LORELEI_2 && trainerId <= TRAINER_ELITE_FOUR_LANCE_2)
        return trainerId - TRAINER_ELITE_FOUR_LORELEI_2;
    if (trainerId >= TRAINER_CHAMPION_REMATCH_SQUIRTLE && trainerId <= TRAINER_CHAMPION_REMATCH_CHARMANDER)
        return 4;
    for (i = 0; i < ARRAY_COUNT(sLeagueMembers); i++)
        if (sLeagueMembers[i] == trainerId)
            return i;
    return -1;
}

static bool32 IsRivalClass(u32 trainerClass);
static bool32 CarriesStarter(void);

// Is this slot the rival's starter, which keeps being his starter?
static bool32 IsCarriedStarterSlot(const struct Trainer *t, u32 slot)
{
    u32 stage;
    return IsRivalClass(t->trainerClass) && CarriesStarter() && RH_StarterFamily(t->party[slot].species, &stage) >= 0;
}

// Rank of a party slot by level (0 = highest; ties: the Pokemon further back ranks higher, FVX). The rival's
// carried starter doesn't count (FVX: the Champion's starter isn't one of his unique Pokemon).
static u32 LevelRank(const struct Trainer *t, u32 slot)
{
    u32 r = 0, i;
    if (IsCarriedStarterSlot(t, slot))
        return PARTY_SIZE;
    for (i = 0; i < t->partySize; i++)
        if (!IsCarriedStarterSlot(t, i)
         && (t->party[i].lvl > t->party[slot].lvl || (t->party[i].lvl == t->party[slot].lvl && i > slot)))
            r++;
    return r;
}

// "Highest Level Only": the highest-level Pokemon (the first one on ties, FVX), the rival's starter included.
static bool32 IsHighestLevelSlot(const struct Trainer *t, u32 slot)
{
    u32 i;
    for (i = 0; i < t->partySize; i++)
        if (t->party[i].lvl > t->party[slot].lvl || (t->party[i].lvl == t->party[slot].lvl && i < slot))
            return FALSE;
    return TRUE;
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

// A type shared by every Pokemon of the trainer's vanilla team (FVX: a single Pokemon counts as themed; a shared
// Normal type loses to another shared type, so Normal/Flying bird teams become Flying).
static u8 TeamTheme(const struct Trainer *trainer)
{
    u32 t, i;
    u8 found = TYPE_NONE;
    if (trainer->partySize < 1)
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
        if (i == trainer->partySize && (found == TYPE_NONE || found == TYPE_NORMAL))
            found = type;
    }
    return found;
}

// A random theme type. "Weight Types by # of Pokemon": in proportion to how many Pokemon have each type.
// Types with (almost) no Pokemon in the pool are never picked (FVX).
static u8 RandomTheme(u32 hash)
{
    u32 total = 0, i, r;
        for (i = 0; i < 18; i++)
        if (RH_TypeCount(i) >= 3)
            total += S->trainerWeightTypes ? RH_TypeCount(i) : 1;
    if (total == 0)
        return RH_RandomMonType(hash);
    r = hash % total;
    for (i = 0; i < 18; i++)
    {
        u32 w;
        if (RH_TypeCount(i) < 3)
            continue;
        w = S->trainerWeightTypes ? RH_TypeCount(i) : 1;
        if (r < w)
            return gRhMonTypes[i];
        r -= w;
    }
    return gRhMonTypes[0];
}

// Gyms and Elite Four members get different random types (a keyed permutation of their official types).
// Types with (almost) no Pokemon in the pool are skipped by cycle walking, which keeps the groups' types different.
static u8 GroupTheme(u8 officialType)
{
    u32 x = RH_Permute(SALT_TRAINER_THEME, RH_TypeIndexOf(officialType), 18), tries;
        for (tries = 0; tries < 18 && RH_TypeCount(x) < 3; tries++)
        x = RH_Permute(SALT_TRAINER_THEME, x, 18);
    return gRhMonTypes[x];
}

// ---------------------------------------------------------------------------
// Team context (diverse types / avoid duplicates / league-unique Pokemon / local Pokemon)
// ---------------------------------------------------------------------------
static EWRAM_DATA const struct Trainer *sTeamTrainer = NULL;
static EWRAM_DATA u16 sTeam[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sTeamCount = 0;
static EWRAM_DATA u8 sDiverse = 0;
static EWRAM_DATA u8 sAvoidDupes = 0;
static EWRAM_DATA u8 sNoWonderGuard = 0;
static EWRAM_DATA u8 sLeagueMember = 0;                        // league member + 1 while building its team

static bool32 IsOwnReservedFamily(u16 species);

static bool32 TeamPredicate(u16 species)
{
    u32 i;
    if (sLeagueMember == 0 && RH_IsLeagueReserved(species))
        return FALSE;                                        // kept for the Elite Four / Champion
    if (sLeagueMember != 0 && sAvoidDupes && IsOwnReservedFamily(species))
        return FALSE;                                        // their unique Pokemon are on the team already
    for (i = 0; i < sTeamCount; i++)
    {
        if (sAvoidDupes && RH_FamilyRoot(sTeam[i]) == RH_FamilyRoot(species))
            return FALSE;
        if (sDiverse && (RH_SpeciesHasType(species, GetSpeciesType(sTeam[i], 0)) || RH_SpeciesHasType(species, GetSpeciesType(sTeam[i], 1))))
            return FALSE;
    }
    return TRUE;
}

static bool32 HasOnlyWonderGuard(u16 species)
{
    return GetSpeciesAbility(species, 0) == ABILITY_WONDER_GUARD
        && (GetSpeciesAbility(species, 1) == ABILITY_WONDER_GUARD || GetSpeciesAbility(species, 1) == ABILITY_NONE)
        && (GetSpeciesAbility(species, 2) == ABILITY_WONDER_GUARD || GetSpeciesAbility(species, 2) == ABILITY_NONE);
}

static bool32 TeamPredicateWG(u16 species)
{
    if (sNoWonderGuard && HasOnlyWonderGuard(species))
        return FALSE;
    return TeamPredicate(species);
}

// ---------------------------------------------------------------------------
// League-unique Pokemon: the Elite Four's / Champion's highest-level Pokemon are used by no other trainer.
// ---------------------------------------------------------------------------
#define MAX_RESERVED 10
struct ReservedCache { u32 key; u8 count; u16 species[MAX_RESERVED]; };
static EWRAM_DATA struct ReservedCache sReserved = {0};

static u32 ReservedKey(void)
{
    return (RH_SettingsHash() + 7) | 1;   // every setting (0 = never valid)
}

static u8 MemberTheme(u32 member)
{
    const struct Trainer *tr = GetTrainerStructFromId(sLeagueMembers[member]);
    u8 official = TeamTheme(tr);
    static const u8 sOfficial[] = { TYPE_ICE, TYPE_FIGHTING, TYPE_GHOST, TYPE_DRAGON, TYPE_NONE };
    switch (S->trainers)
    {
    case 3:
        if (sOfficial[member] == TYPE_NONE)                  // the Champion: his own random theme (see TrainerTheme)
            return RandomTheme(RH_Hash(SALT_TRAINER_THEME, sLeagueMembers[member], 0));
        // fallthrough
    case 4:
        return sOfficial[member] != TYPE_NONE ? GroupTheme(sOfficial[member]) : TYPE_NONE;
    case 5: case 6:
        return sOfficial[member] != TYPE_NONE ? sOfficial[member] : official;
    }
    return TYPE_NONE;
}

static bool32 NotStarterFamily(u16 species)
{
    u32 i;
    if (!CarriesStarter())
        return TRUE;
    for (i = 0; i < 3; i++)
        if (RH_FamilyRoot(RH_StarterForSlot(i)) == RH_FamilyRoot(species))
            return FALSE;
    return TRUE;
}

static void BuildReserved(void)
{
    u32 member, rank, slot;
    u8 savedMember = sLeagueMember;
    sReserved.key = ReservedKey();
    sReserved.count = 0;
    sLeagueMember = 1;                                       // the reserved list itself may use reserved species
    for (member = 0; member < ARRAY_COUNT(sLeagueMembers); member++)
    {
        const struct Trainer *tr = GetTrainerStructFromId(sLeagueMembers[member]);
        for (rank = 0; rank < S->leagueUnique && sReserved.count < MAX_RESERVED; rank++)
        {
            struct RhFilter f = {0};
            u32 j;
            u16 sp, orig = SPECIES_NONE;
            for (slot = 0; slot < tr->partySize; slot++)
                if (LevelRank(tr, slot) == rank)
                    orig = tr->party[slot].species;
            if (orig == SPECIES_NONE)
                continue;
            if (S->trainerNoLegends)
                f.legend = 1;
            f.type = MemberTheme(member);
            f.extra = NotStarterFamily;                      // FVX: the rival's starters are never league-unique
            for (j = 0; j < sReserved.count; j++)
                RH_FilterExclude(&f, sReserved.species[j]);
            {
                // the league's unique Pokemon stay different from each other and keep the member's theme; the
                // rival's starter families are avoided when possible (tiny pools can't always)
                struct RhFilter g = f;
                u32 h = RH_Hash(SALT_LEAGUE, member * 8 + rank, 0);
                u16 similar = S->trainerSimilarStrength ? orig : SPECIES_NONE;
                sp = RH_PickWithFilter(&g, h);
                if (sp != SPECIES_NONE && similar != SPECIES_NONE)
                {
                    g = f;
                    sp = RH_PickSpecies(&g, h, similar);
                    if (!RH_FilterAccepts(&f, RH_PoolIndexOf(sp)))
                        sp = RH_PickWithFilter(&f, h);
                }
                if (sp == SPECIES_NONE)
                {
                    g = f;
                    g.extra = NULL;                          // allow a starter family
                    sp = RH_PickWithFilter(&g, h);
                }
                if (sp == SPECIES_NONE)
                {
                    g = f;
                    g.extra = NULL;
                    g.type = TYPE_NONE;                      // then drop the theme, but never repeat one
                    sp = RH_PickWithFilter(&g, h);
                }
                if (sp == SPECIES_NONE)
                {
                    g = f;
                    sp = RH_PickSpecies(&g, h, similar);
                }
            }
            sReserved.species[sReserved.count++] = sp;
        }
    }
    sLeagueMember = savedMember;
}

static void EnsureReserved(void)
{
    if (sReserved.key != ReservedKey())
        BuildReserved();
}

// Is "species" of the same family as one of the unique Pokemon of the league member being built?
static bool32 IsOwnReservedFamily(u16 species)
{
    u32 r, member = sLeagueMember - 1;
    EnsureReserved();
    for (r = 0; r < S->leagueUnique; r++)
    {
        u32 idx = member * S->leagueUnique + r;
        if (idx < sReserved.count && RH_FamilyRoot(sReserved.species[idx]) == RH_FamilyRoot(species))
            return TRUE;
    }
    return FALSE;
}

bool32 RH_IsLeagueReserved(u16 species)
{
    u32 i;
    if (!S->enabled || !S->leagueUnique || S->trainers == 0)
        return FALSE;
    EnsureReserved();
    for (i = 0; i < sReserved.count; i++)
        if (sReserved.species[i] == species)
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Evolving trainer Pokemon to their level
// ---------------------------------------------------------------------------
static EWRAM_DATA u8 sEvolveKeepType = TYPE_NONE;             // type theme the evolution must keep

static u16 EvolveForLevelEx(u16 species, u32 level, u32 salt, u32 *steps)
{
    u32 i, guard;
    u32 fullBy = S->trainersEvolveOn ? S->trainersEvolveLevel : 0;
    for (guard = 0; guard < 3; guard++)
    {
        const struct Evolution *e = GetSpeciesEvolutions(species);
        u16 next = SPECIES_NONE, legal[8];
        u32 n = 0;
        bool32 ok = FALSE;
        // FVX: pick among the evolutions that are possible at this level (not a branch that isn't reached yet)
        for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        {
            u16 t = e[i].targetSpecies;
            bool32 can;
            if (e[i].method == EVO_NONE || t == SPECIES_NONE || t == species || n >= ARRAY_COUNT(legal))
                continue;
            if (e[i].method == EVO_LEVEL || e[i].method == EVO_LEVEL_BATTLE_ONLY)
                can = level >= e[i].param && e[i].param > 1;
            else
                can = fullBy && level >= min(RH_EstimateEvoLevel(species, t), fullBy);   // FVX: estimated evo level
            if (fullBy && level >= fullBy)
                can = TRUE;
            if (can)
                legal[n++] = t;
        }
        if (n > 0)
        {
            next = legal[RH_Hash(SALT_EVO_LEVEL, species, salt) % n];
            ok = TRUE;
        }
        if (!ok || next == SPECIES_NONE)
            break;
        if (sLeagueMember == 0 && RH_IsLeagueReserved(next))
            break;                                           // the Elite Four's unique Pokemon stay theirs
        if (sEvolveKeepType != TYPE_NONE && !RH_SpeciesHasType(next, sEvolveKeepType))
            break;                                           // a themed trainer's Pokemon keep the theme
        species = next;
        (*steps)++;
    }
    return species;
}

static u16 EvolveForLevel(u16 species, u32 level, u32 salt)
{
    u32 steps = 0;
    return EvolveForLevelEx(species, level, salt, &steps);
}

// ---------------------------------------------------------------------------
// Use Local Pokemon: trainers use the (randomized) wild Pokemon of the area they stand in, evolved to their level.
// (FVX uses the wild Pokemon of the whole game; working that out for every map would stall the GBA for seconds.)
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
    const struct WildPokemonInfo *infos[3];
    static const u8 sCounts[3] = { 12, 5, 10 };
    static const u8 sAreas[3] = { WILD_AREA_LAND, WILD_AREA_WATER, WILD_AREA_FISHING };
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
    infos[2] = h->encounterTypes[0].fishingMonsInfo;
    for (i = 0; i < 3; i++)
        if (infos[i] != NULL)
            total += sCounts[i];
    if (total == 0)
        return SPECIES_NONE;
    r = hash % total;
    for (i = 0; i < 3; i++)
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
static const u16 sSetupMoves[] = {
    MOVE_SWORDS_DANCE, MOVE_NASTY_PLOT, MOVE_CALM_MIND, MOVE_DRAGON_DANCE, MOVE_BULK_UP, MOVE_QUIVER_DANCE,
    MOVE_RECOVER, MOVE_ROOST, MOVE_SOFT_BOILED, MOVE_TOXIC, MOVE_THUNDER_WAVE, MOVE_WILL_O_WISP, MOVE_SPORE,
    MOVE_SLEEP_POWDER, MOVE_LEECH_SEED, MOVE_PROTECT, MOVE_SUBSTITUTE, MOVE_STEALTH_ROCK, MOVE_AGILITY,
};

static const u16 sTutorMovesForTrainers[] = {
    MOVE_DOUBLE_EDGE, MOVE_THUNDER_WAVE, MOVE_ROCK_SLIDE, MOVE_EXPLOSION, MOVE_MEGA_PUNCH, MOVE_MEGA_KICK,
    MOVE_DREAM_EATER, MOVE_SOFT_BOILED, MOVE_SUBSTITUTE, MOVE_SWORDS_DANCE, MOVE_SEISMIC_TOSS, MOVE_COUNTER,
    MOVE_METRONOME, MOVE_MIMIC, MOVE_BODY_SLAM,
};

static void BetterMoveset(struct TrainerMon *mon)
{
    u16 best[3] = {0};
    u32 score[3] = {0};
    u8 bestType[3] = {TYPE_NONE, TYPE_NONE, TYPE_NONE};
    u16 status = MOVE_NONE;
    u32 i, j, k, n = 0;
    const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(mon->species);
    u16 candidates[96];
    u32 count = 0;
    bool32 physical = GetSpeciesBaseAttack(mon->species) >= GetSpeciesBaseSpAttack(mon->species);

    // candidates: level-up moves known by now + TMs + tutor moves it can learn (FVX)
    u32 levelUpCount;
    for (i = 0; l != NULL && l[i].move != LEVEL_UP_MOVE_END && count < ARRAY_COUNT(candidates); i++)
        if (l[i].level <= mon->lvl)
            candidates[count++] = l[i].move;
    levelUpCount = count;
    for (i = 1; i <= NUM_TECHNICAL_MACHINES && count < ARRAY_COUNT(candidates); i++)
    {
        u16 m = GetTMHMMoveId(i);
        if (CanLearnTeachableMove(mon->species, m))
            candidates[count++] = m;
    }
    for (i = 0; i < ARRAY_COUNT(sTutorMovesForTrainers) && count < ARRAY_COUNT(candidates); i++)
    {
        u16 m = RH_TutorMove(sTutorMovesForTrainers[i]);
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
            if ((GetMoveCategory(m) == DAMAGE_CATEGORY_PHYSICAL) == physical)
                s = s * 5 / 4;                               // synergy with the better attacking stat
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
    // top up with the newest remaining level-up moves if needed
    for (i = levelUpCount; i > 0 && n < MAX_MON_MOVES; i--)
    {
        u16 m = candidates[i - 1];
        for (j = 0; j < n; j++)
            if (mon->moves[j] == m)
                break;
        if (j == n && m != MOVE_NONE)
            mon->moves[n++] = m;
    }
}

// The moves the Pokemon will actually have (its set moves, or the default level-up ones).
static u32 EffectiveMoves(const struct TrainerMon *mon, u16 *moves)
{
    u32 i, n = 0;
    const struct LevelUpMove *l;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (mon->moves[i] != MOVE_NONE)
            moves[n++] = mon->moves[i];
    if (n > 0)
        return n;
    l = GetSpeciesLevelUpLearnset(mon->species);
    for (i = 0; l != NULL && l[i].move != LEVEL_UP_MOVE_END; i++)
    {
        if (l[i].level > mon->lvl)
            break;
        if (n < MAX_MON_MOVES)
            moves[n++] = l[i].move;
        else
        {
            moves[0] = moves[1]; moves[1] = moves[2]; moves[2] = moves[3]; moves[3] = l[i].move;
        }
    }
    return n;
}

static const u16 sTypeBoostItems[] = {
    [TYPE_NORMAL] = ITEM_SILK_SCARF, [TYPE_FIGHTING] = ITEM_BLACK_BELT, [TYPE_FLYING] = ITEM_SHARP_BEAK,
    [TYPE_POISON] = ITEM_POISON_BARB, [TYPE_GROUND] = ITEM_SOFT_SAND, [TYPE_ROCK] = ITEM_HARD_STONE,
    [TYPE_BUG] = ITEM_SILVER_POWDER, [TYPE_GHOST] = ITEM_SPELL_TAG, [TYPE_STEEL] = ITEM_METAL_COAT,
    [TYPE_FIRE] = ITEM_CHARCOAL, [TYPE_WATER] = ITEM_MYSTIC_WATER, [TYPE_GRASS] = ITEM_MIRACLE_SEED,
    [TYPE_ELECTRIC] = ITEM_MAGNET, [TYPE_PSYCHIC] = ITEM_TWISTED_SPOON, [TYPE_ICE] = ITEM_NEVER_MELT_ICE,
    [TYPE_DRAGON] = ITEM_DRAGON_FANG, [TYPE_DARK] = ITEM_BLACK_GLASSES, [TYPE_FAIRY] = ITEM_FAIRY_FEATHER,
};

static const u16 sResistBerries[] = {
    [TYPE_NORMAL] = ITEM_CHILAN_BERRY, [TYPE_FIGHTING] = ITEM_CHOPLE_BERRY, [TYPE_FLYING] = ITEM_COBA_BERRY,
    [TYPE_POISON] = ITEM_KEBIA_BERRY, [TYPE_GROUND] = ITEM_SHUCA_BERRY, [TYPE_ROCK] = ITEM_CHARTI_BERRY,
    [TYPE_BUG] = ITEM_TANGA_BERRY, [TYPE_GHOST] = ITEM_KASIB_BERRY, [TYPE_STEEL] = ITEM_BABIRI_BERRY,
    [TYPE_FIRE] = ITEM_OCCA_BERRY, [TYPE_WATER] = ITEM_PASSHO_BERRY, [TYPE_GRASS] = ITEM_RINDO_BERRY,
    [TYPE_ELECTRIC] = ITEM_WACAN_BERRY, [TYPE_PSYCHIC] = ITEM_PAYAPA_BERRY, [TYPE_ICE] = ITEM_YACHE_BERRY,
    [TYPE_DRAGON] = ITEM_HABAN_BERRY, [TYPE_DARK] = ITEM_COLBUR_BERRY, [TYPE_FAIRY] = ITEM_ROSELI_BERRY,
};

// FVX "Sensible Items": a random pick among items that make sense for this Pokemon and its moves.
static u16 SensibleItem(const struct TrainerMon *mon, u32 hash)
{
    u16 list[40], moves[MAX_MON_MOVES];
    u32 n = 0, i, t, nMoves, damaging = 0, physical = 0;
    u16 sp = mon->species;
    u32 spe = GetSpeciesBaseSpeed(sp), bulk = GetSpeciesBaseHP(sp) + GetSpeciesBaseDefense(sp) + GetSpeciesBaseSpDefense(sp);
    bool32 consumable = S->heldConsumableOnly;

    nMoves = EffectiveMoves(mon, moves);
    for (i = 0; i < nMoves; i++)
    {
        if (!RH_IsDamagingMove(moves[i]))
            continue;
        damaging++;
        physical += (GetMoveCategory(moves[i]) == DAMAGE_CATEGORY_PHYSICAL);
        t = GetMoveType(moves[i]);
        if (!consumable && t < ARRAY_COUNT(sTypeBoostItems) && sTypeBoostItems[t] != ITEM_NONE)
            list[n++] = sTypeBoostItems[t];                  // Silk Scarf only with a damaging Normal move
    }
    // resist berries for types this Pokemon is weak to
    for (t = 0; t < 18 && n < ARRAY_COUNT(list) - 12; t++)
    {
        u8 type = gRhMonTypes[t];
        uq4_12_t m = GetTypeModifier(type, GetSpeciesType(sp, 0));
        if (GetSpeciesType(sp, 1) != GetSpeciesType(sp, 0))
            m = uq4_12_multiply(m, GetTypeModifier(type, GetSpeciesType(sp, 1)));
        if (m > UQ_4_12(1.0) && type < ARRAY_COUNT(sResistBerries))
            list[n++] = sResistBerries[type];
    }
    list[n++] = ITEM_SITRUS_BERRY;
    list[n++] = ITEM_LUM_BERRY;
    list[n++] = ITEM_WHITE_HERB;
    if (bulk < 220 && spe >= 80)
        list[n++] = ITEM_FOCUS_SASH;
    if (!consumable)
    {
        list[n++] = ITEM_LEFTOVERS;
        if (damaging > 0)
            list[n++] = ITEM_LIFE_ORB;
        if (RH_EvolveOnce(sp, 0) != SPECIES_NONE)
            list[n++] = ITEM_EVIOLITE;
        if (damaging == nMoves && nMoves >= 2)
        {
            list[n++] = ITEM_ASSAULT_VEST;
            list[n++] = ITEM_CHOICE_SCARF;
            if (physical == damaging)
                list[n++] = ITEM_CHOICE_BAND;
            if (physical == 0)
                list[n++] = ITEM_CHOICE_SPECS;
        }
    }
    for (i = 0; i < n; i++)
        if (RH_ItemBanned(list[i]))
            list[i] = ITEM_SITRUS_BERRY;
    return list[hash % n];
}

// ---------------------------------------------------------------------------
// Species choice
// ---------------------------------------------------------------------------
// "Random (even distribution)": the index-th allowed Pokemon in a keyed order, so every Pokemon is used about
// equally often.
static u16 PickEven(const struct RhFilter *f, u32 index)
{
    return RH_PickRanked(f, index, SALT_TRAINER);
}

// The type theme of a trainer (TYPE_NONE = none), and for "Keep Themes Or Primary" the per-Pokemon type.
static u8 TrainerTheme(const struct Trainer *trainer, s32 trainerId)
{
    u8 gym = GymTheme(trainerId);
    switch (S->trainers)
    {
    case 3:     // type themed: every trainer gets a theme; gym / Elite Four groups share theirs
        if (gym != TYPE_NONE)
            return GroupTheme(gym);
        if (LeagueMemberOf(trainerId) == 4)                  // all three Champion battles share one theme
            trainerId = sLeagueMembers[4];
        return RandomTheme(RH_Hash(SALT_TRAINER_THEME, trainerId, 0));
    case 4:     // themes for gyms and the Elite Four only
        return gym != TYPE_NONE ? GroupTheme(gym) : TYPE_NONE;
    case 5:     // keep type-themed trainers' themes (gyms / Elite Four: their official type)
    case 6:
        return gym != TYPE_NONE ? gym : TeamTheme(trainer);
    }
    return TYPE_NONE;
}

static void SetupTeamFilter(struct RhFilter *f, const struct TrainerMon *mon, u32 tier, u8 theme)
{
    if (S->trainerNoLegends)
        f->legend = 1;
    f->allowVariants = S->trainerAllowAltFormes;
    sAvoidDupes = S->trainerAvoidDuplicates;
    sDiverse = S->diverseTypes[tier] && theme == TYPE_NONE; // FVX: ignored for type-themed trainers
    sNoWonderGuard = S->noEarlyWonderGuard && mon->lvl < 20;
    f->extra = TeamPredicateWG;
    f->noLeagueReserved = (sLeagueMember == 0);             // kept even when the other rules are relaxed
    f->type = theme;
    // "No Premature Evolutions" (FVX): nothing evolved further than its level allows
    if (S->noPrematureEvos && !(S->trainersEvolveOn && mon->lvl >= S->trainersEvolveLevel))
        f->legalAtLevel = min(mon->lvl, 255);
}

static u16 ChooseSpecies(const struct TrainerMon *mon, const struct Trainer *trainer, s32 trainerId, u32 slot, u32 tier)
{
    struct RhFilter f = {0};
    u16 similar = SPECIES_NONE, result = SPECIES_NONE;
    u32 key = (trainerId >= 0 ? trainerId : 0xFFF) * 8 + slot;
    u8 theme = TrainerTheme(trainer, trainerId);
    u32 tries;

    SetupTeamFilter(&f, mon, tier, theme);
    if (theme == TYPE_NONE && S->trainers == 6)
    {
        f.type = gSpeciesInfo[mon->species].types[0];      // "or primary": the replacement has the original's primary type
        sDiverse = 0;
    }
    if (S->trainerSimilarStrength)
        similar = mon->species;

    if (S->trainerLocalPokemon)
    {
        for (tries = 0; tries < 8; tries++)
        {
            result = LocalSpecies(RH_Hash(SALT_TRAINER, key, 1 + tries), mon->lvl);
            if (result != SPECIES_NONE && (f.type == TYPE_NONE || RH_SpeciesHasType(result, f.type))
             && !(f.legend == 1 && RH_IsLegendary(result)) && TeamPredicateWG(result))
                return result;
        }
    }
    if (S->trainers == 2)
    {
        if (similar != SPECIES_NONE)
        {
            // similar strength takes precedence (FVX): even distribution inside the +-20% window
            u32 bst = RH_SpeciesBST(similar);
            f.minBst = bst * 80 / 100;
            f.maxBst = bst * 120 / 100;
        }
        result = PickEven(&f, key);
        if (result != SPECIES_NONE)
            return result;
        f.minBst = f.maxBst = 0;
    }
    return RH_PickSpecies(&f, RH_Hash(SALT_TRAINER, key, 0), similar);
}

// ---------------------------------------------------------------------------
// Hook: called for every trainer Pokemon before it is created.
// ---------------------------------------------------------------------------
static bool32 CarriesStarter(void)
{
    // With starters randomized the rival always matches them; with trainers randomized only when "Rival Carries
    // Starter" is on (otherwise his team is random like everyone else's).
    if (S->trainers == 0)
        return S->starters != 0;
    return S->rivalCarriesTeam;
}

void RH_ModifyTrainerMon(struct TrainerMon *mon, const struct Trainer *trainer, u32 slot)
{
    s32 trainerId, member;
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
    member = (S->leagueUnique && S->trainers != 0) ? LeagueMemberOf(trainerId) : -1;
    sLeagueMember = member + 1;

    if (S->trainerLevelModOn && !(IsFirstRivalBattle(trainerId) && S->trainerLevelMod > 0))
        mon->lvl = min(max(RH_ApplyPercent(mon->lvl, S->trainerLevelMod), 1), MAX_LEVEL);

    starterSlot = RH_StarterFamily(mon->species, &stage);
    if (starterSlot >= 0 && slot < trainer->partySize && IsRivalClass(trainer->trainerClass) && CarriesStarter())
    {
        // The rival uses the starter the player didn't pick, evolved like the original (or further by level).
        u32 steps = 0;
        newSpecies = RH_StarterForSlot(starterSlot);
        if (S->trainersEvolveOn)
            newSpecies = EvolveForLevelEx(newSpecies, mon->lvl, 0, &steps);
        if (steps < stage)
            newSpecies = RH_EvolveTimes(newSpecies, stage - steps);
        changed = TRUE;
    }
    else if (member >= 0 && slot < trainer->partySize && LevelRank(trainer, slot) < S->leagueUnique)
    {
        u32 idx;
        EnsureReserved();
        idx = member * S->leagueUnique + LevelRank(trainer, slot);
        if (idx < sReserved.count && sReserved.species[idx] != SPECIES_NONE)
        {
            newSpecies = sReserved.species[idx];
            if (S->trainersEvolveOn)
                newSpecies = EvolveForLevel(newSpecies, mon->lvl, slot);
            changed = TRUE;
        }
    }
    if (!changed && S->trainers != 0)
    {
        newSpecies = ChooseSpecies(mon, trainer, trainerId, slot, tier);
        if (newSpecies == SPECIES_NONE)
            newSpecies = mon->species;
        if (S->trainersEvolveOn)
        {
            sEvolveKeepType = TrainerTheme(trainer, trainerId);
            if (sEvolveKeepType == TYPE_NONE && S->trainers == 6)
                sEvolveKeepType = gSpeciesInfo[mon->species].types[0];
            newSpecies = EvolveForLevel(newSpecies, mon->lvl, slot);
            sEvolveKeepType = TYPE_NONE;
        }
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
    if (S->heldItemsFor[tier] && !IsFirstRivalBattle(trainerId))
    {
        bool32 give = TRUE;
        if (S->heldHighestOnly)
            give = slot < trainer->partySize && IsHighestLevelSlot(trainer, slot);   // one Pokemon only
        if (give)
        {
            u32 h = RH_Hash(SALT_HELD_ITEM, trainerId, slot);
            mon->heldItem = S->heldSensible ? SensibleItem(mon, h) : RH_RandomHeldItem(h, !S->heldConsumableOnly, S->heldConsumableOnly);
        }
    }
    if (S->trainerRandomShiny && (RH_Hash(SALT_TRAINER_SHINY, trainerId, slot) & 0xFF) == 0)
        mon->isShiny = TRUE;                         // FVX: 1/256
    if (sTeamCount < PARTY_SIZE)
        sTeam[sTeamCount++] = mon->species;
    sLeagueMember = 0;
}

// Adds "Additional Pokemon" (and a second Pokemon when a single-Pokemon trainer must fight a double battle).
// FVX: they are chosen like the rest of the team, with levels between the team's lowest and highest - 1, and the
// team's last Pokemon (its ace) stays last.
void RH_FinishTrainerParty(struct Pokemon *party, const struct Trainer *trainer, u32 *count, struct TrainerGenerator *gen)
{
    s32 trainerId;
    u32 tier, want, i, lo = MAX_LEVEL, hi = 1, first;
    const struct Trainer *keep;
    if (!S->enabled)
        return;
    trainerId = TrainerIdOf(trainer);
    if (trainerId < 0 || *count == 0 || IsFirstRivalBattle(trainerId))
        return;
    tier = TierOf(trainer->trainerClass);
    want = *count + S->additionalMons[tier];
    if (RH_TrainerBattleType(trainerId, trainer->battleType) == TRAINER_BATTLE_TYPE_DOUBLES && want < 2)
        want = 2;
    if (want > PARTY_SIZE)
        want = PARTY_SIZE;
    if (want <= *count)
        return;
    for (i = 0; i < trainer->partySize; i++)
    {
        lo = min(lo, trainer->party[i].lvl);
        hi = max(hi, trainer->party[i].lvl);
    }
    {
        // FVX: extras stay below the ace's level, unless two or more share the highest level
        u32 atHi = 0;
        for (i = 0; i < trainer->partySize; i++)
            atHi += (trainer->party[i].lvl == hi);
        if (atHi >= 2)
            hi++;
    }
    first = *count;
    keep = gen->trainer;
    for (i = first; i < want; i++)
    {
        struct TrainerMon extra = trainer->party[(i - 1) % trainer->partySize];
        struct RhFilter f = {0};
        u32 j, span = (hi > lo + 1) ? hi - 1 - lo + 1 : 1;
        u8 theme;
        extra.lvl = (hi > lo) ? lo + RH_Hash(SALT_EXTRA_MON, trainerId, i + 16) % span : lo;
        extra.nickname = NULL;
        extra.heldItem = ITEM_NONE;
        extra.ability = ABILITY_NONE;
        extra.isShiny = FALSE;
        for (j = 0; j < MAX_MON_MOVES; j++)
            extra.moves[j] = MOVE_NONE;
        if (S->trainers == 0)
        {
            // Trainers unchanged: pick by the "Keep Themed" rules (FVX), avoiding the team's species
            theme = GymTheme(trainerId) != TYPE_NONE ? GymTheme(trainerId) : TeamTheme(trainer);
            sTeamTrainer = trainer;
            SetupTeamFilter(&f, &extra, tier, theme);
            sAvoidDupes = TRUE;
            extra.species = RH_PickSpecies(&f, RH_Hash(SALT_EXTRA_MON, trainerId, i), S->trainerSimilarStrength ? extra.species : SPECIES_NONE);
            if (S->trainersEvolveOn)                         // at the level it will really have (after the modifier)
                extra.species = EvolveForLevel(extra.species, S->trainerLevelModOn ? RH_ApplyPercent(extra.lvl, S->trainerLevelMod) : extra.lvl, i);
        }
        RH_ModifyTrainerMon(&extra, trainer, i);    // the real slot, so every added Pokemon is its own pick
        gen->trainer = NULL;                         // already modified
        GenerateMonFromTrainerMon(&party[i], &extra, gen);
        gen->trainer = keep;
    }
    // keep the ace last
    if (first >= 1)
    {
        struct Pokemon ace = party[first - 1];
        for (i = first - 1; i + 1 < want; i++)
            party[i] = party[i + 1];
        party[want - 1] = ace;
    }
    *count = want;
}

// ---------------------------------------------------------------------------
// Battle style / names
// ---------------------------------------------------------------------------
enum TrainerBattleType RH_TrainerBattleType(u16 trainerId, enum TrainerBattleType vanilla)
{
    enum TrainerBattleType type;
    if (!S->enabled || S->battleStyle == 0 || trainerId >= TRAINERS_COUNT || IsFirstRivalBattle(trainerId))
        return vanilla;
    if (S->battleStyle == 2)
        type = S->battleStyleDoubles ? TRAINER_BATTLE_TYPE_DOUBLES : TRAINER_BATTLE_TYPE_SINGLES;
    else
        type = (RH_Hash(SALT_BATTLE_STYLE, trainerId, 0) & 1) ? TRAINER_BATTLE_TYPE_DOUBLES : TRAINER_BATTLE_TYPE_SINGLES;
    // a battle that became a double battle needs two usable Pokemon on the player's side (vanilla doubles ask for it)
    if (type == TRAINER_BATTLE_TYPE_DOUBLES && vanilla != TRAINER_BATTLE_TYPE_DOUBLES
     && GetMonsStateToDoubles() != PLAYER_HAS_TWO_USABLE_MONS)
        type = TRAINER_BATTLE_TYPE_SINGLES;
    return type;
}

static u32 StringHash(const u8 *s)
{
    u32 h = 0;
    for (; *s != EOS; s++)
        h = h * 31 + *s;
    return h;
}

// FVX: the same original name always becomes the same new name (so rematches keep theirs); every Rocket
// Grunt gets his own. The rival keeps the name the player gave him.
const u8 *RH_TrainerName(u16 trainerId, const u8 *vanilla)
{
    const struct Trainer *t;
    u32 key;
    if (!S->enabled || !S->randomTrainerNames || trainerId >= TRAINERS_COUNT)
        return vanilla;
    t = GetTrainerStructFromId(trainerId);
    if (IsRivalClass(t->trainerClass) || vanilla[0] == EOS)
        return vanilla;
    key = (t->trainerClass == TRAINER_CLASS_TEAM_ROCKET_FRLG) ? trainerId + 0x10000 : StringHash(vanilla);
    return sRhTrainerNames[RH_Hash(SALT_TRAINER_NAME, key, 0) % ARRAY_COUNT(sRhTrainerNames)];
}

const u8 *RH_TrainerClassName(u32 trainerClass, const u8 *vanilla)
{
    if (!S->enabled || !S->randomTrainerClassNames || IsRivalClass(trainerClass))
        return vanilla;
    return sRhTrainerClassNames[RH_Hash(SALT_TRAINER_CLASS, StringHash(vanilla), 0) % ARRAY_COUNT(sRhTrainerClassNames)];
}
