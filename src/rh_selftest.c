// Debug-only self-test of the randomizer options (never in RELEASE builds).
// Runs from the RH_TEST_EXTRA hook of a quickstart new game: every option is switched on in turn and the rules it
// promises are checked on the real data. Results go to gRhSelfTestLog (plain ASCII) for the emulator harness.
#include "global.h"
#include "config/rh_test.h"
#ifndef RELEASE
#include "constants/characters.h"
#include "battle.h"
#include "battle_setup.h"
#include "data.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "move.h"
#include "overworld.h"
#include "pokemon.h"
#include "string_util.h"
#include "wild_encounter.h"
#include "rh_internal.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/trainers.h"
#include "constants/rh_special_shops.h"

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------
#define LOG_SIZE 12288
EWRAM_DATA char gRhSelfTestLog[LOG_SIZE] = {0};
EWRAM_DATA u32 gRhSelfTestDone = 0;
static EWRAM_DATA u32 sLogLen = 0;
static EWRAM_DATA u32 sFails = 0;          // in the current test
static EWRAM_DATA u32 sChecks = 0;
static EWRAM_DATA u32 sTotalFails = 0;
static EWRAM_DATA u32 sTestStart = 0;

static void Log(const char *s)
{
    while (*s && sLogLen < LOG_SIZE - 1)
        gRhSelfTestLog[sLogLen++] = *s++;
    gRhSelfTestLog[sLogLen] = 0;
}

static void LogU(u32 v)
{
    char buf[12];
    s32 i = 0;
    if (v == 0)
        buf[i++] = '0';
    while (v != 0 && i < 11)
    {
        buf[i++] = '0' + v % 10;
        v /= 10;
    }
    while (i > 0 && sLogLen < LOG_SIZE - 1)
        gRhSelfTestLog[sLogLen++] = buf[--i];
    gRhSelfTestLog[sLogLen] = 0;
}

static void Begin(const char *name)
{
    RH_InvalidateSettingsHash();
    Log(name);
    Log(": ");
    sFails = 0;
    sChecks = 0;
    sTestStart = gMain.vblankCounter1;
}

static void End(void)
{
    if (sFails == 0)
        Log("ok");
    else
    {
        Log("FAILED ");
        LogU(sFails);
    }
    Log(" (");
    LogU(sChecks);
    Log(" checks, ");
    LogU(gMain.vblankCounter1 - sTestStart);
    Log(" frames)\n");
    sTotalFails += sFails;
}

// Records a failed check; the first few of each test are described.
static void Check(bool32 ok, const char *what, u32 a, u32 b)
{
    sChecks++;
    if (ok)
        return;
    if (sFails < 4)
    {
        Log("\n  [");
        Log(what);
        Log(" ");
        LogU(a);
        Log(" ");
        LogU(b);
        Log("] ");
    }
    sFails++;
}

static void Note(const char *what, u32 v)
{
    Log(what);
    Log("=");
    LogU(v);
    Log(" ");
}

static EWRAM_DATA u8 sBasePool = 0;

static void Reset(void)
{
    u8 text[RH_SEED_TEXT_LENGTH + 1];
    u32 seed = S->seed;
    memcpy(text, S->seedText, sizeof(text));
    RH_SetDefaultSettings(S);
    S->speciesPool = sBasePool;
    S->seed = seed;
    memcpy(S->seedText, text, sizeof(text));
    RH_InvalidateSettingsHash();
}

// Allowed pool species (the ones the randomizer uses), every "step"-th one.
#define FOR_POOL(sp, step) for (u32 _p = 0; _p < RH_PoolCount(); _p += (step)) if (RH_PoolAllowed(_p) && ((sp) = RH_PoolSpecies(_p)) != SPECIES_NONE)

static u32 BST(u16 sp)
{
    u32 i, t = 0;
    for (i = 0; i < NUM_STATS; i++)
        t += GetSpeciesBaseStat(sp, i);
    return t;
}

static bool32 HasType(u16 sp, u8 type)
{
    return GetSpeciesType(sp, 0) == type || GetSpeciesType(sp, 1) == type;
}

static bool32 SharesType(u16 a, u16 b)
{
    return HasType(a, GetSpeciesType(b, 0)) || HasType(a, GetSpeciesType(b, 1));
}

// ---------------------------------------------------------------------------
// Pokemon traits
// ---------------------------------------------------------------------------
static void TestBaseStats(void)
{
    u16 sp;
    u32 mode;
    for (mode = 1; mode <= 2; mode++)
    {
        Reset();
        S->baseStats = mode;
        S->baseStatsFollowEvos = TRUE;
        S->baseStatsRandomAdded = (mode == 2);
        Begin(mode == 1 ? "stats shuffle" : "stats random+added");
        FOR_POOL(sp, 1)
        {
            const struct SpeciesInfo *si = &gSpeciesInfo[sp];
            u32 v = si->baseHP + si->baseAttack + si->baseDefense + si->baseSpeed + si->baseSpAttack + si->baseSpDefense;
            Check(BST(sp) == v, "bst", sp, BST(sp));
            if (sp == SPECIES_SHEDINJA)
                Check(GetSpeciesBaseStat(sp, 0) == 1, "shedinja hp", sp, GetSpeciesBaseStat(sp, 0));
            else if (mode == 2 && v >= 100)
            {
                u32 i;
                Check(GetSpeciesBaseStat(sp, 0) >= 20, "hp min", sp, GetSpeciesBaseStat(sp, 0));
                for (i = 1; i < NUM_STATS; i++)
                    Check(GetSpeciesBaseStat(sp, i) >= 10, "stat min", sp, GetSpeciesBaseStat(sp, i));
            }
        }
        End();
    }
}

static void TestExpCurve(void)
{
    u16 sp;
    Reset();
    S->expCurve = 2;         // Medium Slow
    S->expCurveWho = 0;      // legendaries: slow
    Begin("exp curves");
    FOR_POOL(sp, 1)
        Check(GetSpeciesGrowthRate(sp) == (RH_IsLegendary(sp) ? GROWTH_SLOW : GROWTH_MEDIUM_SLOW), "curve", sp, GetSpeciesGrowthRate(sp));
    End();
}

static void TestTypes(void)
{
    u16 sp;
    u32 changed = 0;
    Reset();
    S->types = 1;
    S->forceDualTypes = TRUE;
    Begin("types follow+dual");
    FOR_POOL(sp, 1)
    {
        u16 root = RH_TraitRoot(sp);
        Check(GetSpeciesType(sp, 0) != GetSpeciesType(sp, 1), "dual", sp, GetSpeciesType(sp, 0));
        if (root != sp)
            Check(GetSpeciesType(sp, 0) == GetSpeciesType(root, 0), "follow", sp, root);
        changed += GetSpeciesType(sp, 0) != gSpeciesInfo[sp].types[0];
    }
    Check(changed > 100, "changed", changed, 0);
    End();
    Reset();
    S->types = 2;
    Begin("types completely");
    changed = 0;
    FOR_POOL(sp, 1)
    {
        Check(RH_TypeIndexOf(GetSpeciesType(sp, 0)) < 18, "valid", sp, GetSpeciesType(sp, 0));
        if (gSpeciesInfo[sp].types[0] == gSpeciesInfo[sp].types[1])
            Check(GetSpeciesType(sp, 0) == GetSpeciesType(sp, 1), "mono stays mono", sp, 0);
        changed += GetSpeciesType(sp, 0) != gSpeciesInfo[sp].types[0];
    }
    Check(changed > RH_TypeCount(0) / 4, "changed", changed, 0);
    End();
}

static void TestAbilities(void)
{
    static const u16 sBanned[] = { ABILITY_ARENA_TRAP, ABILITY_SHADOW_TAG, ABILITY_MAGNET_PULL, ABILITY_TRUANT, ABILITY_SLOW_START,
                                   ABILITY_DEFEATIST, ABILITY_MINUS, ABILITY_PLUS, ABILITY_HONEY_GATHER };
    u16 sp;
    Reset();
    S->abilities = 1;
    S->ensureTwoAbilities = TRUE;
    S->banTrapAbilities = S->banNegativeAbilities = S->banBadAbilities = TRUE;
    S->allowWonderGuard = FALSE;
    S->combineDuplicateAbilities = TRUE;
    Begin("abilities");
    FOR_POOL(sp, 1)
    {
        u32 s, t, k;
        u16 a[3];
        for (s = 0; s < 3; s++)
            a[s] = GetSpeciesAbility(sp, s);
        Check(a[0] != ABILITY_NONE, "slot0", sp, 0);
        Check(a[1] != ABILITY_NONE || sp == SPECIES_SHEDINJA, "ensure two", sp, 0);
        for (s = 0; s < 3; s++)
        {
            for (t = s + 1; t < 3; t++)
                Check(a[s] == ABILITY_NONE || a[s] != a[t], "dup", sp, a[s]);
            for (k = 0; k < ARRAY_COUNT(sBanned); k++)
                Check(a[s] != sBanned[k], "banned", sp, a[s]);
            if (sp != SPECIES_SHEDINJA)
                Check(a[s] != ABILITY_WONDER_GUARD, "wonder guard", sp, s);
        }
    }
    End();
}

// Every evolution target of "sp" (current evolutions).
static u32 Targets(u16 sp, u16 *out)
{
    const struct Evolution *e = GetSpeciesEvolutions(sp);
    u32 i, j, n = 0;
    for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END && n < 12; i++)
    {
        if (e[i].method == EVO_NONE || e[i].targetSpecies == SPECIES_NONE)
            continue;
        for (j = 0; j < n && out[j] != e[i].targetSpecies; j++)
            ;
        if (j == n)
            out[n++] = e[i].targetSpecies;
    }
    return n;
}

static u32 Depth(u16 sp, u32 guard)
{
    u16 t[12];
    u32 i, n, best = 0;
    if (guard == 0)
        return 99;
    n = Targets(sp, t);
    for (i = 0; i < n; i++)
        best = max(best, 1 + Depth(t[i], guard - 1));
    return best;
}

static bool32 VanillaTarget(u16 sp, u16 target)
{
    const struct Evolution *v = GetSpeciesEvolutionsVanilla(sp);
    u32 i;
    for (i = 0; v != NULL && v[i].method != EVOLUTIONS_END; i++)
        if (v[i].targetSpecies == target)
            return TRUE;
    return FALSE;
}

static void TestEvolutionsWith(const char *name, u32 mode, bool32 noConv, bool32 force, bool32 growth, bool32 typing, bool32 three)
{
    u8 *seen = AllocZeroed(NUM_SPECIES);
    u16 sp;
    Reset();
    S->evolutions = mode;
    S->evoNoConvergence = noConv;
    S->evoForceChange = force;
    S->evoForceGrowth = growth;
    S->evoSameTyping = typing;
    S->evoLimitThreeStages = three;
    Begin(name);
    FOR_POOL(sp, 1)
    {
        u16 t[12];
        u32 i, n = Targets(sp, t);
        if (mode == 2)
            Check(n == 1, "every level has one", sp, n);
        for (i = 0; i < n; i++)
        {
            Check(t[i] != sp && t[i] < NUM_SPECIES, "target", sp, t[i]);
            Check(GetSpeciesGrowthRate(t[i]) == GetSpeciesGrowthRate(sp), "growth rate", sp, t[i]);
            if (noConv && seen != NULL)
            {
                Check(seen[t[i]] == 0, "converges", sp, t[i]);
                seen[t[i]] = 1;
            }
            if (force)
                Check(!VanillaTarget(sp, t[i]), "force change", sp, t[i]);
            if (growth)
                Check(RH_VanillaBST(t[i]) > RH_VanillaBST(sp), "force growth", sp, t[i]);
            if (typing)
                Check(SharesType(t[i], sp), "same typing", sp, t[i]);
        }
        if (mode == 1)
            Check(Depth(sp, 8) <= (three ? 2 : 7), "depth/cycle", sp, Depth(sp, 8));
    }
    Free(seen);
    End();
}

static void TestEvolutionFixes(void)
{
    u16 sp;
    Reset();
    S->evoChangeImpossible = TRUE;
    S->evoRemoveTimeBased = TRUE;
    S->evoMakeEasier = 40;
    Begin("evo impossible/time/easier");
    FOR_POOL(sp, 1)
    {
        const struct Evolution *e = GetSpeciesEvolutions(sp);
        u32 i, k;
        for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
        {
            if (e[i].method == EVO_NONE)
                continue;
            Check(e[i].method != EVO_TRADE && e[i].method != EVO_SCRIPT_TRIGGER && e[i].method != EVO_SPIN, "impossible", sp, e[i].method);
            if (e[i].method == EVO_LEVEL)
                Check(e[i].param <= 40, "easier", sp, e[i].param);
            for (k = 0; e[i].params != NULL && e[i].params[k].condition != CONDITIONS_END; k++)
            {
                u16 c = e[i].params[k].condition;
                Check(c != IF_TIME && c != IF_NOT_TIME, "time", sp, c);
                Check(c != IF_IN_MAP && c != IF_IN_MAPSEC && c != IF_TRADE_PARTNER_SPECIES, "impossible cond", sp, c);
            }
        }
    }
    // Eevee keeps every stone and Leafeon/Glaceon aren't forced at a level
    {
        u16 t[12];
        Check(Targets(SPECIES_EEVEE, t) >= 7, "eevee targets", Targets(SPECIES_EEVEE, t), 0);
    }
    End();
}

// ---------------------------------------------------------------------------
// Type chart
// ---------------------------------------------------------------------------
static u8 Eff(uq4_12_t m)
{
    return m == UQ_4_12(0.0) ? 0 : m < UQ_4_12(1.0) ? 1 : m > UQ_4_12(1.0) ? 3 : 2;
}

static void TestTypeChart(void)
{
    static const char *const sNames[] = { "", "chart random", "chart balanced", "chart identities", "chart inverse" };
    u32 mode;
    for (mode = 1; mode <= 4; mode++)
    {
        u32 a, d, e, baseCount[4] = {0}, count[4] = {0}, changed = 0;
        Reset();
        S->typeChart = mode;
        S->inverseRandomImmunities = (mode == 4);
        Begin(sNames[mode]);
        for (a = 0; a < 18; a++)
        {
            u32 rowB[4] = {0}, row[4] = {0};
            for (d = 0; d < 18; d++)
            {
                u8 at = gRhMonTypes[a], de = gRhMonTypes[d];
                u8 b = Eff(gTypeEffectivenessTable[at][de]);
                u8 m = Eff(RH_TypeModifier(at, de, gTypeEffectivenessTable[at][de]));
                baseCount[b]++;
                count[m]++;
                rowB[b]++;
                row[m]++;
                changed += (b != m);
                if (mode == 4)
                {
                    if (b == 3)
                        Check(m == 1 || m == 0, "inverse SE", a, d);
                    if (b == 0 || b == 1)
                        Check(m == 3, "inverse NVE", a, d);
                }
            }
            if (mode == 3)
                for (e = 0; e < 4; e++)
                    Check(rowB[e] == row[e], "row count", a, e);
        }
        if (mode == 3)
        {
            for (d = 0; d < 18; d++)
            {
                u32 colB[4] = {0}, col[4] = {0};
                for (a = 0; a < 18; a++)
                {
                    u8 at = gRhMonTypes[a], de = gRhMonTypes[d];
                    colB[Eff(gTypeEffectivenessTable[at][de])]++;
                    col[Eff(RH_TypeModifier(at, de, gTypeEffectivenessTable[at][de]))]++;
                }
                for (e = 0; e < 4; e++)
                    Check(colB[e] == col[e], "col count", d, e);
            }
        }
        if (mode <= 3)
            for (e = 0; e < 4; e++)
                Check(baseCount[e] == count[e], "count", e, count[e]);
        if (mode == 4)
            Check(count[0] == baseCount[0], "immunities", count[0], baseCount[0]);
        Check(changed > 20, "changed", changed, 0);
        End();
    }
}

// ---------------------------------------------------------------------------
// Starters / statics / trades
// ---------------------------------------------------------------------------
static void TestStartersWith(const char *name, u32 mode, u32 typesMode, bool32 noLegends, bool32 noDual, bool32 bst)
{
    u16 st[3];
    u32 i, j;
    Reset();
    S->starters = mode;
    S->starterTypes = typesMode;
    S->starterNoLegends = noLegends;
    S->starterNoDualTypes = noDual;
    S->starterBstMinOn = S->starterBstMaxOn = bst;
    S->starterBstMin = 300;
    S->starterBstMax = 400;
    Begin(name);
    for (i = 0; i < 3; i++)
        st[i] = RH_StarterForSlot(i);
    for (i = 0; i < 3; i++)
    {
        Check(st[i] != SPECIES_NONE, "none", i, 0);
        for (j = i + 1; j < 3; j++)
            Check(st[i] != st[j], "distinct", st[i], st[j]);
        if (noLegends)
            Check(!RH_IsLegendary(st[i]), "legend", st[i], 0);
        if (noDual)
            Check(GetSpeciesType(st[i], 0) == GetSpeciesType(st[i], 1), "dual", st[i], 0);
        if (bst)
            Check(RH_VanillaBST(st[i]) >= 300 && RH_VanillaBST(st[i]) <= 400, "bst", st[i], RH_VanillaBST(st[i]));
        if (mode == 4)
            Check(RH_PreEvo(st[i]) == SPECIES_NONE, "basic", st[i], 0);
        if (mode == 3)
            Check(RH_PreEvo(st[i]) == SPECIES_NONE && Depth(st[i], 4) == 2, "two evolutions", st[i], Depth(st[i], 4));
    }
    switch (typesMode)
    {
    case 1:
        Check(HasType(st[0], TYPE_GRASS) && HasType(st[1], TYPE_FIRE) && HasType(st[2], TYPE_WATER), "fire/water/grass", st[0], st[1]);
        break;
    case 2:     // slot0 beats slot2, slot2 beats slot1, slot1 beats slot0
        Check(GetTypeModifier(GetSpeciesType(st[0], 0), GetSpeciesType(st[2], 0)) > UQ_4_12(1.0)
           || GetTypeModifier(GetSpeciesType(st[0], 0), GetSpeciesType(st[2], 1)) > UQ_4_12(1.0), "triangle 0>2", st[0], st[2]);
        break;
    case 3:
        for (i = 0; i < 3; i++)
            for (j = i + 1; j < 3; j++)
                Check(!SharesType(st[i], st[j]), "unique types", st[i], st[j]);
        break;
    case 4:
        Check(SharesType(st[0], st[1]) && SharesType(st[1], st[2]), "single type", st[0], st[1]);
        break;
    }
    End();
}

static void TestStatics(void)
{
    u32 mode;
    for (mode = 1; mode <= 3; mode++)
    {
        u32 i, j, n = RH_DebugStaticCount();
        Reset();
        S->statics = mode;
        Begin(mode == 1 ? "statics swap" : mode == 2 ? "statics random" : "statics similar");
        for (i = 0; i < n; i++)
        {
            u16 o = RH_DebugStaticOriginal(i), r = RH_DebugStaticResult(i);
            Check(r != SPECIES_NONE && r < NUM_SPECIES, "valid", o, r);
            for (j = 0; j < i; j++)
                Check(RH_DebugStaticResult(j) != r, "repeat", o, r);
            if (mode == 1)
                Check(RH_IsLegendary(o) == RH_IsLegendary(r), "legend swap", o, r);
        }
        End();
    }
}

static void TestTrades(void)
{
    u32 i, j, n = RH_InGameTradeCount();
    Reset();
    S->trades = 2;
    S->tradeNicknames = S->tradeOTs = S->tradeIVs = S->tradeItems = TRUE;
    Begin("trades");
    for (i = 0; i < n; i++)
    {
        u16 g0, r0, g, r;
        RH_InGameTradeSpecies(i, &g0, &r0);
        g = RH_TradeSpecies(i, g0, FALSE);
        r = RH_TradeSpecies(i, r0, TRUE);
        Check(g != r, "given==asked", g, r);
        for (j = 0; j < i; j++)
        {
            u16 gj0, rj0;
            RH_InGameTradeSpecies(j, &gj0, &rj0);
            Check(RH_TradeSpecies(j, gj0, FALSE) != g && RH_TradeSpecies(j, rj0, TRUE) != r, "repeat", i, j);
        }
    }
    End();
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------
static bool32 InMovePool(u16 move)
{
    return move != MOVE_NONE && move < MOVES_COUNT_GEN9 && move != MOVE_STRUGGLE && gMovesInfo[move].effect != EFFECT_PLACEHOLDER;
}

static void TestMoveData(void)
{
    u32 m, changed = 0;
    Reset();
    S->movePower = S->moveAccuracy = S->movePP = S->moveType = S->moveCategory = S->moveNames = TRUE;
    Begin("move data");
    for (m = 1; m < MOVES_COUNT_GEN9; m++)
    {
        u32 p = GetMovePower(m), a = GetMoveAccuracy(m), pp = GetMovePP(m);
        const u8 *name;
        if (!InMovePool(m))
            continue;
        if (gMovesInfo[m].power > 1)
            Check(p >= 5 && p <= 250 && p % 5 == 0, "power", m, p);
        if (gMovesInfo[m].power <= 1)
            Check(p == gMovesInfo[m].power, "status power", m, p);
        if (gMovesInfo[m].accuracy == 0)
            Check(a == 0, "sure hit", m, a);
        else
            Check(a >= 20 && a <= 100, "accuracy", m, a);
        if (gMovesInfo[m].effect == EFFECT_OHKO)
            Check(a <= 75, "ohko acc", m, a);
        Check(pp >= 5 && pp <= 40, "pp", m, pp);
        Check(RH_TypeIndexOf(GetMoveType(m)) < 18 || gMovesInfo[m].type == TYPE_MYSTERY, "type", m, GetMoveType(m));
        if (gMovesInfo[m].category == DAMAGE_CATEGORY_STATUS)
            Check(GetMoveCategory(m) == DAMAGE_CATEGORY_STATUS, "status cat", m, 0);
        name = GetMoveName(m);
        Check(name[0] != EOS && StringLength(name) <= MOVE_NAME_LENGTH, "name", m, StringLength(name));
        changed += (p != gMovesInfo[m].power);
    }
    Check(changed > 200, "changed", changed, 0);
    End();
}

static void TestMovesetsWith(const char *name, u32 mode, bool32 level1, bool32 noBreaking, bool32 reorder)
{
    u16 sp;
    Reset();
    S->movesets = mode;
    S->guaranteedLevel1On = level1;
    S->guaranteedLevel1Moves = 4;
    S->movesetNoGameBreaking = noBreaking;
    S->reorderDamagingMoves = reorder;
    S->movesetGoodDamagingOn = TRUE;
    S->movesetGoodDamaging = 30;
    Begin(name);
    FOR_POOL(sp, 2)
    {
        const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(sp);
        u32 i, j, lv1 = 0, lv1Attack = 0;
        for (i = 0; l[i].move != LEVEL_UP_MOVE_END && i < 64; i++)
        {
            u16 m = l[i].move;
            if (mode == 3)
            {
                Check(m == MOVE_METRONOME, "metronome", sp, m);
                continue;
            }
            for (j = 0; j < i; j++)
                Check(l[j].move != m, "duplicate", sp, m);
            if (l[i].level <= 1)
            {
                lv1 += (l[i].level == 1);
                lv1Attack += (GetMoveCategory(m) != DAMAGE_CATEGORY_STATUS && GetMovePower(m) > 1);
            }
            if (noBreaking)
                Check(m != MOVE_SONIC_BOOM && m != MOVE_DRAGON_RAGE, "game breaking", sp, m);
            Check(m != MOVE_CUT && m != MOVE_SURF && m != MOVE_FLY && m != MOVE_STRENGTH, "hm move", sp, m);
        }
        if (mode != 3)
        {
            Check(lv1Attack >= 1, "level-1 attack", sp, 0);
            if (level1)
                Check(lv1 >= 4, "level-1 count", sp, lv1);
        }
    }
    End();
}

static void TestMachines(void)
{
    u16 moves[NUM_TECHNICAL_MACHINES + 64];
    u32 i, j, n = 0;
    u16 sp;
    Reset();
    S->tmMoves = 1;
    S->tutorMoves = 1;
    S->tmKeepFieldMoves = TRUE;
    Begin("tm/tutor moves");
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
        moves[n++] = GetTMHMMoveId(i);
    for (i = NUM_TECHNICAL_MACHINES + 1; i <= NUM_ALL_MACHINES; i++)
        Check(GetTMHMMoveId(i) == gTMHMItemMoveIds[i].moveId, "hm changed", i, 0);
    for (i = 0; i < n; i++)
    {
        Check(moves[i] != MOVE_NONE, "none", i, 0);
        for (j = 0; j < i; j++)
            Check(moves[i] != moves[j], "tm repeat", i, moves[i]);
        for (j = NUM_TECHNICAL_MACHINES + 1; j <= NUM_ALL_MACHINES; j++)
            Check(moves[i] != gTMHMItemMoveIds[j].moveId, "tm is hm move", i, moves[i]);
    }
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
        if (gTMHMItemMoveIds[i].moveId == MOVE_DIG || gTMHMItemMoveIds[i].moveId == MOVE_SECRET_POWER)
            Check(GetTMHMMoveId(i) == gTMHMItemMoveIds[i].moveId, "field tm kept", i, 0);
    End();

    Reset();
    S->tmCompat = 3;
    Begin("tm full compat");
    FOR_POOL(sp, 7)
        for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
            Check(CanLearnTeachableMove(sp, GetTMHMMoveId(i)), "full", sp, i);
    End();

    Reset();
    S->tmCompat = 2;
    S->tmLevelupSanity = TRUE;
    S->fullHMCompat = TRUE;
    Begin("tm sanity/full hm");
    FOR_POOL(sp, 3)
    {
        const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(sp);
        for (i = 0; l[i].move != LEVEL_UP_MOVE_END && i < 64; i++)
            for (j = 1; j <= NUM_TECHNICAL_MACHINES; j++)
                if (GetTMHMMoveId(j) == l[i].move)
                    Check(CanLearnTeachableMove(sp, l[i].move), "sanity", sp, l[i].move);
        for (j = NUM_TECHNICAL_MACHINES + 1; j <= NUM_ALL_MACHINES; j++)
            Check(CanLearnTeachableMove(sp, GetTMHMMoveId(j)), "full hm", sp, j);
    }
    End();

    Reset();
    S->movesets = 3;
    Begin("metronome tms");
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
        Check(GetTMHMMoveId(i) == MOVE_METRONOME, "tm", i, GetTMHMMoveId(i));
    Check(GetMovePP(MOVE_METRONOME) == 40, "pp", GetMovePP(MOVE_METRONOME), 0);
    End();
}

// ---------------------------------------------------------------------------
// Trainers
// ---------------------------------------------------------------------------
static bool32 Reachable(u16 from, u16 to, u32 depth)
{
    u16 t[12];
    u32 i, n;
    if (from == to)
        return TRUE;
    if (depth == 0)
        return FALSE;
    n = Targets(from, t);
    for (i = 0; i < n; i++)
        if (Reachable(t[i], to, depth - 1))
            return TRUE;
    return FALSE;
}

static bool32 IsLeagueTrainer(u32 id)
{
    if ((id >= TRAINER_ELITE_FOUR_LORELEI_2 && id <= TRAINER_ELITE_FOUR_LANCE_2)
     || (id >= TRAINER_CHAMPION_REMATCH_SQUIRTLE && id <= TRAINER_CHAMPION_REMATCH_CHARMANDER))
        return TRUE;
    return id == TRAINER_ELITE_FOUR_LORELEI || id == TRAINER_ELITE_FOUR_BRUNO || id == TRAINER_ELITE_FOUR_AGATHA
        || id == TRAINER_ELITE_FOUR_LANCE || id == TRAINER_CHAMPION_FIRST_SQUIRTLE || id == TRAINER_CHAMPION_FIRST_BULBASAUR
        || id == TRAINER_CHAMPION_FIRST_CHARMANDER;
}

static void TestTrainersWith(const char *name, u32 mode, u32 step)
{
    u32 id, worst = 0, worstId = 0;
    Reset();
    S->trainers = mode;
    S->starters = 2;
    S->rivalCarriesTeam = TRUE;
    S->trainerNoLegends = TRUE;
    S->trainerAvoidDuplicates = TRUE;
    S->leagueUnique = 2;
    S->additionalMons[2] = 1;
    S->heldItemsFor[0] = S->heldItemsFor[2] = TRUE;
    S->heldSensible = TRUE;
    S->trainerLevelModOn = TRUE;
    S->trainerLevelMod = 20;
    S->trainersEvolveOn = TRUE;
    S->battleStyle = 1;
    S->trainerLocalPokemon = (mode == 1);
    Begin(name);
    gBattleTypeFlags = 0;
    for (id = 1; id < TRAINERS_COUNT; id += step)
    {
        const struct Trainer *t = GetTrainerStructFromId(id);
        struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
        u32 i, j, count = 0, start;
        bool32 rival = t->trainerClass == TRAINER_CLASS_RIVAL_EARLY_FRLG || t->trainerClass == TRAINER_CLASS_RIVAL_LATE_FRLG
                    || t->trainerClass == TRAINER_CLASS_CHAMPION_FRLG;
        if (t->partySize == 0 || t->party == NULL)
            continue;
        start = gMain.vblankCounter1;
        CreateNPCTrainerPartyFromTrainer(party, t);
        if (gMain.vblankCounter1 - start > worst)
        {
            worst = gMain.vblankCounter1 - start;
            worstId = id;
        }
        for (i = 0; i < PARTY_SIZE; i++)
            count += GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE;
        Check(count >= t->partySize, "count", id, count);
        if (t->trainerClass != TRAINER_CLASS_LEADER_FRLG && t->trainerClass != TRAINER_CLASS_ELITE_FOUR_FRLG && t->trainerClass != TRAINER_CLASS_CHAMPION_FRLG
         && !rival && t->trainerClass != TRAINER_CLASS_BOSS_FRLG && id != TRAINER_RIVAL_OAKS_LAB_SQUIRTLE)
            Check(count >= min(PARTY_SIZE, t->partySize + 1), "additional", id, count);
        for (i = 0; i < count; i++)
        {
            u16 sp = GetMonData(&party[i], MON_DATA_SPECIES);
            Check(sp < NUM_SPECIES, "species", id, sp);
            Check(!RH_IsLegendary(sp) || (rival && Reachable(RH_StarterForSlot(0), sp, 3)) || (rival && Reachable(RH_StarterForSlot(1), sp, 3))
               || (rival && Reachable(RH_StarterForSlot(2), sp, 3)), "legend", id, sp);
            if (!IsLeagueTrainer(id))
                Check(!RH_IsLeagueReserved(sp), "league unique", id, sp);
            for (j = 0; j < i; j++)
                Check(RH_FamilyRoot(GetMonData(&party[j], MON_DATA_SPECIES)) != RH_FamilyRoot(sp) || rival, "duplicate", id, sp);
            if (t->trainerClass == TRAINER_CLASS_LEADER_FRLG || t->trainerClass == TRAINER_CLASS_ELITE_FOUR_FRLG)
                Check(GetMonData(&party[i], MON_DATA_HELD_ITEM) != ITEM_NONE, "held item", id, i);
        }
        // the rival's starter: his slot with a starter-line Pokemon now has the matching random starter's line
        if (rival)
        {
            for (i = 0; i < t->partySize; i++)
            {
                u32 stage;
                s32 slot = RH_StarterFamily(t->party[i].species, &stage);
                bool32 found = FALSE;
                if (slot < 0)
                    continue;
                for (j = 0; j < count; j++)
                    found |= Reachable(RH_StarterForSlot(slot), GetMonData(&party[j], MON_DATA_SPECIES), 3);
                Check(found, "rival starter", id, slot);
            }
        }
        if (mode >= 3 && (t->trainerClass == TRAINER_CLASS_LEADER_FRLG || t->trainerClass == TRAINER_CLASS_ELITE_FOUR_FRLG))
        {
            // themed: every Pokemon shares a type with the first one
            u16 first = GetMonData(&party[0], MON_DATA_SPECIES);
            u32 k, shared = 0;
            for (k = 0; k < 2; k++)
            {
                u8 type = GetSpeciesType(first, k);
                for (i = 1; i < count && HasType(GetMonData(&party[i], MON_DATA_SPECIES), type); i++)
                    ;
                if (i == count)
                    shared = 1;
            }
            if (!shared)
            {
                Note("team", id);
                for (i = 0; i < count; i++)
                    Note("", GetMonData(&party[i], MON_DATA_SPECIES));
            }
            Check(shared, "gym theme", id, first);
        }
    }
    Note("worst frames", worst);
    Note("trainer", worstId);
    if (worstId != 0)
    {
        u32 start = gMain.vblankCounter1;
        CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(worstId));
        Note("again", gMain.vblankCounter1 - start);
    }
    End();
}

static u32 TimeParty(u32 id)
{
    u32 start, r;
    RH_InvalidateSettingsHash();
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(id));   // warm caches
    start = gMain.vblankCounter1;
    for (r = 0; r < 4; r++)
        CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(id));
    return (gMain.vblankCounter1 - start) * 100 / 4;
}

static bool32 AlwaysTrue(u16 sp) { return sp != 0; }
static void BenchPick(void)
{
    struct RhFilter f = {0};
    u32 start, r, sink = 0;
    Reset(); S->trainers = 1;
    Log("pick bench (1/100 frames per call): ");
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_PickWithFilter(&f, r);
    Note("empty", (gMain.vblankCounter1 - start) * 5);
    f.extra = AlwaysTrue;
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_PickWithFilter(&f, r);
    Note("extra", (gMain.vblankCounter1 - start) * 5);
    f.extra = NULL; f.noLeagueReserved = TRUE;
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_PickWithFilter(&f, r);
    Note("noReserved", (gMain.vblankCounter1 - start) * 5);
    f.noLeagueReserved = FALSE; f.type = TYPE_FIRE;
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_PickWithFilter(&f, r);
    Note("type", (gMain.vblankCounter1 - start) * 5);
    f.type = TYPE_NONE; f.minBst = 300; f.maxBst = 400;
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_PickSpecies(&f, r, SPECIES_PIDGEY);
    Note("similar", (gMain.vblankCounter1 - start) * 5);
    start = gMain.vblankCounter1;
    for (r = 0; r < 20; r++) sink += RH_SettingsHash();
    Note("hash", (gMain.vblankCounter1 - start) * 5);
    Note("sink", sink & 1);
    Log("\n");
}

static void BenchTrainer(void)
{
    u32 id = 301;
    BenchPick();
    Log("bench (1/100 frames per party): ");
    Reset(); Note("vanilla", TimeParty(id));
    Reset(); S->trainers = 1; Note("random", TimeParty(id));
    Reset(); S->trainers = 2; Note("even", TimeParty(id));
    Reset(); S->trainers = 3; Note("themed", TimeParty(id));
    Reset(); S->trainers = 1; S->trainerSimilarStrength = TRUE; Note("+similar", TimeParty(id));
    Reset(); S->trainers = 1; S->leagueUnique = 2; Note("+league", TimeParty(id));
    Reset(); S->trainers = 1; S->trainerAvoidDuplicates = TRUE; Note("+nodup", TimeParty(id));
    Reset(); S->trainers = 1; S->diverseTypes[2] = TRUE; Note("+diverse", TimeParty(id));
    Reset(); S->trainers = 1; S->heldItemsFor[2] = TRUE; S->heldSensible = TRUE; Note("+sensible", TimeParty(id));
    Reset(); S->trainers = 1; S->betterMovesets[2] = TRUE; Note("+better", TimeParty(id));
    Reset(); S->trainers = 1; S->trainersEvolveOn = TRUE; Note("+evolve", TimeParty(id));
    Reset(); S->trainers = 1; S->additionalMons[2] = 2; Note("+2 mons", TimeParty(id));
    Reset(); S->trainers = 1; S->types = 2; Note("+types", TimeParty(id));
    Reset(); S->trainers = 1; S->movesets = 2; Note("+movesets", TimeParty(id));
    Reset(); S->trainers = 1; S->abilities = 1; Note("+abilities", TimeParty(id));
    Reset(); S->trainers = 1; S->evolutions = 1; S->trainersEvolveOn = TRUE; Note("+evos", TimeParty(id));
    Reset(); S->trainers = 1; S->baseStats = 2; Note("+stats", TimeParty(id));
    Log("\n");
}

// ---------------------------------------------------------------------------
// Wild
// ---------------------------------------------------------------------------
static void TestWildWith(const char *name, u32 zone, bool32 catchAll, u32 typeMode, bool32 similar)
{
    u16 *owner = AllocZeroed(NUM_SPECIES * 2);      // result -> vanilla species (catch 'em all)
    u16 *global = AllocZeroed(NUM_SPECIES * 2);     // vanilla species -> result (whole game zone)
    u32 h, worst = 0;
    Reset();
    S->wild = TRUE;
    S->wildZone = zone;
    S->wildCatchEmAll = catchAll;
    S->wildTypeRestriction = typeMode;
    S->wildSimilarStrength = similar;
    S->wildNoLegends = TRUE;
    S->wildMegas = FALSE;
    Begin(name);
    for (h = 0; gWildMonHeaders[h].mapGroup != MAP_GROUP(MAP_UNDEFINED); h++)
    {
        const struct WildPokemonHeader *hd = &gWildMonHeaders[h];
        const struct WildPokemonInfo *infos[4] = { hd->encounterTypes[0].landMonsInfo, hd->encounterTypes[0].waterMonsInfo,
                                                   hd->encounterTypes[0].rockSmashMonsInfo, hd->encounterTypes[0].fishingMonsInfo };
        static const u8 sCounts[4] = { 12, 5, 5, 10 };
        u32 a, start = gMain.vblankCounter1;
        for (a = 0; a < 4; a++)
        {
            u32 s, k;
            u16 res[12] = {0};
            if (infos[a] == NULL)
                continue;
            for (s = 0; s < sCounts[a]; s++)
            {
                u16 v = infos[a]->wildPokemon[s].species, r = RH_WildSpeciesAt(hd->mapGroup, hd->mapNum, infos[a], s, a, 10);
                res[s] = r;
                Check(r != SPECIES_NONE && r < NUM_SPECIES, "valid", h, r);
                Check(!RH_IsLegendary(r), "legend", h, r);
                for (k = 0; k < s && zone != 3; k++)
                    if (infos[a]->wildPokemon[k].species == v)
                        Check(res[k] == r, "same species same result", h, v);
                if (zone == 0)
                {
                    if (global[v] == 0)
                        global[v] = r;
                    Check(global[v] == r, "whole game zone", v, r);
                }
                if (catchAll && zone == 0)
                {
                    if (owner[r] == 0)
                        owner[r] = v;
                    Check(owner[r] == v, "catch em all repeat", v, r);
                }
            }
            if (typeMode == 1 && zone == 2)
            {
                u8 type = GetSpeciesType(res[0], 0);
                u32 ok0 = 1, ok1 = 1;
                for (s = 1; s < sCounts[a]; s++)
                {
                    ok0 &= HasType(res[s], type);
                    ok1 &= HasType(res[s], GetSpeciesType(res[0], 1));
                }
                Check(ok0 || ok1, "zone theme", h, a);
            }
        }
        if (gMain.vblankCounter1 - start > worst)
            worst = gMain.vblankCounter1 - start;
    }
    Note("worst map frames", worst);
    Free(owner);
    Free(global);
    End();
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------
static void TestSpecialShops(void)
{
    u16 *buf = Alloc(128 * 2);
    u16 *all = Alloc(600 * 2);
    u32 list, n = 0, i, j;
    Reset();
    S->shopSpecial = TRUE;
    Begin("special shops");
    for (list = 0; list < 9; list++)
    {
        u32 c = RH_BuildSpecialShop(list, buf, 128);
        Check(c > 0, "empty", list, 0);
        for (i = 0; i < c && n < 600; i++)
        {
            Check(buf[i] != ITEM_NONE && GetItemPocket(buf[i]) != POCKET_KEY_ITEMS, "item", list, buf[i]);
            for (j = 0; j < n; j++)
                Check(all[j] != buf[i], "duplicate", list, buf[i]);
            all[n++] = buf[i];
        }
    }
    Note("items", n);
    End();
    Free(buf);
    Free(all);
}

static void TestPrices(void)
{
    Reset();
    S->shopBalancePrices = TRUE;
    S->shopAddCheapRareCandy = TRUE;
    Begin("prices");
    Check(GetItemPrice(ITEM_RARE_CANDY) == 10, "rare candy", GetItemPrice(ITEM_RARE_CANDY), 0);
    Check(GetItemPrice(ITEM_TM07) >= 1000 && GetItemPrice(ITEM_TM07) <= 5000, "tm07", GetItemPrice(ITEM_TM07), 0);
    Check(GetItemPrice(ITEM_POTION) > 0 && GetItemPrice(ITEM_POTION) < 1000, "potion", GetItemPrice(ITEM_POTION), 0);
    End();
}

// ---------------------------------------------------------------------------
static void RunSuite(void)
{
    if (sBasePool == RH_POOL_ALL)
        BenchTrainer();
    TestBaseStats();
    TestExpCurve();
    TestTypes();
    TestAbilities();
    TestEvolutionsWith("evo random", 1, FALSE, FALSE, FALSE, FALSE, FALSE);
    TestEvolutionsWith("evo random noconv+force", 1, TRUE, TRUE, FALSE, FALSE, FALSE);
    TestEvolutionsWith("evo random growth+typing+3", 1, FALSE, FALSE, TRUE, TRUE, TRUE);
    TestEvolutionsWith("evo random all rules", 1, TRUE, TRUE, TRUE, TRUE, TRUE);
    TestEvolutionsWith("evo every level", 2, FALSE, FALSE, FALSE, FALSE, FALSE);
    TestEvolutionsWith("evo every level noconv", 2, TRUE, TRUE, FALSE, FALSE, FALSE);
    TestEvolutionFixes();
    TestTypeChart();
    TestStartersWith("starters random", 2, 0, TRUE, FALSE, FALSE);
    TestStartersWith("starters fwg", 2, 1, TRUE, FALSE, FALSE);
    TestStartersWith("starters triangle", 2, 2, TRUE, FALSE, FALSE);
    TestStartersWith("starters unique", 2, 3, TRUE, FALSE, FALSE);
    TestStartersWith("starters single", 2, 4, TRUE, TRUE, FALSE);
    TestStartersWith("starters 2evo bst", 3, 0, TRUE, FALSE, TRUE);
    TestStartersWith("starters basic", 4, 3, FALSE, FALSE, FALSE);
    TestStatics();
    TestTrades();
    TestMoveData();
    TestMovesetsWith("movesets same type", 1, TRUE, TRUE, TRUE);
    TestMovesetsWith("movesets random", 2, FALSE, TRUE, FALSE);
    TestMovesetsWith("movesets metronome", 3, FALSE, FALSE, FALSE);
    TestMachines();
    TestTrainersWith("trainers random+local", 1, 3);
    TestTrainersWith("trainers even", 2, 5);
    TestTrainersWith("trainers themed", 3, 4);
    TestTrainersWith("trainers gym themes", 4, 4);
    TestTrainersWith("trainers keep themes", 5, 6);
    TestWildWith("wild whole game+catch all", 0, TRUE, 0, TRUE);
    TestWildWith("wild per location+primary", 1, FALSE, 2, TRUE);
    TestWildWith("wild per set+zone themes", 2, FALSE, 1, FALSE);
    TestWildWith("wild per map+catch all", 5, TRUE, 0, FALSE);
    TestWildWith("wild max possible", 3, FALSE, 0, TRUE);
    TestSpecialShops();
    TestPrices();

}

void RH_SelfTest(void)
{
    struct RhSettings saved = *S;
    static const u8 sPools[] = { RH_POOL_ALL, RH_POOL_GEN1, RH_POOL_ALL_FORMS };
    static const char *const sPoolNames[] = { "=== pool: all 1025\n", "=== pool: Kanto 151\n", "=== pool: all + forms\n" };
    u32 pool;
    sLogLen = 0;
    sTotalFails = 0;
    gRhSelfTestLog[0] = 0;
    Log("RH SELF TEST\n");
    for (pool = 0; pool < ARRAY_COUNT(sPools); pool++)
    {
        sBasePool = sPools[pool];
        Log(sPoolNames[pool]);
        RunSuite();
    }
    Log("TOTAL FAILS ");
    LogU(sTotalFails);
    Log("\n");
    *S = saved;
    RH_InvalidateSettingsHash();
    gRhSelfTestDone = 0xC0FFEE;
}
#endif
