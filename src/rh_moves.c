// Moves & Movesets, TMs & HMs, Move Tutors.
#include "global.h"
#include "constants/characters.h"
#include "event_data.h"
#include "item.h"
#include "malloc.h"
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

// Random move names ("Randomize Move Names"), built from FVX's word lists: "<type word> <action word>". The action
// word follows the move (FVX getActionWords): punch / sound / drain moves, healing, status infliction, trapping,
// stat buffs / debuffs, else Physical / Special / Status words. No two moves share a name (FVX usedMoveNames).
#define MOVE_NAME_BUFFERS 16                         // names stay valid for 16 more calls (menus keep a few pointers)
static EWRAM_DATA u8 sMoveNameBuf[MOVE_NAME_BUFFERS][MOVE_NAME_LENGTH + 1] = {0};
static EWRAM_DATA u8 sMoveNameNext = 0;
struct MoveNameCache { u32 key; u16 pick[MOVES_COUNT]; };   // pick: (type word << 8) | action word
static EWRAM_DATA struct MoveNameCache sMoveNames = {0};

enum { AW_PHYSICAL, AW_SPECIAL, AW_STATUS, AW_HEAL, AW_BUFF, AW_DEBUFF, AW_TRAP, AW_POISON, AW_BURN, AW_FREEZE,
       AW_PARALYZE, AW_SLEEP, AW_CONFUSION, AW_DRAIN, AW_PUNCH, AW_SOUND, AW_COUNT };
static const u8 *const *const sActionWords[AW_COUNT] = {
    sRhMoveWords_PHYSICAL, sRhMoveWords_SPECIAL, sRhMoveWords_STATUS, sRhMoveWords_STATUS_HEAL, sRhMoveWords_STATUS_BUFF,
    sRhMoveWords_STATUS_DEBUFF, sRhMoveWords_STATUS_TRAP, sRhMoveWords_INFLICT_POISON, sRhMoveWords_INFLICT_BURN,
    sRhMoveWords_INFLICT_FREEZE, sRhMoveWords_INFLICT_PARALYZE, sRhMoveWords_INFLICT_SLEEP, sRhMoveWords_INFLICT_CONFUSION,
    sRhMoveWords_DRAIN, sRhMoveWords_PUNCH, sRhMoveWords_SOUND,
};
static const u8 sActionWordCounts[AW_COUNT] = {
    ARRAY_COUNT(sRhMoveWords_PHYSICAL), ARRAY_COUNT(sRhMoveWords_SPECIAL), ARRAY_COUNT(sRhMoveWords_STATUS),
    ARRAY_COUNT(sRhMoveWords_STATUS_HEAL), ARRAY_COUNT(sRhMoveWords_STATUS_BUFF), ARRAY_COUNT(sRhMoveWords_STATUS_DEBUFF),
    ARRAY_COUNT(sRhMoveWords_STATUS_TRAP), ARRAY_COUNT(sRhMoveWords_INFLICT_POISON), ARRAY_COUNT(sRhMoveWords_INFLICT_BURN),
    ARRAY_COUNT(sRhMoveWords_INFLICT_FREEZE), ARRAY_COUNT(sRhMoveWords_INFLICT_PARALYZE), ARRAY_COUNT(sRhMoveWords_INFLICT_SLEEP),
    ARRAY_COUNT(sRhMoveWords_INFLICT_CONFUSION), ARRAY_COUNT(sRhMoveWords_DRAIN), ARRAY_COUNT(sRhMoveWords_PUNCH),
    ARRAY_COUNT(sRhMoveWords_SOUND),
};

static bool32 HasAdditionalEffect(u16 move, enum MoveEffect effect)
{
    u32 i, n = GetMoveAdditionalEffectCount(move);
    for (i = 0; i < n; i++)
        if (GetMoveAdditionalEffectById(move, i)->moveEffect == effect)
            return TRUE;
    return FALSE;
}

static u32 ActionWordList(u16 move)
{
    u32 cat = GetMoveCategory(move), effect = GetMoveEffect(move);
    bool32 trap = (effect == EFFECT_MEAN_LOOK || HasAdditionalEffect(move, MOVE_EFFECT_WRAP)
                || HasAdditionalEffect(move, MOVE_EFFECT_PREVENT_ESCAPE));
    if (gMovesInfo[move].punchingMove)
        return AW_PUNCH;
    if (gMovesInfo[move].soundMove)
        return AW_SOUND;
    if (cat != DAMAGE_CATEGORY_STATUS && HasAdditionalEffect(move, MOVE_EFFECT_ABSORB))
        return AW_DRAIN;
    if (cat == DAMAGE_CATEGORY_STATUS)
    {
        if (gMovesInfo[move].healingMove)
            return AW_HEAL;
        switch (GetMoveNonVolatileStatus(move))
        {
        case MOVE_EFFECT_POISON: case MOVE_EFFECT_TOXIC: return AW_POISON;
        case MOVE_EFFECT_BURN:                          return AW_BURN;
        case MOVE_EFFECT_FREEZE: case MOVE_EFFECT_FROSTBITE: return AW_FREEZE;
        case MOVE_EFFECT_PARALYSIS:                     return AW_PARALYZE;
        case MOVE_EFFECT_SLEEP:                         return AW_SLEEP;
        default: break;
        }
        if (effect == EFFECT_CONFUSE || effect == EFFECT_SWAGGER)
            return AW_CONFUSION;
        if (trap)
            return AW_TRAP;
        if (effect == EFFECT_STAT_CHANGE)
            return GetMoveTarget(move) == TARGET_USER ? AW_BUFF : AW_DEBUFF;
        return AW_STATUS;
    }
    if (trap)
        return AW_TRAP;
    return cat == DAMAGE_CATEGORY_PHYSICAL ? AW_PHYSICAL : AW_SPECIAL;
}

static u32 MoveNamesKey(void)
{
    return (RH_SettingsHash() + 11) | 1;
}

static bool32 FitsName(const u8 *w1, const u8 *w2)
{
    return StringLength(w1) + StringLength(w2) <= MOVE_NAME_LENGTH;   // with or without the space
}

static bool32 SameWord(const u8 *a, const u8 *b)
{
    return a == b || (a[0] == b[0] && StringCompare(a, b) == 0);
}

static void BuildMoveNames(void)
{
    u32 move, j, tries;
    // the words each move got so far, for the no-repeat check (the same word can be in two lists)
    const u8 **w1s = Alloc(MOVES_COUNT * sizeof(u8 *));
    const u8 **w2s = Alloc(MOVES_COUNT * sizeof(u8 *));
    for (move = 0; move < MOVES_COUNT; move++)
    {
        u32 typeIdx, list, h, pick = 0xFFFF;
        sMoveNames.pick[move] = 0xFFFF;
        if (w1s != NULL && w2s != NULL)
            w1s[move] = w2s[move] = NULL;
        if (!IsRandomizableMove(move))
            continue;
        typeIdx = RH_TypeIndexOf(GetMoveType(move));
        list = ActionWordList(move);
        for (tries = 0; tries < 50 && pick == 0xFFFF; tries++)
        {
            u32 w1, w2;
            bool32 used = FALSE;
            const u8 *a, *b;
            h = RH_Hash(SALT_MOVE_NAME, move, tries);
            w1 = h % sRhMoveTypeWordCounts[typeIdx];
            w2 = (h >> 12) % sActionWordCounts[list];
            a = sRhMoveTypeWords[typeIdx][w1];
            b = sActionWords[list][w2];
            if (!FitsName(a, b))
                continue;
            for (j = 0; j < move && !used && w1s != NULL && w2s != NULL; j++)
                used = (w1s[j] != NULL && SameWord(w1s[j], a) && SameWord(w2s[j], b));
            if (!used)
            {
                pick = (w1 << 8) | w2;
                if (w1s != NULL && w2s != NULL)
                    w1s[move] = a, w2s[move] = b;
            }
        }
        sMoveNames.pick[move] = pick;
    }
    if (w1s != NULL)
        Free(w1s);
    if (w2s != NULL)
        Free(w2s);
    sMoveNames.key = MoveNamesKey();
}

const u8 *RH_MoveName(enum Move move, const u8 *vanilla)
{
    u8 *buf, *end;
    const u8 *w1, *w2;
    u32 typeIdx, pick;
    if (!S->enabled || !S->moveNames || !IsRandomizableMove(move))
        return vanilla;
    if (sMoveNames.key != MoveNamesKey())
        BuildMoveNames();
    pick = sMoveNames.pick[move];
    if (pick == 0xFFFF)
        return vanilla;
    typeIdx = RH_TypeIndexOf(GetMoveType(move));
    w1 = sRhMoveTypeWords[typeIdx][pick >> 8];
    w2 = sActionWords[ActionWordList(move)][pick & 0xFF];
    buf = sMoveNameBuf[sMoveNameNext];
    sMoveNameNext = (sMoveNameNext + 1) % MOVE_NAME_BUFFERS;
    end = StringCopy(buf, w1);
    if (StringLength(w1) + 1 + StringLength(w2) <= MOVE_NAME_LENGTH)
        *end++ = CHAR_SPACE;                                 // FVX: without the space if it doesn't fit
    StringCopy(end, w2);
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
#define MF_PHYSICAL (1 << 3)
#define MF_SPECIAL  (1 << 4)

struct MoveCache
{
    u32 key;
    u8 type[RH_MOVE_COUNT];
    u8 flags[RH_MOVE_COUNT];
    u16 byType[RH_MOVE_COUNT];               // pool indexes grouped by current type
    u16 typeStart[NUMBER_OF_MON_TYPES + 1];
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
        if (GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL)
            f |= MF_PHYSICAL;
        else if (GetMoveCategory(move) == DAMAGE_CATEGORY_SPECIAL)
            f |= MF_SPECIAL;
        sMoveCache.flags[i] = f;
    }
    // counting sort by type, so a same-type pick only looks at that type's moves
    memset(sMoveCache.typeStart, 0, sizeof(sMoveCache.typeStart));
    for (i = 0; i < RH_MOVE_COUNT; i++)
        if (sMoveCache.type[i] < NUMBER_OF_MON_TYPES)
            sMoveCache.typeStart[sMoveCache.type[i] + 1]++;
    for (i = 1; i <= NUMBER_OF_MON_TYPES; i++)
        sMoveCache.typeStart[i] += sMoveCache.typeStart[i - 1];
    {
        u16 fill[NUMBER_OF_MON_TYPES];
        memcpy(fill, sMoveCache.typeStart, sizeof(fill));
        for (i = 0; i < RH_MOVE_COUNT; i++)
            if (sMoveCache.type[i] < NUMBER_OF_MON_TYPES)
                sMoveCache.byType[fill[sMoveCache.type[i]]++] = i;
    }
    sMoveCache.key = key;
}

static bool32 IsBroken(u32 i)
{
    return sRhMoves[i].broken || (sRhMoves[i].gen1Broken && S->mechanicsGen == 1);
}

// Forced good attacks lean Physical or Special by the Pokemon's Attack : Sp. Atk ratio (FVX). 0 = no preference.
static EWRAM_DATA u8 sPickCategoryFlag = 0;

// type: TYPE_NONE = any type. noHM: level-up movesets never get HM moves (FVX).
static bool32 MoveOk(u32 i, u8 type, u32 need, bool32 noBreaking, bool32 noHM)
{
    u8 f = sMoveCache.flags[i];
    if (noBreaking && IsBroken(i))
        return FALSE;
    if (sPickCategoryFlag && need != PICK_ANY && !(f & sPickCategoryFlag))
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
    if (type < NUMBER_OF_MON_TYPES && type != TYPE_NONE)
    {
        // only this type's moves
        u32 lo = sMoveCache.typeStart[type], hi = sMoveCache.typeStart[type + 1];
        for (i = lo; i < hi; i++)
            count += MoveOk(sMoveCache.byType[i], type, need, noBreaking, noHM);
        if (count != 0)
        {
            target = hash % count;
            for (i = lo; i < hi; i++)
                if (MoveOk(sMoveCache.byType[i], type, need, noBreaking, noHM) && target-- == 0)
                    return sRhMoves[sMoveCache.byType[i]].move;
        }
        type = TYPE_NONE;
    }
    // any type: random tries first (most moves qualify), a full count only when unlucky
    for (i = 0; i < 48; i++)
    {
        u32 k = RH_Hash(SALT_LEARNSET, hash, 1000 + i) % RH_MOVE_COUNT;
        if (MoveOk(k, TYPE_NONE, need, noBreaking, noHM))
            return sRhMoves[k].move;
    }
    for (i = 0; i < RH_MOVE_COUNT; i++)
        if (MoveOk(i, type, need, noBreaking, noHM))
            count++;
    if (count == 0 && sPickCategoryFlag)
    {
        sPickCategoryFlag = 0;
        return PickMove(hash, type, need, noBreaking, noHM);
    }
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

// "Random (prefer same type)" (FVX): Normal/X -> 10% Normal, 30% X; X/Y -> 20% each; single type -> 40%. Else any type.
static u8 SameTypePick(u16 species, u32 h)
{
    u8 t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
    u32 r = (h >> 3) % 100;
    if (t1 != t2 && (t1 == TYPE_NORMAL || t2 == TYPE_NORMAL))
    {
        u8 other = (t1 == TYPE_NORMAL) ? t2 : t1;
        if (r < 10)
            return TYPE_NORMAL;
        return r < 40 ? other : TYPE_NONE;
    }
    if (t1 != t2)
        return r < 20 ? t1 : (r < 40 ? t2 : TYPE_NONE);
    return r < 40 ? t1 : TYPE_NONE;
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
    {
        // "Force % Good Damaging": exactly round(% x moves) of the moves are good attacks (FVX), at hashed slots
        u8 forced[LEARNSET_MAX + 2];
        u32 want = (goodPct * n + 50) / 100, have = 0;
        u32 atk = GetSpeciesBaseAttack(species), spa = GetSpeciesBaseSpAttack(species);
        memset(forced, 0, sizeof(forced));
        forced[lastLv1] = TRUE;                              // FVX: always start with a good attack
        for (i = 0; have < want && i < n * 4; i++)
        {
            u32 k = RH_Hash(SALT_LEARNSET, species, 5000 + i) % n;
            if (!forced[k])
            {
                forced[k] = TRUE;
                have++;
            }
        }
        for (i = 0; have < want && i < n; i++)
            if (!forced[i])
                forced[i] = TRUE, have++;
        for (i = 0; i < n; i++)
        {
            u16 move = MOVE_NONE;
            u32 tries, need = forced[i] ? PICK_GOOD : PICK_ANY;
            for (tries = 0; tries < 12; tries++)
            {
                u32 h = RH_Hash(SALT_LEARNSET, species, i * 16 + tries);
                u8 type = TYPE_NONE;
                if (S->movesets == 1)
                    type = SameTypePick(species, h);
                // forced attacks: Physical with chance Atk / (Atk + SpA), else Special
                sPickCategoryFlag = 0;
                if (need == PICK_GOOD && atk + spa > 0)
                    sPickCategoryFlag = ((RH_Hash(SALT_LEARNSET, species, 9000 + i) % (atk + spa)) < atk) ? MF_PHYSICAL : MF_SPECIAL;
                move = PickMove(h >> 12, type, need, S->movesetNoGameBreaking, TRUE);
                sPickCategoryFlag = 0;
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
                u8 type = (S->movesets == 1) ? SameTypePick(species, h) : TYPE_NONE;
                u32 k;
                // egg moves follow "Force % Good Damaging" too (FVX)
                u32 need = (S->movesetGoodDamagingOn && (RH_Hash(SALT_EGG_MOVES, species, 500 + i) % 100) < S->movesetGoodDamaging) ? PICK_GOOD : PICK_ANY;
                move = PickMove(h >> 12, type, need, S->movesetNoGameBreaking, TRUE);
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
    MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH, MOVE_ROCK_SMASH, MOVE_WATERFALL,
    MOVE_DIG, MOVE_TELEPORT, MOVE_SWEET_SCENT,                // FVX's FRLG list (no Secret Power / Dive here)
};



static bool32 IsFieldMove(u16 move)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sFieldMoves); i++)
        if (sFieldMoves[i] == move)
            return TRUE;
    return FALSE;
}

// A field move that "Keep Field Move TMs" keeps on its own TM (it may not appear on another one).
static bool32 IsKeptFieldTMMove(u16 move)
{
    u32 i;
    if (!S->tmKeepFieldMoves || !IsFieldMove(move))
        return FALSE;
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
        if (gTMHMItemMoveIds[i].moveId == move)
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
        u16 move = PickMove(h >> 8, TYPE_NONE, goodPct ? PICK_GOOD : PICK_ANY, noBreaking, FALSE);
        if (IsKeptFieldTMMove(move) || UsedMachineMove(move, tmCount, tutorCount))
            continue;
        return move;
    }
    // unlucky: take the next unused move of the pool, so no TM / tutor ever repeats one
    for (tries = 0; tries < RH_MOVE_COUNT; tries++)
    {
        const struct RhMove *m = &sRhMoves[(RH_Hash(salt, index, 999) + tries) % RH_MOVE_COUNT];
        if (noBreaking && m->broken)
            continue;
        if (IsHMMove(m->move) || IsKeptFieldTMMove(m->move) || UsedMachineMove(m->move, tmCount, tutorCount))
            continue;
        return m->move;
    }
    return MOVE_NONE;
}

// "Force % Good Damaging" (FVX): exactly round(% x count) of the machines teach a good attack.
static u32 ForcedGood(u32 salt, u32 index, u32 count, u32 pct)
{
    u32 want = (pct * count + 50) / 100;
    return RH_Permute(salt + 77, index, count) < want;
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
            u16 m = RandomMachineMove(SALT_TM, i, ForcedGood(SALT_TM, i - 1, NUM_TECHNICAL_MACHINES, S->tmGoodDamagingOn ? S->tmGoodDamaging : 0),
                                      S->tmNoGameBreaking, S->tmKeepFieldMoves, i - 1, 0);
            if (m != MOVE_NONE)
                sMachines.tm[i] = m;
        }
    }
    for (i = 0; i < NUM_RH_TUTORS; i++)
    {
        sMachines.tutor[i] = sTutorMoves[i];
        if (S->tutorMoves && !(S->tutorKeepFieldMoves && IsFieldMove(sTutorMoves[i])))
        {
            u16 m = RandomMachineMove(SALT_TUTOR, i, ForcedGood(SALT_TUTOR, i, NUM_RH_TUTORS, S->tutorGoodDamagingOn ? S->tutorGoodDamaging : 0),
                                      S->tutorNoGameBreaking, S->tutorKeepFieldMoves, NUM_TECHNICAL_MACHINES, i);
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

// NPC texts such as "TM39 contains ROCK TOMB." name the vanilla move: every field message is passed through here
// and the vanilla move name that follows a "TMxx" is replaced by the move the TM teaches now (FVX rewrites them).
static u8 FoldChar(u8 ch)
{
    if (ch >= CHAR_a && ch <= CHAR_z)
        return ch - CHAR_a + CHAR_A;
    return ch;
}

static bool32 SkipChar(u8 ch)
{
    return ch == CHAR_SPACE || ch == CHAR_HYPHEN || ch == CHAR_NEWLINE;
}

// Length of the match of "name" at str (ignoring case, spaces and hyphens), 0 if none.
static u32 MatchMoveName(const u8 *str, const u8 *name)
{
    const u8 *s = str;
    if (SkipChar(*s))
        return 0;
    while (*name != EOS)
    {
        if (SkipChar(*name))
        {
            name++;
            continue;
        }
        while (*s != EOS && SkipChar(*s))
            s++;
        if (*s == EOS || FoldChar(*s) != FoldChar(*name))
            return 0;
        s++;
        name++;
    }
    return s - str;
}

void RH_FixTMText(u8 *str)
{
    u32 i, len;
    if (!S->enabled || (!S->tmMoves && S->movesets != 3 && !S->moveNames))
        return;
    len = StringLength(str);
    for (i = 0; i + 3 < len; i++)
    {
        u32 idx, j, k, m;
        u16 oldMove, newMove;
        const u8 *oldName;
        u8 newName[MOVE_NAME_LENGTH + 1];
        if (str[i] != CHAR_T || str[i + 1] != CHAR_M || str[i + 2] < CHAR_0 || str[i + 2] > CHAR_9
         || str[i + 3] < CHAR_0 || str[i + 3] > CHAR_9)
            continue;
        idx = (str[i + 2] - CHAR_0) * 10 + (str[i + 3] - CHAR_0);
        if (idx == 0 || idx > NUM_TECHNICAL_MACHINES)
            continue;
        oldMove = gTMHMItemMoveIds[idx].moveId;
        newMove = GetTMHMMoveId(idx);
        oldName = gMovesInfo[oldMove].name;
        for (j = i + 4; j < len; j++)
        {
            m = MatchMoveName(&str[j], oldName);
            if (m == 0)
                continue;
            StringCopy(newName, GetMoveName(newMove));
            for (k = 0; newName[k] != EOS; k++)
                newName[k] = FoldChar(newName[k]);           // the texts write move names in capitals
            k = StringLength(newName);
            if (len - m + k >= 900)
                return;
            memmove(&str[j + k], &str[j + m], len - (j + m) + 1);
            memcpy(&str[j], newName, k);
            len = len - m + k;
            i = j + k - 1;
            break;
        }
    }
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
        odds = 50;
    else if (RH_SpeciesHasType(species, type))
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

// TM/Tutor Levelup Move Sanity, and with Follow Evolutions also what a pre-evolution learns by level up (FVX).
static bool32 LearnsByLevelUpOrAncestor(u16 species, u16 move, bool32 followEvos)
{
    u32 depth;
    for (depth = 0; depth < 4 && species != SPECIES_NONE; depth++)
    {
        if (LearnsByLevelUp(species, move))
            return TRUE;
        if (!followEvos)
            break;
        species = RH_PreEvo(species);
    }
    return FALSE;
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
        if (S->tmLevelupSanity && LearnsByLevelUpOrAncestor(species, move, S->tmCompatFollowEvos))
            return TRUE;
        if (S->tmCompat == 0)
            return vanillaCheck(species, gTMHMItemMoveIds[tm].moveId);   // the slot keeps its original compatibility
        return RandomCompat(SALT_TM_COMPAT, S->tmCompat, S->tmCompatFollowEvos, species, move, tm, 0);
    }
    tutor = TutorSlotOfMove(move);
    if (tutor >= 0)
    {
        if (S->tutorLevelupSanity && LearnsByLevelUpOrAncestor(species, move, S->tutorCompatFollowEvos))
            return TRUE;
        if (S->tutorCompat == 0)
            return vanillaCheck(species, sTutorMoves[tutor]);
        return RandomCompat(SALT_TUTOR_COMPAT, S->tutorCompat, S->tutorCompatFollowEvos, species, move, tutor, 0);
    }
    return vanillaCheck(species, move);
}
