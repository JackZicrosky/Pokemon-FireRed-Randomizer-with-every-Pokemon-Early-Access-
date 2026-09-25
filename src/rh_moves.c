// Moves & Movesets, TMs & HMs, Move Tutors.
#include "global.h"
#include "constants/characters.h"
#include "event_data.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "string_util.h"
#include "rh_internal.h"
#include "constants/moves.h"
#include "data/rh_randomizer_tables.h"
#include "data/rh_names.h"

// ---------------------------------------------------------------------------
// Move data: generation data first, then randomization.
// ---------------------------------------------------------------------------
static bool32 IsRandomizableMove(u32 move)
{
    return move != MOVE_NONE && move < MOVES_COUNT && move != MOVE_STRUGGLE;
}

static bool32 GenData(enum Move move, struct RhMoveData *d)
{
    u32 gen = RH_MovesGen();
    if (gen >= 9)
        return FALSE;
    return RH_GenMoveData(move, gen, d);
}

u32 RH_MovePower(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    if (GenData(move, &d))
        vanilla = d.power;
    if (!S->enabled || !S->movePower || vanilla <= 1 || !IsRandomizableMove(move))
        return vanilla;
    // FVX: 20-150 in steps of 5, weighted towards the middle
    return 20 + 5 * ((RH_Hash(SALT_MOVE_POWER, move, 0) % 14) + (RH_Hash(SALT_MOVE_POWER, move, 1) % 14));
}

u32 RH_MoveAccuracy(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    u32 h;
    if (GenData(move, &d))
        vanilla = d.accuracy;
    if (!S->enabled || !S->moveAccuracy || vanilla == 0 || !IsRandomizableMove(move))
        return vanilla;
    h = RH_Hash(SALT_MOVE_ACC, move, 0);
    if (vanilla == 100 && h % 4)
        return 100;                                          // mostly keep perfect-accuracy moves accurate
    if (h % 3 == 0)
        return 100;
    return 60 + 5 * ((h >> 2) % 8);                          // 60..95
}

u32 RH_MovePP(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    if (GenData(move, &d))
        vanilla = d.pp;
    if (!S->enabled || !S->movePP || !IsRandomizableMove(move) || vanilla == 0)
        return vanilla;
    return 5 * (1 + RH_Hash(SALT_MOVE_PP, move, 0) % 8);     // 5..40
}

enum Type RH_MoveType(enum Move move, enum Type vanilla)
{
    struct RhMoveData d;
    if (GenData(move, &d))
        vanilla = d.type;
    if (!S->enabled || !S->moveType || !IsRandomizableMove(move) || vanilla == TYPE_MYSTERY || vanilla == TYPE_NONE)
        return vanilla;
    return RH_RandomMonType(RH_Hash(SALT_MOVE_TYPE, move, 0));
}

u32 RH_MoveCategory(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    if (GenData(move, &d))
        return d.category;
    return vanilla;
}

// Random move names ("Randomize Move Names"), built from FVX's word lists: "<type word> <category word>".
static EWRAM_DATA u8 sMoveNameBuf[4][MOVE_NAME_LENGTH + 1] = {0};
static EWRAM_DATA u8 sMoveNameNext = 0;

const u8 *RH_MoveName(enum Move move, const u8 *vanilla)
{
    u8 *buf, *end;
    const u8 *w1, *w2;
    u32 typeIdx, h, cat;
    if (!S->enabled || !S->moveNames || !IsRandomizableMove(move))
        return vanilla;
    typeIdx = RH_TypeIndexOf(gMovesInfo[move].type);
    h = RH_Hash(SALT_MOVE_NAME, move, 0);
    w1 = sRhMoveTypeWords[typeIdx][h % sRhMoveTypeWordCounts[typeIdx]];
    cat = gMovesInfo[move].category;
    if (cat == DAMAGE_CATEGORY_PHYSICAL)
        w2 = sRhMoveWords_PHYSICAL[(h >> 12) % ARRAY_COUNT(sRhMoveWords_PHYSICAL)];
    else if (cat == DAMAGE_CATEGORY_SPECIAL)
        w2 = sRhMoveWords_SPECIAL[(h >> 12) % ARRAY_COUNT(sRhMoveWords_SPECIAL)];
    else
        w2 = sRhMoveWords_STATUS[(h >> 12) % ARRAY_COUNT(sRhMoveWords_STATUS)];
    buf = sMoveNameBuf[sMoveNameNext];
    sMoveNameNext = (sMoveNameNext + 1) % ARRAY_COUNT(sMoveNameBuf);
    end = StringCopy(buf, w1);
    if (StringLength(w1) + 1 + StringLength(w2) <= MOVE_NAME_LENGTH)
    {
        *end++ = CHAR_SPACE;
        StringCopy(end, w2);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Move picking
// ---------------------------------------------------------------------------
enum { PICK_ANY, PICK_DAMAGING, PICK_GOOD };

static bool32 MoveOk(const struct RhMove *m, u32 need, bool32 noBreaking)
{
    if (noBreaking && m->broken)
        return FALSE;
    if (need == PICK_DAMAGING && !m->damaging)
        return FALSE;
    if (need == PICK_GOOD && !m->good)
        return FALSE;
    return TRUE;
}

static s32 TypeRangeIndex(u8 type)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhTypes); i++)
        if (sRhTypes[i] == type)
            return i;
    return -1;
}

static u16 PickMove(u32 hash, s32 typeIdx, u32 need, bool32 noBreaking)
{
    u32 start = 0, end = RH_MOVE_COUNT, i, count = 0, target;
    if (typeIdx >= 0)
    {
        start = sRhMoveTypeStart[typeIdx];
        end = sRhMoveTypeStart[typeIdx + 1];
    }
    for (i = start; i < end; i++)
        if (MoveOk(&sRhMoves[i], need, noBreaking))
            count++;
    if (count == 0)
    {
        if (typeIdx >= 0)
            return PickMove(hash, -1, need, noBreaking);
        if (need != PICK_ANY)
            return PickMove(hash, -1, PICK_ANY, noBreaking);
        return MOVE_TACKLE;
    }
    target = hash % count;
    for (i = start; i < end; i++)
    {
        if (MoveOk(&sRhMoves[i], need, noBreaking))
        {
            if (target == 0)
                return sRhMoves[i].move;
            target--;
        }
    }
    return MOVE_TACKLE;
}

static const struct RhMove *FindMove(u16 move)
{
    u32 i;
    for (i = 0; i < RH_MOVE_COUNT; i++)
        if (sRhMoves[i].move == move)
            return &sRhMoves[i];
    return NULL;
}

bool32 RH_IsGoodDamagingMove(u16 move)
{
    const struct RhMove *m = FindMove(move);
    return m != NULL && m->good;
}

bool32 RH_IsDamagingMove(u16 move)
{
    return move != MOVE_NONE && GetMoveCategory(move) != DAMAGE_CATEGORY_STATUS && GetMovePower(move) > 1;
}

// ---------------------------------------------------------------------------
// Level-up movesets
// ---------------------------------------------------------------------------
#define LEARNSET_MAX 40
struct LearnsetCache { u16 species; u32 key; struct LevelUpMove moves[LEARNSET_MAX + 1]; };
static EWRAM_DATA struct LearnsetCache sLearnsetCache[8] = {0};
static EWRAM_DATA u8 sLearnsetCacheNext = 0;

static u32 LearnsetKey(void)
{
    return S->seed ^ (S->movesets << 1) ^ (S->guaranteedLevel1On << 3) ^ (S->guaranteedLevel1Moves << 4)
         ^ (S->reorderDamagingMoves << 7) ^ (S->movesetNoGameBreaking << 8) ^ (S->movesetGoodDamagingOn << 9)
         ^ (S->movesetGoodDamaging << 10) ^ (S->types << 18) ^ (S->enabled << 20);
}

static bool32 InLearnset(const struct LevelUpMove *moves, u32 n, u16 move)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (moves[i].move == move)
            return TRUE;
    return FALSE;
}

static u32 MovePowerForSort(u16 move)
{
    return GetMovePower(move) * max(1, gMovesInfo[move].strikeCount);
}

const struct LevelUpMove *RH_LevelUpLearnset(enum Species species, const struct LevelUpMove *vanilla)
{
    u32 i, j, n, total, level1, key, goodPct;
    struct LearnsetCache *c;
    if (!S->enabled || S->movesets == 0 || species == SPECIES_NONE)
        return vanilla;
    key = LearnsetKey();
    for (i = 0; i < ARRAY_COUNT(sLearnsetCache); i++)
        if (sLearnsetCache[i].species == species && sLearnsetCache[i].key == key)
            return sLearnsetCache[i].moves;
    c = &sLearnsetCache[sLearnsetCacheNext];
    sLearnsetCacheNext = (sLearnsetCacheNext + 1) % ARRAY_COUNT(sLearnsetCache);
    c->species = SPECIES_NONE;

    if (S->movesets == 3)
    {
        c->moves[0].move = MOVE_METRONOME;
        c->moves[0].level = 1;
        c->moves[1].move = LEVEL_UP_MOVE_END;
        c->moves[1].level = 0;
        c->species = species;
        c->key = key;
        return c->moves;
    }

    // Level slots: the vanilla ones, plus extra level-1 slots for "Guaranteed Level 1 Moves".
    for (total = 0; total < LEARNSET_MAX && vanilla[total].move != LEVEL_UP_MOVE_END; total++)
        ;
    level1 = 0;
    for (i = 0; i < total && vanilla[i].level <= 1; i++)
        level1++;
    n = 0;
    if (S->guaranteedLevel1On)
    {
        u32 want = min(max(S->guaranteedLevel1Moves, 2), 4);
        for (i = 0; i < want && n < LEARNSET_MAX; i++)
        {
            c->moves[n].level = 1;
            n++;
        }
        // keep the vanilla slots after the level-1 block
        for (i = level1; i < total && n < LEARNSET_MAX; i++)
            c->moves[n++].level = vanilla[i].level;
    }
    else
    {
        for (i = 0; i < total; i++)
            c->moves[n++].level = vanilla[i].level;
    }
    if (n == 0)
    {
        c->moves[0].level = 1;
        n = 1;
    }

    goodPct = S->movesetGoodDamagingOn ? S->movesetGoodDamaging : 0;
    for (i = 0; i < n; i++)
    {
        u16 move = MOVE_TACKLE;
        u32 tries;
        for (tries = 0; tries < 12; tries++)
        {
            u32 h = RH_Hash(SALT_LEARNSET, species, i * 16 + tries);
            u32 need = PICK_ANY;
            s32 typeIdx = -1;
            if ((h % 100) < goodPct)
                need = PICK_GOOD;
            else if (i == 0)
                need = PICK_DAMAGING;                        // always start with an attack
            if (S->movesets == 1 && ((h >> 8) % 5) < 2)
                typeIdx = TypeRangeIndex(GetSpeciesType(species, (h >> 11) & 1));   // 40%: one of its types
            move = PickMove(h >> 12, typeIdx, need, S->movesetNoGameBreaking);
            if (!InLearnset(c->moves, i, move))
                break;
        }
        c->moves[i].move = move;
    }

    if (S->reorderDamagingMoves)
    {
        // Stronger attacks are learned later: sort the damaging moves by power, keeping the level slots.
        for (i = 0; i < n; i++)
        {
            if (!RH_IsDamagingMove(c->moves[i].move))
                continue;
            for (j = i + 1; j < n; j++)
            {
                if (RH_IsDamagingMove(c->moves[j].move) && MovePowerForSort(c->moves[j].move) < MovePowerForSort(c->moves[i].move))
                {
                    u16 t = c->moves[i].move;
                    c->moves[i].move = c->moves[j].move;
                    c->moves[j].move = t;
                }
            }
        }
    }
    c->moves[n].move = LEVEL_UP_MOVE_END;
    c->moves[n].level = 0;
    c->species = species;
    c->key = key;
    return c->moves;
}

static bool32 LearnsByLevelUp(enum Species species, enum Move move)
{
    const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(species);
    u32 i;
    for (i = 0; l != NULL && l[i].move != LEVEL_UP_MOVE_END; i++)
        if (l[i].move == move)
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// TMs and move tutors: every machine / tutor teaches a unique random move.
// ---------------------------------------------------------------------------
static const u16 sFieldMoves[] = {
    MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH, MOVE_ROCK_SMASH, MOVE_WATERFALL, MOVE_DIVE,
    MOVE_DIG, MOVE_TELEPORT, MOVE_SOFT_BOILED, MOVE_MILK_DRINK, MOVE_SWEET_SCENT, MOVE_SECRET_POWER, MOVE_ROCK_CLIMB,
};

static bool32 IsFieldMove(u16 move)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sFieldMoves); i++)
        if (sFieldMoves[i] == move)
            return TRUE;
    return FALSE;
}

// FireRed's move tutors, in the order of data/scripts/move_tutors_frlg.inc.
static const u16 sTutorMoves[] = {
    MOVE_DOUBLE_EDGE, MOVE_THUNDER_WAVE, MOVE_ROCK_SLIDE, MOVE_EXPLOSION, MOVE_MEGA_PUNCH, MOVE_MEGA_KICK,
    MOVE_DREAM_EATER, MOVE_SOFT_BOILED, MOVE_SUBSTITUTE, MOVE_SWORDS_DANCE, MOVE_SEISMIC_TOSS, MOVE_COUNTER,
    MOVE_METRONOME, MOVE_MIMIC, MOVE_BODY_SLAM,
};
#define NUM_RH_TUTORS ARRAY_COUNT(sTutorMoves)

struct MachineCache
{
    u32 key;
    u16 tm[NUM_TECHNICAL_MACHINES + 1];          // 1-based like gTMHMItemMoveIds
    u16 tutor[NUM_RH_TUTORS];
};
static EWRAM_DATA struct MachineCache sMachines = {0};

static u32 MachineKey(void)
{
    return S->seed ^ (S->tmMoves << 1) ^ (S->tmNoGameBreaking << 2) ^ (S->tmKeepFieldMoves << 3) ^ (S->tmGoodDamagingOn << 4)
         ^ (S->tmGoodDamaging << 5) ^ (S->tutorMoves << 12) ^ (S->tutorNoGameBreaking << 13) ^ (S->tutorKeepFieldMoves << 14)
         ^ (S->tutorGoodDamagingOn << 15) ^ (S->tutorGoodDamaging << 16) ^ (S->enabled << 24) ^ 0x77;
}

static bool32 UsedMachineMove(u16 move, u32 tmCount, u32 tutorCount)
{
    u32 i;
    for (i = 1; i <= tmCount; i++)
        if (sMachines.tm[i] == move)
            return TRUE;
    for (i = 0; i < tutorCount; i++)
        if (sMachines.tutor[i] == move)
            return TRUE;
    for (i = NUM_TECHNICAL_MACHINES + 1; i <= NUM_ALL_MACHINES; i++)
        if (gTMHMItemMoveIds[i].moveId == move)
            return TRUE;                                     // never duplicate an HM
    return FALSE;
}

static u16 RandomMachineMove(u32 salt, u32 index, u32 goodPct, bool32 noBreaking, u32 tmCount, u32 tutorCount)
{
    u32 tries;
    for (tries = 0; tries < 64; tries++)
    {
        u32 h = RH_Hash(salt, index, tries);
        u16 move = PickMove(h >> 8, -1, (h % 100) < goodPct ? PICK_GOOD : PICK_ANY, noBreaking);
        if (IsFieldMove(move) || UsedMachineMove(move, tmCount, tutorCount))
            continue;
        return move;
    }
    return MOVE_NONE;
}

static void BuildMachines(void)
{
    u32 i;
    memset(&sMachines, 0, sizeof(sMachines));
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
    {
        u16 vanilla = gTMHMItemMoveIds[i].moveId;
        sMachines.tm[i] = vanilla;
        if (S->tmMoves && !(S->tmKeepFieldMoves && IsFieldMove(vanilla)))
        {
            u16 m = RandomMachineMove(SALT_TM, i, S->tmGoodDamagingOn ? S->tmGoodDamaging : 0, S->tmNoGameBreaking, i - 1, 0);
            if (m != MOVE_NONE)
                sMachines.tm[i] = m;
        }
    }
    for (i = 0; i < NUM_RH_TUTORS; i++)
    {
        sMachines.tutor[i] = sTutorMoves[i];
        if (S->tutorMoves && !(S->tutorKeepFieldMoves && IsFieldMove(sTutorMoves[i])))
        {
            u16 m = RandomMachineMove(SALT_TUTOR, i, S->tutorGoodDamagingOn ? S->tutorGoodDamaging : 0, S->tutorNoGameBreaking,
                                      NUM_TECHNICAL_MACHINES, i);
            if (m != MOVE_NONE)
                sMachines.tutor[i] = m;
        }
    }
    sMachines.key = MachineKey();
}

static void EnsureMachines(void)
{
    if (sMachines.key != MachineKey())
        BuildMachines();
}

enum Move RH_TMMove(u32 index, enum Move vanilla)
{
    if (!S->enabled || !S->tmMoves || index == 0 || index > NUM_TECHNICAL_MACHINES)
        return vanilla;                                      // HMs never change
    EnsureMachines();
    return sMachines.tm[index];
}

static s32 TutorSlotOfVanilla(u16 move)
{
    u32 i;
    for (i = 0; i < NUM_RH_TUTORS; i++)
        if (sTutorMoves[i] == move)
            return i;
    return -1;
}

enum Move RH_TutorMove(enum Move vanilla)
{
    s32 slot = TutorSlotOfVanilla(vanilla);
    if (!S->enabled || !S->tutorMoves || slot < 0)
        return vanilla;
    EnsureMachines();
    return sMachines.tutor[slot];
}

// Script special: VAR_0x8005 = a tutor's vanilla move -> its randomized move; the move name goes to STR_VAR_2.
void RH_RemapTutorMove(void)
{
    gSpecialVar_0x8005 = RH_TutorMove(gSpecialVar_0x8005);
    StringCopy(gStringVar2, GetMoveName(gSpecialVar_0x8005));
}

// ---------------------------------------------------------------------------
// Compatibility
// ---------------------------------------------------------------------------
static s32 TMIndexOfMove(enum Move move)
{
    u32 i;
    for (i = 1; i <= NUM_ALL_MACHINES; i++)
        if (GetTMHMMoveId(i) == move)
            return i;
    return -1;
}

static s32 TutorSlotOfMove(enum Move move)
{
    u32 i;
    if (!S->tutorMoves)
        return TutorSlotOfVanilla(move);
    EnsureMachines();
    for (i = 0; i < NUM_RH_TUTORS; i++)
        if (sMachines.tutor[i] == move)
            return i;
    return -1;
}

static bool32 RandomCompat(u32 salt, u32 mode, enum Species species, enum Move move, u32 slot)
{
    u32 key = S->tmCompatFollowEvos ? RH_FamilyRoot(species) : species;
    u32 h = RH_Hash(salt, key, slot) % 100;
    u8 type = GetMoveType(move);
    if (mode == 3)
        return TRUE;
    if (mode == 2)
        return h < 50;
    if (RH_SpeciesHasType(species, type))
        return h < 90;
    if (type == TYPE_NORMAL)
        return h < 50;
    return h < 25;
}

bool32 RH_CanLearnTeachable(enum Species species, enum Move move, bool32 (*vanillaCheck)(enum Species, enum Move))
{
    s32 tm, tutor;
    if (!S->enabled || species == SPECIES_EGG || species == SPECIES_NONE)
        return vanillaCheck(species, move);
    tm = TMIndexOfMove(move);
    if (tm > 0)
    {
        if (tm > NUM_TECHNICAL_MACHINES)                     // HM
        {
            if (S->fullHMCompat || S->tmCompat == 3)
                return TRUE;
            if (S->tmCompat == 0)
                return vanillaCheck(species, move);
            return RandomCompat(SALT_TM_COMPAT, S->tmCompat, species, move, tm);
        }
        if (S->tmCompat != 0 && S->tmLevelupSanity && LearnsByLevelUp(species, move))
            return TRUE;
        if (S->tmCompat == 0)
            return vanillaCheck(species, gTMHMItemMoveIds[tm].moveId);   // the slot keeps its original compatibility
        return RandomCompat(SALT_TM_COMPAT, S->tmCompat, species, move, tm);
    }
    tutor = TutorSlotOfMove(move);
    if (tutor >= 0)
    {
        if (S->tutorCompat == 0)
            return vanillaCheck(species, sTutorMoves[tutor]);
        if (S->tmLevelupSanity && LearnsByLevelUp(species, move))
            return TRUE;
        return RandomCompat(SALT_TUTOR_COMPAT, S->tutorCompat, species, move, tutor);
    }
    return vanillaCheck(species, move);
}
