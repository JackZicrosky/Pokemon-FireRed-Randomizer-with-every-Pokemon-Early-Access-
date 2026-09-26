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
    return move != MOVE_NONE && move < MOVES_COUNT_GEN9 && move != MOVE_STRUGGLE;   // no Z / Max moves
}

static bool32 GenData(enum Move move, struct RhMoveData *d)
{
    u32 gen = RH_MovesGen();
    if (gen >= 9)
        return FALSE;
    return RH_GenMoveData(move, gen, d);
}

static u32 HitCount(enum Move move)
{
    if (gMovesInfo[move].multiHit)
        return 3;
    return max(1, gMovesInfo[move].strikeCount);
}

// FVX: 2/3 "regular" 50-100, 1/3 "extreme" 20-150, 1% (twice) +50; multi-hit moves divide by their hit count.
u32 RH_MovePower(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    u32 h, p, hits;
    if (GenData(move, &d))
        vanilla = d.power;
    if (!S->enabled || !S->movePower || vanilla < 10 || !IsRandomizableMove(move))
        return vanilla;
    h = RH_Hash(SALT_MOVE_POWER, move, 0);
    p = (h % 3 != 2) ? 50 + 5 * ((h >> 2) % 11) : 20 + 5 * ((h >> 2) % 27);
    if ((h >> 10) % 100 == 0)
        p += 50;
    if ((h >> 17) % 100 == 0)
        p += 50;
    hits = HitCount(move);
    if (hits > 1)
    {
        p = (p / hits + 2) / 5 * 5;
        if (p == 0)
            p = 5;
    }
    return p;
}

// FVX tiers: accuracy <= 50 -> 20-50 (10%: x1.5); < 90 -> count down from 100 with a 20% stop chance per 5%;
// >= 90 -> the same with a 40% stop chance. Moves that never miss are left alone.
u32 RH_MoveAccuracy(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    u32 h, acc, i;
    if (GenData(move, &d))
        vanilla = d.accuracy;
    if (!S->enabled || !S->moveAccuracy || vanilla < 5 || !IsRandomizableMove(move))
        return vanilla;
    h = RH_Hash(SALT_MOVE_ACC, move, 0);
    if (vanilla <= 50)
    {
        acc = 20 + 5 * (h % 7);
        if ((h >> 4) % 10 == 0)
            acc = acc * 3 / 2 / 5 * 5;
        return acc;
    }
    acc = 100;
    for (i = 1; acc > 20; i++)
    {
        if (RH_Hash(SALT_MOVE_ACC, move, i) % 10 < (vanilla < 90 ? 2u : 4u))
            break;
        acc -= 5;
    }
    return acc;
}

u32 RH_MovePP(enum Move move, u32 vanilla)
{
    struct RhMoveData d;
    u32 h;
    if (GenData(move, &d))
        vanilla = d.pp;
    if (!S->enabled || vanilla == 0 || !IsRandomizableMove(move))
        return vanilla;
    if (S->movesets == 3 && move == MOVE_METRONOME)
        return 40;                                           // FVX "Metronome Only"
    if (!S->movePP)
        return vanilla;
    h = RH_Hash(SALT_MOVE_PP, move, 0);
    return (h % 3 != 2) ? 15 + 5 * ((h >> 2) % 3) : 5 + 5 * ((h >> 2) % 8);
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
        vanilla = d.category;
    if (!S->enabled || !S->moveCategory || vanilla == DAMAGE_CATEGORY_STATUS || !IsRandomizableMove(move))
        return vanilla;
    if (RH_Hash(SALT_MOVE_CAT2, move, 0) & 1)
        return vanilla == DAMAGE_CATEGORY_PHYSICAL ? DAMAGE_CATEGORY_SPECIAL : DAMAGE_CATEGORY_PHYSICAL;
    return vanilla;
}

// Random move names ("Randomize Move Names"), built from FVX's word lists: "<type word> <category word>", following
// the move's (possibly randomized) type and category.
#define MOVE_NAME_BUFFERS 16                         // names stay valid for 16 more calls (menus keep a few pointers)
static EWRAM_DATA u8 sMoveNameBuf[MOVE_NAME_BUFFERS][MOVE_NAME_LENGTH + 1] = {0};
static EWRAM_DATA u8 sMoveNameNext = 0;

const u8 *RH_MoveName(enum Move move, const u8 *vanilla)
{
    u8 *buf, *end;
    const u8 *w1, *w2;
    u32 typeIdx, h, cat, l1, l2;
    if (!S->enabled || !S->moveNames || !IsRandomizableMove(move))
        return vanilla;
    typeIdx = RH_TypeIndexOf(GetMoveType(move));
    h = RH_Hash(SALT_MOVE_NAME, move, 0);
    w1 = sRhMoveTypeWords[typeIdx][h % sRhMoveTypeWordCounts[typeIdx]];
    cat = GetMoveCategory(move);
    if (cat == DAMAGE_CATEGORY_PHYSICAL)
        w2 = sRhMoveWords_PHYSICAL[(h >> 12) % ARRAY_COUNT(sRhMoveWords_PHYSICAL)];
    else if (cat == DAMAGE_CATEGORY_SPECIAL)
        w2 = sRhMoveWords_SPECIAL[(h >> 12) % ARRAY_COUNT(sRhMoveWords_SPECIAL)];
    else
        w2 = sRhMoveWords_STATUS[(h >> 12) % ARRAY_COUNT(sRhMoveWords_STATUS)];
    buf = sMoveNameBuf[sMoveNameNext];
    sMoveNameNext = (sMoveNameNext + 1) % MOVE_NAME_BUFFERS;
    end = StringCopy(buf, w1);
    l1 = StringLength(w1);
    l2 = StringLength(w2);
    if (l1 + 1 + l2 <= MOVE_NAME_LENGTH)
    {
        *end++ = CHAR_SPACE;
        StringCopy(end, w2);
    }
    else if (l1 + l2 <= MOVE_NAME_LENGTH)
    {
        StringCopy(end, w2);                                 // FVX: try the two words without a space
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Move picking. Picks look at the moves' current (possibly randomized) type / power / accuracy, which are cached
// once per settings in EWRAM (recomputing them for every pick would be far too slow on the GBA).
// ---------------------------------------------------------------------------
enum { PICK_ANY, PICK_DAMAGING, PICK_GOOD };
#define MF_DAMAGING (1 << 0)
#define MF_GOOD     (1 << 1)
#define MF_HM       (1 << 2)

struct MoveCache
{
    u32 key;
    u8 type[RH_MOVE_COUNT];
    u8 flags[RH_MOVE_COUNT];
};
static EWRAM_DATA struct MoveCache sMoveCache = {0};

static u32 MoveDataKey(void)
{
    return (RH_SettingsHash() + 1) | 1;   // every setting (0 = never valid)
}

static bool32 IsHMMove(u16 move)
{
    u32 i;
    for (i = NUM_TECHNICAL_MACHINES + 1; i <= NUM_ALL_MACHINES; i++)
        if (gTMHMItemMoveIds[i].moveId == move)
            return TRUE;
    return FALSE;
}

static void EnsureMoveCache(void)
{
    u32 i, key = MoveDataKey();
    if (sMoveCache.key == key)
        return;
    for (i = 0; i < RH_MOVE_COUNT; i++)
    {
        u16 move = sRhMoves[i].move;
        u32 power = GetMovePower(move) * sRhMoves[i].hits;
        u32 acc = GetMoveAccuracy(move);
        u8 f = 0;
        sMoveCache.type[i] = GetMoveType(move);
        if (GetMoveCategory(move) != DAMAGE_CATEGORY_STATUS && GetMovePower(move) > 1 && !sRhMoves[i].noDamaging)
        {
            f |= MF_DAMAGING;
            // FVX Move.isGoodDamaging: power >= 100, or >= 50 with 90%+ (or perfect) accuracy
            if (power >= 100 || (power >= 50 && (acc >= 90 || acc == 0)))
                f |= MF_GOOD;
        }
        if (IsHMMove(move))
            f |= MF_HM;
        sMoveCache.flags[i] = f;
    }
    sMoveCache.key = key;
}

static bool32 IsBroken(u32 i)
{
    return sRhMoves[i].broken || (sRhMoves[i].gen1Broken && S->mechanicsGen == 1);
}

// type: TYPE_NONE = any type. noHM: level-up movesets never get HM moves (FVX).
static bool32 MoveOk(u32 i, u8 type, u32 need, bool32 noBreaking, bool32 noHM)
{
    u8 f = sMoveCache.flags[i];
    if (noBreaking && IsBroken(i))
        return FALSE;
    if (noHM && (f & MF_HM))
        return FALSE;
    if (need == PICK_DAMAGING && !(f & MF_DAMAGING))
        return FALSE;
    if (need == PICK_GOOD && !(f & MF_GOOD))
        return FALSE;
    if (type != TYPE_NONE && sMoveCache.type[i] != type)
        return FALSE;
    return TRUE;
}

static u16 PickMove(u32 hash, u8 type, u32 need, bool32 noBreaking, bool32 noHM)
{
    u32 i, count = 0, target;
    EnsureMoveCache();
    for (i = 0; i < RH_MOVE_COUNT; i++)
        if (MoveOk(i, type, need, noBreaking, noHM))
            count++;
    if (count == 0)
    {
        if (type != TYPE_NONE)
            return PickMove(hash, TYPE_NONE, need, noBreaking, noHM);
        if (need != PICK_ANY)
            return PickMove(hash, TYPE_NONE, need == PICK_GOOD ? PICK_DAMAGING : PICK_ANY, noBreaking, noHM);
        return MOVE_TACKLE;
    }
    target = hash % count;
    for (i = 0; i < RH_MOVE_COUNT; i++)
    {
        if (MoveOk(i, type, need, noBreaking, noHM))
        {
            if (target == 0)
                return sRhMoves[i].move;
            target--;
        }
    }
    return MOVE_TACKLE;
}

static s32 PoolIndexOfMove(u16 move)
{
    s32 lo = 0, hi = RH_MOVE_COUNT - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sRhMoves[mid].move == move)
            return mid;
        if (sRhMoves[mid].move < move)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

bool32 RH_IsGoodDamagingMove(u16 move)
{
    s32 i = PoolIndexOfMove(move);
    EnsureMoveCache();
    return i >= 0 && (sMoveCache.flags[i] & MF_GOOD);
}

bool32 RH_IsDamagingMove(u16 move)
{
    return move != MOVE_NONE && GetMoveCategory(move) != DAMAGE_CATEGORY_STATUS && GetMovePower(move) > 1;
}

// ---------------------------------------------------------------------------
// Level-up movesets
// ---------------------------------------------------------------------------
#define LEARNSET_MAX 40
struct LearnsetCache { u16 species; u32 key; struct LevelUpMove moves[LEARNSET_MAX + 2]; };   // +1 added level-1 slot, +1 end
static EWRAM_DATA struct LearnsetCache sLearnsetCache[8] = {0};
static EWRAM_DATA u8 sLearnsetCacheNext = 0;

static u32 LearnsetKey(void)
{
    return (RH_SettingsHash() + 2) | 1;   // every setting (0 = never valid)
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
    return GetMovePower(move) * HitCount(move);
}

const struct LevelUpMove *RH_LevelUpLearnset(enum Species species, const struct LevelUpMove *vanilla)
{
    u32 i, j, n, total, key, goodPct, lv1Count, lastLv1;
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

    // Level slots: the vanilla ones (evolution moves at level 0 first), plus extra level-1 slots for
    // "Guaranteed Level 1 Moves" (FVX only adds, never removes).
    for (total = 0; total < LEARNSET_MAX && vanilla[total].move != LEVEL_UP_MOVE_END; total++)
        ;
    lv1Count = 0;
    for (i = 0; i < total; i++)
        lv1Count += (vanilla[i].level == 1);
    n = 0;
    for (i = 0; i < total && vanilla[i].level == 0; i++)
        c->moves[n++].level = 0;
    if (S->evolutionMovesForAll && n == 0 && RH_PreEvo(species) != SPECIES_NONE)
        c->moves[n++].level = 0;                             // "Evolution Moves for All Pokemon"
    if (S->guaranteedLevel1On)
    {
        u32 want = min(max(S->guaranteedLevel1Moves, 2), 4);
        for (j = lv1Count; j < want && n < LEARNSET_MAX; j++)
            c->moves[n++].level = 1;
    }
    for (; i < total && n < LEARNSET_MAX; i++)
        c->moves[n++].level = vanilla[i].level;
    lastLv1 = n;
    for (i = 0; i < n; i++)
        if (c->moves[i].level == 1)
            lastLv1 = i;
    if (lastLv1 == n)
    {
        // no level-1 move at all: add one so the Pokemon can always attack
        for (i = n; i > 0 && c->moves[i - 1].level > 1; i--)
            c->moves[i] = c->moves[i - 1];
        c->moves[i].level = 1;
        lastLv1 = i;
        n++;
    }

    goodPct = S->movesetGoodDamagingOn ? S->movesetGoodDamaging : 0;
    for (i = 0; i < n; i++)
    {
        u16 move = MOVE_NONE;
        u32 tries, need = PICK_ANY;
        for (tries = 0; tries < 12; tries++)
        {
            u32 h = RH_Hash(SALT_LEARNSET, species, i * 16 + tries);
            u8 type = TYPE_NONE;
            need = PICK_ANY;
            if (i == lastLv1)
                need = PICK_GOOD;                            // FVX: always start with a good attack
            else if ((h % 100) < goodPct)
                need = PICK_GOOD;
            if (S->movesets == 1 && ((h >> 8) % 5) < 2)
                type = GetSpeciesType(species, (h >> 11) & 1);   // "Prefer Same Type": 40% one of its types
            move = PickMove(h >> 12, type, need, S->movesetNoGameBreaking, TRUE);
            if (!InLearnset(c->moves, i, move))
                break;
            move = MOVE_NONE;
        }
        if (move == MOVE_NONE)
        {
            // no duplicates: take the first unused move that fits
            for (j = 0; j < RH_MOVE_COUNT && move == MOVE_NONE; j++)
                if (MoveOk(j, TYPE_NONE, need, S->movesetNoGameBreaking, TRUE) && !InLearnset(c->moves, i, sRhMoves[j].move))
                    move = sRhMoves[j].move;
            if (move == MOVE_NONE)
                move = MOVE_TACKLE;
        }
        c->moves[i].move = move;
    }

    if (S->reorderDamagingMoves)
    {
        // Stronger attacks are learned later: sort the damaging moves by power, keeping the level slots
        // (evolution moves at level 0 and the guaranteed level-1 attack stay where they are).
        for (i = 0; i < n; i++)
        {
            if (c->moves[i].level == 0 || i == lastLv1 || !RH_IsDamagingMove(c->moves[i].move))
                continue;
            for (j = i + 1; j < n; j++)
            {
                if (j != lastLv1 && RH_IsDamagingMove(c->moves[j].move) && MovePowerForSort(c->moves[j].move) < MovePowerForSort(c->moves[i].move))
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

// Egg moves: with random movesets, every Pokemon keeps its number of egg moves but they are random (FVX).
#define EGG_MOVES_MAX 24
static EWRAM_DATA u16 sEggMoves[EGG_MOVES_MAX + 1] = {0};
static EWRAM_DATA u16 sEggMovesSpecies = 0;
static EWRAM_DATA u32 sEggMovesKey = 0;

const u16 *RH_EggMoves(enum Species species, const u16 *vanilla)
{
    u32 i, j, n;
    if (!S->enabled || S->movesets == 0 || vanilla == NULL)
        return vanilla;
    if (sEggMovesSpecies == species && sEggMovesKey == LearnsetKey())
        return sEggMoves;
    for (n = 0; n < EGG_MOVES_MAX && vanilla[n] != MOVE_UNAVAILABLE; n++)
        ;
    for (i = 0; i < n; i++)
    {
        u16 move = MOVE_METRONOME;
        if (S->movesets != 3)
        {
            for (j = 0; j < 12; j++)
            {
                u32 h = RH_Hash(SALT_EGG_MOVES, species, i * 16 + j);
                u8 type = (S->movesets == 1 && ((h >> 8) % 5) < 2) ? GetSpeciesType(species, (h >> 11) & 1) : TYPE_NONE;
                u32 k;
                move = PickMove(h >> 12, type, PICK_ANY, S->movesetNoGameBreaking, TRUE);
                for (k = 0; k < i && sEggMoves[k] != move; k++)
                    ;
                if (k == i)
                    break;
            }
        }
        sEggMoves[i] = move;
        if (S->movesets == 3)
        {
            n = 1;
            break;
        }
    }
    sEggMoves[n] = MOVE_UNAVAILABLE;
    sEggMovesSpecies = species;
    sEggMovesKey = LearnsetKey();
    return sEggMoves;
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
// FVX field moves (healing moves such as Soft-Boiled are not field moves there).
static const u16 sFieldMoves[] = {
    MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH, MOVE_ROCK_SMASH, MOVE_WATERFALL, MOVE_DIVE,
    MOVE_DIG, MOVE_TELEPORT, MOVE_SWEET_SCENT, MOVE_SECRET_POWER, MOVE_ROCK_CLIMB,
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
    return (RH_SettingsHash() + 3) | 1;   // every setting (0 = never valid)
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
    if (!S->tutorMoves)
        for (i = 0; i < NUM_RH_TUTORS; i++)
            if (sTutorMoves[i] == move)
                return TRUE;                                 // unchanged tutors keep their moves to themselves
    if (!S->tmMoves)
        for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
            if (gTMHMItemMoveIds[i].moveId == move)
                return TRUE;
    return IsHMMove(move);                                   // never duplicate an HM
}

static u16 RandomMachineMove(u32 salt, u32 index, u32 goodPct, bool32 noBreaking, bool32 keepField, u32 tmCount, u32 tutorCount)
{
    u32 tries;
    for (tries = 0; tries < 64; tries++)
    {
        u32 h = RH_Hash(salt, index, tries);
        u16 move = PickMove(h >> 8, TYPE_NONE, (h % 100) < goodPct ? PICK_GOOD : PICK_ANY, noBreaking, FALSE);
        if ((keepField && IsFieldMove(move)) || UsedMachineMove(move, tmCount, tutorCount))
            continue;
        return move;
    }
    // unlucky: take the next unused move of the pool, so no TM / tutor ever repeats one
    for (tries = 0; tries < RH_MOVE_COUNT; tries++)
    {
        const struct RhMove *m = &sRhMoves[(RH_Hash(salt, index, 999) + tries) % RH_MOVE_COUNT];
        if (noBreaking && m->broken)
            continue;
        if (IsHMMove(m->move) || (keepField && IsFieldMove(m->move)) || UsedMachineMove(m->move, tmCount, tutorCount))
            continue;
        return m->move;
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
            u16 m = RandomMachineMove(SALT_TM, i, S->tmGoodDamagingOn ? S->tmGoodDamaging : 0, S->tmNoGameBreaking,
                                      S->tmKeepFieldMoves, i - 1, 0);
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
                                      S->tutorKeepFieldMoves, NUM_TECHNICAL_MACHINES, i);
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
    if (!S->enabled || index == 0 || index > NUM_TECHNICAL_MACHINES)
        return vanilla;                                      // HMs never change
    if (S->movesets == 3)
        return MOVE_METRONOME;                               // FVX "Metronome Only": every TM is Metronome
    if (!S->tmMoves)
        return vanilla;
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
    if (!S->enabled || slot < 0)
        return vanilla;
    if (S->movesets == 3)
        return MOVE_METRONOME;
    if (!S->tutorMoves)
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

// Script special: STR_VAR_2 = the move taught by the TM item in VAR_TEMP_1 (Game Corner prize list).
void RH_BufferTMMoveFromItem(void)
{
    StringCopy(gStringVar2, GetMoveName(GetItemTMHMMoveId(VarGet(VAR_TEMP_1))));
}

// ---------------------------------------------------------------------------
// Compatibility
// ---------------------------------------------------------------------------
static s32 TMIndexOfMove(enum Move move)
{
    u32 i;
    if (S->movesets == 3 && move == MOVE_METRONOME)
        return 1;
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

// FVX odds: prefer type 90% (own type) / 50% (Normal) / 25% (other), completely random 50%. Cut gets x1.8 in
// prefer-type mode (it is needed early in FRLG).
static bool32 RollCompat(u32 salt, u32 mode, u16 species, u16 move, u32 slot, u32 odds)
{
    return RH_Hash(salt, species, slot) % 100 < min(odds, 100u);
}

static u32 BaseOdds(u32 mode, u16 species, u16 move)
{
    u8 type = GetMoveType(move);
    u32 odds;
    if (mode == 2)
        return 50;
    if (RH_SpeciesHasType(species, type))
        odds = 90;
    else if (type == TYPE_NORMAL)
        odds = 50;
    else
        odds = 25;
    if (move == MOVE_CUT)
        odds = odds * 18 / 10;
    return odds;
}

static bool32 RandomCompat(u32 salt, u32 mode, bool32 followEvos, u16 species, u16 move, u32 slot, u32 depth)
{
    u16 pre;
    if (mode == 3)
        return TRUE;
    pre = (followEvos && depth < 3) ? RH_PreEvo(species) : SPECIES_NONE;
    if (pre == SPECIES_NONE)
        return RollCompat(salt, mode, species, move, slot, BaseOdds(mode, species, move));
    // Follow Evolutions (FVX): an evolution learns everything its pre-evolution learns, plus a few more
    // (25% completely random, 10% prefer type, 90% for a type it gained).
    if (RandomCompat(salt, mode, TRUE, pre, move, slot, depth + 1))
        return TRUE;
    if (mode == 2)
        return RollCompat(salt, mode, species, move, slot, 25);
    {
        u8 type = GetMoveType(move);
        bool32 gained = RH_SpeciesHasType(species, type) && !RH_SpeciesHasType(pre, type);
        return RollCompat(salt, mode, species, move, slot, gained ? 90 : 10);
    }
}

bool32 RH_CanLearnTeachable(enum Species species, enum Move move, bool32 (*vanillaCheck)(enum Species, enum Move))
{
    s32 tm, tutor;
    if (!S->enabled || species == SPECIES_EGG || species == SPECIES_NONE)
        return vanillaCheck(species, move);
    if (S->movesets == 3 && move == MOVE_METRONOME)
        return TRUE;
    tm = TMIndexOfMove(move);
    if (tm > 0)
    {
        if (tm > NUM_TECHNICAL_MACHINES)                     // HM
        {
            if (S->fullHMCompat || S->tmCompat == 3)
                return TRUE;
            if (S->tmCompat == 0)
                return vanillaCheck(species, move);
            return RandomCompat(SALT_TM_COMPAT, S->tmCompat, S->tmCompatFollowEvos, species, move, tm, 0);
        }
        if (S->tmLevelupSanity && LearnsByLevelUp(species, move))
            return TRUE;
        if (S->tmCompat == 0)
            return vanillaCheck(species, gTMHMItemMoveIds[tm].moveId);   // the slot keeps its original compatibility
        return RandomCompat(SALT_TM_COMPAT, S->tmCompat, S->tmCompatFollowEvos, species, move, tm, 0);
    }
    tutor = TutorSlotOfMove(move);
    if (tutor >= 0)
    {
        if (S->tutorLevelupSanity && LearnsByLevelUp(species, move))
            return TRUE;
        if (S->tutorCompat == 0)
            return vanillaCheck(species, sTutorMoves[tutor]);
        return RandomCompat(SALT_TUTOR_COMPAT, S->tutorCompat, S->tutorCompatFollowEvos, species, move, tutor, 0);
    }
    return vanillaCheck(species, move);
}
