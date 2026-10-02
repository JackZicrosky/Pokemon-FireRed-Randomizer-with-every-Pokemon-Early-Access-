// Debug-only self-test of the randomizer options (never in RELEASE builds).
// Runs from the RH_TEST_EXTRA hook of a quickstart new game: every option is switched on in turn and the rules it
// promises are checked on the real data. Results go to gRhSelfTestLog (plain ASCII) for the emulator harness.
#include "global.h"
#include "pokemon_storage_system.h"
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
#include "data/rh_randomizer_tables.h"
#include "item_menu.h"
#include "event_data.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/trainers.h"
#include "constants/rh_special_shops.h"

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------
#define LOG_SIZE 5120
// The log lives on the heap while the test runs (EWRAM is nearly full); the harness follows this pointer.
EWRAM_DATA char *gRhSelfTestLog = NULL;
EWRAM_DATA u32 gRhSelfTestDone = 0;
static EWRAM_DATA u32 sLogLen = 0;
static EWRAM_DATA u32 sFails = 0;          // in the current test
static EWRAM_DATA u32 sChecks = 0;
static EWRAM_DATA u32 sTotalFails = 0;
static EWRAM_DATA u32 sTestStart = 0;

static void Log(const char *s)
{
    if (gRhSelfTestLog == NULL)
        return;
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
    if (gRhSelfTestLog == NULL)
        return;
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
        Log("/");
        LogU(sChecks);
    }
    Log(" f=");
    LogU(gMain.vblankCounter1 - sTestStart);
    Log("\n");
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
        // (battle forms tied to their ability, like Minior's Shields Down, keep it)
        Check(a[1] != ABILITY_NONE || sp == SPECIES_SHEDINJA || gAbilitiesInfo[gSpeciesInfo[sp].abilities[0]].cantBeSwapped, "ensure two", sp, 0);
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
    u32 relaxed = 0, total = 0;
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
                // the Kanto pool is too small for every chain to find an unused target with the same EXP curve
                if (sBasePool == RH_POOL_GEN1)
                    relaxed += (seen[t[i]] != 0);
                else
                    Check(seen[t[i]] == 0, "converges", sp, t[i]);
                seen[t[i]] = 1;
            }
            if (force)
                Check(!VanillaTarget(sp, t[i]) || RH_PoolIndexOf(t[i]) < 0 || !RH_PoolAllowed(RH_PoolIndexOf(t[i])), "force change", sp, t[i]);
            // with every rule on, a few Pokemon have no unused target that also fits typing / growth: those rules are
            // relaxed for them (best effort, like FVX's relaxing); allow up to 2%
            if (growth && RH_SpeciesBST(t[i]) <= RH_SpeciesBST(sp))
                relaxed++;
            if (typing && !SharesType(t[i], sp))
                relaxed++;
        }
        if (mode == 1)
            Check(Depth(sp, 8) <= (three ? 2 : 7), "depth/cycle", sp, Depth(sp, 8));
        total++;
    }
    Note("relaxed", relaxed);
    Check(relaxed * (sBasePool == RH_POOL_GEN1 ? 3 : 50) <= total, "relaxed rules", relaxed, total);   // Kanto: tiny pool
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
            // (Kanto 151 has fewer legendaries than there are legendary statics: swapping must repeat)
            for (j = 0; j < i; j++)
                Check(RH_DebugStaticResult(j) != r || (mode == 1 && sBasePool == RH_POOL_GEN1), "repeat", o, r);
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
        if (gTMHMItemMoveIds[i].moveId == MOVE_DIG || gTMHMItemMoveIds[i].moveId == MOVE_FLASH)
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
        // Kanto 151 has one Dragon family: Lance's themed team can't be both Dragon and duplicate-free
        if (sBasePool == RH_POOL_GEN1 && (id == TRAINER_ELITE_FOUR_LANCE || id == TRAINER_ELITE_FOUR_LANCE_2))
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
    for (list = 0; list <= RH_SPECIAL_BALLS; list++)
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
    // Guarantee Evolution Items: the evolution seller is randomized too, but every evolution item is still sold.
    Reset();
    S->shopSpecial = TRUE;
    S->shopGuaranteeEvo = TRUE;
    Begin("special shops evo");
    n = 0;
    for (list = 0; list <= RH_SPECIAL_BALLS; list++)
    {
        u32 c = RH_BuildSpecialShop(list, buf, 128);
        for (i = 0; i < c && n < 600; i++)
        {
            for (j = 0; j < n; j++)
                Check(all[j] != buf[i], "duplicate", list, buf[i]);
            all[n++] = buf[i];
        }
    }
    {
        u32 evoSame = 0, c = RH_BuildSpecialShop(RH_SPECIAL_EVO, buf, 128);
        for (i = 0; RH_DebugPoolItem(i) != ITEM_NONE; i++)
        {
            u16 it = RH_DebugPoolItem(i);
            bool32 found = FALSE;
            if (!RH_DebugIsEvolutionItem(it) || !RH_DebugSpecialItemOk(it))
                continue;
            for (j = 0; j < n; j++)
                if (all[j] == it)
                    found = TRUE;
            Check(found, "evo missing", it, 0);
        }
        for (i = 0; i < c; i++)
            if (RH_DebugIsEvolutionItem(buf[i]))
                evoSame++;
        Check(evoSame < c, "evo shop unchanged", evoSame, c);
        Note("evo in evo shop", evoSame);
    }
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
// More options
// ---------------------------------------------------------------------------
static bool32 ProtectedItem(u16 it)
{
    return it == ITEM_NONE || GetItemPocket(it) == POCKET_KEY_ITEMS || (GetItemTMHMIndex(it) > NUM_TECHNICAL_MACHINES);
}

static void TestFieldItems(void)
{
    u32 mode, i, j;
    u16 *res = Alloc(RH_FIELD_ITEM_COUNT * 2);
    for (mode = 1; mode <= 3; mode++)
    {
        u32 changed = 0;
        Reset();
        S->fieldItems = mode;
        S->fieldBanBad = TRUE;
        Begin(mode == 1 ? "field items shuffle" : mode == 2 ? "field items random" : "field items even");
        for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
        {
            u16 o = sRhFieldItems[i].item, r = RH_FieldItem(o, sRhFieldItems[i].flag);
            res[i] = r;
            if (ProtectedItem(o))
                Check(r == o, "protected changed", i, r);
            else
            {
                Check(r != ITEM_NONE && !ProtectedItem(r), "item", i, r);
                Check((GetItemTMHMIndex(o) != 0) == (GetItemTMHMIndex(r) != 0), "tm stays tm", i, r);
            }
            changed += (r != o);
        }
        if (mode == 1)
        {
            // shuffle: every non-TM item appears as often as before (except banned ones)
            for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
            {
                u32 before = 0, after = 0;
                u16 it = sRhFieldItems[i].item;
                if (ProtectedItem(it) || GetItemTMHMIndex(it) != 0 || RH_ItemIsBad(it) || RH_ItemBanned(it))
                    continue;
                for (j = 0; j < RH_FIELD_ITEM_COUNT; j++)
                {
                    before += (sRhFieldItems[j].item == it);
                    after += (res[j] == it);
                }
                Check(after >= 1 && after <= before * 2 + 3, "shuffle count", it, after);
            }
        }
        Check(changed > RH_FIELD_ITEM_COUNT / 3, "changed", changed, 0);
        End();
    }
    Free(res);
}

static void TestShopItems(void)
{
    u32 mode, m, i;
    for (mode = 1; mode <= 2; mode++)
    {
        u32 changed = 0, total = 0;
        Reset();
        S->shopItems = mode;
        S->shopBanOverpowered = TRUE;
        Begin(mode == 1 ? "shop items shuffle" : "shop items random");
        for (m = 0; m < 14; m++)
        {
            for (i = 0; i < 24; i++)
            {
                u16 o = (i % 3 == 0) ? ITEM_POKE_BALL : (i % 3 == 1) ? ITEM_POTION : ITEM_X_ATTACK;
                u16 r = RH_ShopItem(o, m, i);
                if (o != ITEM_X_ATTACK)
                    Check(r == o, "regular kept", m, r);
                else
                {
                    Check(r != ITEM_NONE && GetItemPocket(r) != POCKET_KEY_ITEMS, "item", m, r);
                    if (mode == 2)
                        Check(!RH_ItemIsOverpowered(r), "overpowered", m, r);
                    changed += (r != o);
                    total++;
                }
            }
        }
        if (mode == 2)
            Check(changed > total / 2, "changed", changed, total);
        End();
    }
}

static void TestMiscItems(void)
{
    u32 i;
    Reset();
    S->pickupItems = 1;
    S->pickupBanBad = TRUE;
    S->banLuckyEgg = TRUE;
    S->wildHeldItems = TRUE;
    S->wildBanBadItems = TRUE;
    Begin("pickup/held/lucky egg");
    for (i = 0; i < 18; i++)
    {
        u16 r = RH_PickupItem(ITEM_POTION, i);
        Check(r != ITEM_NONE && r != ITEM_LUCKY_EGG && GetItemPocket(r) != POCKET_KEY_ITEMS, "pickup", i, r);
    }
    for (i = 1; i < 400; i++)
    {
        u16 a = RH_WildHeldItem(i, FALSE, gSpeciesInfo[i].itemCommon), b = RH_WildHeldItem(i, TRUE, gSpeciesInfo[i].itemRare);
        if (gSpeciesInfo[i].itemCommon == ITEM_NONE && gSpeciesInfo[i].itemRare == ITEM_NONE)
            Check(a == ITEM_NONE && b == ITEM_NONE, "nothing stays nothing", i, a);
        Check(a != ITEM_LUCKY_EGG && b != ITEM_LUCKY_EGG, "lucky egg", i, a);
    }
    End();
}

static void TestLevelsAndRates(void)
{
    Reset();
    S->wildLevelModOn = TRUE;
    S->wildLevelMod = 50;
    S->staticLevelModOn = TRUE;
    S->staticLevelMod = -50;
    S->wildCatchRateOn = TRUE;
    S->wildCatchRate = 2;
    S->balanceStaticLevels = TRUE;
    Begin("levels/catch rate");
    Check(RH_ModifyWildLevel(10) == 15, "wild +50%", RH_ModifyWildLevel(10), 0);
    Check(RH_ModifyWildLevel(90) == 100, "wild cap", RH_ModifyWildLevel(90), 0);
    Check(RH_StaticLevel(50) == 25, "static -50%", RH_StaticLevel(50), 0);
    Check(RH_CatchRate(SPECIES_PIDGEY, 3) == 128, "catch rate", RH_CatchRate(SPECIES_PIDGEY, 3), 0);
    Check(RH_CatchRate(SPECIES_PIDGEY, 255) == 255, "catch rate keep", 0, 0);
    Check(RH_BalanceStaticLevel(SPECIES_OMANYTE, 5) == 30, "fossil level", RH_BalanceStaticLevel(SPECIES_OMANYTE, 5), 0);
    S->wildCatchRate = 5;
    RH_InvalidateSettingsHash();
    Check(RH_GuaranteedCatch(), "guaranteed", 0, 0);
    End();
}

static void TestNames(void)
{
    u32 id, changed = 0;
    Reset();
    S->randomTrainerNames = TRUE;
    S->randomTrainerClassNames = TRUE;
    S->moveNames = TRUE;
    S->lowerCaseNames = TRUE;
    Begin("names");
    for (id = 1; id < TRAINERS_COUNT; id++)
    {
        const u8 *n = GetTrainerNameFromId(id);
        Check(StringLength(n) <= TRAINER_NAME_LENGTH, "trainer name length", id, StringLength(n));
        changed += StringCompare(n, GetTrainerStructFromId(id)->trainerName) != 0;
        Check(StringLength(GetTrainerClassNameFromId(id)) <= 12, "class length", id, 0);
    }
    Check(changed > 200, "changed", changed, 0);
    {
        const u8 *n = GetSpeciesName(SPECIES_PIKACHU);
        Check(n[1] >= CHAR_a && n[1] <= CHAR_z, "lower case", n[1], 0);
    }
    S->lowerCaseNames = FALSE;
    RH_InvalidateSettingsHash();
    {
        const u8 *n = GetSpeciesName(SPECIES_PIKACHU);
        Check(n[1] >= CHAR_A && n[1] <= CHAR_Z, "upper case", n[1], 0);
    }
    End();
}

static void TestBattleStyle(void)
{
    u32 id, doubles = 0;
    Reset();
    S->battleStyle = 2;
    S->battleStyleDoubles = TRUE;
    Begin("battle style");
    // the player needs two usable Pokemon for a battle to become a double battle
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_PIDGEY, 10, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gParties[B_TRAINER_PLAYER][1], SPECIES_RATTATA, 10, 0, OTID_STRUCT_PLAYER_ID);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][1]);
    CalculatePlayerPartyCount();
    for (id = 1; id < TRAINERS_COUNT; id++)
    {
        enum TrainerBattleType b = GetTrainerBattleType(id);
        if (id == TRAINER_RIVAL_OAKS_LAB_SQUIRTLE || id == TRAINER_RIVAL_OAKS_LAB_BULBASAUR || id == TRAINER_RIVAL_OAKS_LAB_CHARMANDER)
            Check(b == GetTrainerStructFromId(id)->battleType, "first rival", id, b);
        else
            doubles += (b == TRAINER_BATTLE_TYPE_DOUBLES);
    }
    Check(doubles > TRAINERS_COUNT - 20, "doubles", doubles, 0);
    ZeroMonData(&gParties[B_TRAINER_PLAYER][1]);
    CalculatePlayerPartyCount();
    Check(GetTrainerBattleType(100) == GetTrainerStructFromId(100)->battleType, "one mon: no forced double", 0, 0);
    End();
}

static void TestTraitsMore(void)
{
    u16 sp;
    Reset();
    S->abilities = 1;
    S->abilitiesFollowEvos = TRUE;
    Begin("abilities follow evos");
    FOR_POOL(sp, 1)
    {
        u16 root = RH_TraitRoot(sp);
        if (root != sp && sp != SPECIES_SHEDINJA)
            Check(GetSpeciesAbility(sp, 0) == GetSpeciesAbility(root, 0), "follow", sp, root);
    }
    End();

    Reset();
    S->updateBaseStatsGen = 1;
    Begin("base stats gen 1");
    Check(GetSpeciesBaseStat(SPECIES_PIKACHU, STAT_DEF) == 30, "pikachu def", GetSpeciesBaseStat(SPECIES_PIKACHU, STAT_DEF), 0);
    Check(GetSpeciesBaseStat(SPECIES_BUTTERFREE, STAT_SPATK) == 80, "butterfree spa", GetSpeciesBaseStat(SPECIES_BUTTERFREE, STAT_SPATK), 0);
    End();

    Reset();
    S->mechanicsGen = 1;
    S->updateTypeChart = FALSE;
    Begin("gen 1 chart");
    Check(RH_TypeModifier(TYPE_GHOST, TYPE_PSYCHIC, UQ_4_12(2.0)) == UQ_4_12(0.0), "ghost vs psychic", 0, 0);
    Check(RH_TypeModifier(TYPE_BUG, TYPE_POISON, UQ_4_12(0.5)) == UQ_4_12(2.0), "bug vs poison", 0, 0);
    End();

    Reset();
    S->moveCategory = TRUE;
    Begin("move category");
    {
        u32 m, flipped = 0;
        for (m = 1; m < MOVES_COUNT_GEN9; m++)
        {
            if (!InMovePool(m))
                continue;
            if (gMovesInfo[m].category == DAMAGE_CATEGORY_STATUS)
                Check(GetMoveCategory(m) == DAMAGE_CATEGORY_STATUS, "status", m, 0);
            else
                flipped += GetMoveCategory(m) != gMovesInfo[m].category;
        }
        Check(flipped > 100, "flipped", flipped, 0);
    }
    End();
}

static void TestTutorsAndEggs(void)
{
    static const u16 sTutors[] = { MOVE_DOUBLE_EDGE, MOVE_THUNDER_WAVE, MOVE_ROCK_SLIDE, MOVE_EXPLOSION, MOVE_MEGA_PUNCH, MOVE_MEGA_KICK,
        MOVE_DREAM_EATER, MOVE_SOFT_BOILED, MOVE_SUBSTITUTE, MOVE_SWORDS_DANCE, MOVE_SEISMIC_TOSS, MOVE_COUNTER, MOVE_METRONOME, MOVE_MIMIC, MOVE_BODY_SLAM };
    u32 i, j;
    u16 sp;
    Reset();
    S->tutorMoves = 1;
    S->tmMoves = 1;
    S->tutorCompat = 3;
    S->movesets = 2;
    Begin("tutors/egg moves");
    for (i = 0; i < ARRAY_COUNT(sTutors); i++)
    {
        u16 m = RH_TutorMove(sTutors[i]);
        Check(m != MOVE_NONE, "none", i, 0);
        for (j = 0; j < i; j++)
            Check(RH_TutorMove(sTutors[j]) != m, "repeat", i, m);
        for (j = 1; j <= NUM_TECHNICAL_MACHINES; j++)
            Check(GetTMHMMoveId(j) != m, "tutor is tm", i, m);
    }
    FOR_POOL(sp, 11)
    {
        const u16 *e = GetSpeciesEggMoves(sp);
        for (i = 0; e != NULL && e[i] != MOVE_UNAVAILABLE && i < 32; i++)
            for (j = 0; j < i; j++)
                Check(e[i] != e[j], "egg dup", sp, e[i]);
        Check(CanLearnTeachableMove(sp, RH_TutorMove(sTutors[0])), "tutor full compat", sp, 0);
    }
    End();
}

bool8 RH_IsPostgameNationalDex(void);
static void TestEVsAndDex(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_B][0];
    u32 i, sum = 0;
    u16 item = ITEM_POWER_WEIGHT;
    Reset();
    S->noEVs = TRUE;
    S->nationalDexAtStart = TRUE;
    Begin("no evs/national dex");
    CreateMon(mon, SPECIES_PIDGEY, 10, 0, OTID_STRUCT_PLAYER_ID);
    MonGainEVs(mon, SPECIES_MACHAMP);
    for (i = 0; i < NUM_STATS; i++)
        sum += GetMonData(mon, MON_DATA_HP_EV + i);
    Check(sum == 0, "no evs", sum, 0);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    MonGainEVs(mon, SPECIES_MACHAMP);
    Check(GetMonData(mon, MON_DATA_HP_EV) > 0, "power item still works", GetMonData(mon, MON_DATA_HP_EV), 0);
    RH_OnNewGame();
    Check(IsNationalPokedexEnabled(), "national dex", 0, 0);
    Check(!RH_IsPostgameNationalDex(), "not post-game", 0, 0);
    End();
}

static void TestNewGameItems(void)
{
    Reset();
    S->nuzlocke = TRUE;
    S->randomPcPotion = TRUE;
    S->randomCatchTutorial = TRUE;
    Begin("new game items");
    RH_OnNewGame();
    Check(CheckBagHasItem(ITEM_HM_KIT, 1), "hm kit", 0, 0);
    Check(CheckBagHasItem(ITEM_INFINITE_CANDY, 1), "infinite candy", 0, 0);
    Check(CheckBagHasItem(ITEM_HEALING_KIT, 1), "healing kit", 0, 0);
    Check(CheckBagHasItem(ITEM_RANDOMIZER_SETTINGS, 1), "randomizer settings", 0, 0);
    RemoveBagItem(ITEM_RANDOMIZER_SETTINGS, 1);
    RH_OnContinue();                                   // old saves get it when loaded
    Check(CheckBagHasItem(ITEM_RANDOMIZER_SETTINGS, 1), "settings on continue", 0, 0);
    RH_OnContinue();
    Check(CountTotalItemQuantityInBag(ITEM_RANDOMIZER_SETTINGS) == 1, "only one", CountTotalItemQuantityInBag(ITEM_RANDOMIZER_SETTINGS), 0);
    Check(gPokemonStoragePtr->pcItems[0].itemId != ITEM_NONE, "pc item", 0, 0);
    Check(RH_CatchTutorialSpecies(SPECIES_WEEDLE) != SPECIES_NONE, "tutorial", 0, 0);
    End();
}


// ---------------------------------------------------------------------------
// v0.3 additions
// ---------------------------------------------------------------------------
u32 RH_DebugMenuRowCount(void);
bool32 RH_IsGoodDamagingMove(u16 move);
u32 RH_DebugMenuRowCheck(u32 i);

static void TestMenuText(void)
{
    u32 i, n = RH_DebugMenuRowCount();
    Begin("menu text fits");
    for (i = 0; i < n; i++)
    {
        u32 r = RH_DebugMenuRowCheck(i);
        if (r != 0)
            Note(r == 1 ? "desc" : "overlap", i);
        Check(r == 0, r == 1 ? "desc too wide row" : "label/value overlap row", i, r);
    }
    End();
}

static bool32 IsMega(u16 sp)
{
    return gSpeciesInfo[sp].isMegaEvolution || gSpeciesInfo[sp].isPrimalReversion;
}

static void TestBstModes(void)
{
    u16 sp;
    u32 mode;
    for (mode = 1; mode <= 3; mode++)
    {
        u32 changed = 0;
        u32 sumOld = 0, sumNew = 0;
        Reset();
        S->bstMode = mode;
        S->bstChangePct = 20;
        S->bstFollowEvos = (mode == 1);
        S->bstSeparateLegends = (mode == 2);
        Begin(mode == 1 ? "bst buff/nerf follow" : mode == 2 ? "bst shuffle sep. legends" : "bst random");
        FOR_POOL(sp, 1)
        {
            u32 v = RH_VanillaBST(sp), n = RH_SpeciesBST(sp), actual = BST(sp);
            // stats follow the total (a stat capped at 255 can make it a little lower)
            Check(actual <= n && actual + 12 >= n, "stats sum", sp, actual);
            if (IsMega(sp))
                continue;
            if (mode == 1)
            {
                u16 root = RH_TraitRoot(sp);
                Check(n * 100 >= v * 79 && n * 100 <= v * 121, "within 20%", sp, n);
                if (root != sp && GET_BASE_SPECIES_ID(sp) == sp)
                    Check(abs((s32)(n * 1000 / v) - (s32)(RH_SpeciesBST(root) * 1000 / RH_VanillaBST(root))) <= 15, "family modifier", sp, root);
            }
            if (mode == 3 && GET_BASE_SPECIES_ID(sp) == sp)
                Check(n >= 180 && n < 720, "random range", sp, n);
            changed += (n != v);
        }
        if (mode == 2)
        {
            // shuffle keeps the multiset: sum over the base species is unchanged, legendaries swap among themselves
            u32 s;
            for (s = 1; s < NUM_SPECIES; s++)
            {
                if (!IsSpeciesEnabled(s) || GET_BASE_SPECIES_ID(s) != s || gSpeciesInfo[s].natDexNum == 0 || gSpeciesInfo[s].natDexNum > 1025)
                    continue;
                if (RH_PoolIndexOf(s) < 0)
                    continue;
                sumOld += RH_VanillaBST(s);
                sumNew += RH_SpeciesBST(s);
            }
            Check(sumOld == sumNew, "shuffle sum", sumOld, sumNew);
            Check(RH_IsLegendary(SPECIES_MEWTWO) && RH_SpeciesBST(SPECIES_MEWTWO) >= 500, "legend keeps legend bst", RH_SpeciesBST(SPECIES_MEWTWO), 0);
        }
        Check(changed > 100, "changed", changed, 0);
        End();
    }
}

static void TestFormsFollow(void)
{
    Reset();
    S->types = 2;
    S->abilities = 1;
    S->typesFollowMegas = S->abilitiesFollowMegas = TRUE;
    S->expCurve = 1;
    S->expCurveWho = 1;
    Begin("forms follow base");
    Check(GetSpeciesType(SPECIES_CHARIZARD_MEGA_X, 0) == GetSpeciesType(SPECIES_CHARIZARD, 0), "mega type", 0, 0);
    Check(GetSpeciesType(SPECIES_VENUSAUR_MEGA, 0) == GetSpeciesType(SPECIES_VENUSAUR, 0)
       && GetSpeciesType(SPECIES_VENUSAUR_MEGA, 1) == GetSpeciesType(SPECIES_VENUSAUR, 1), "same-typed mega", 0, 0);
    Check(GetSpeciesAbility(SPECIES_GARDEVOIR_MEGA, 0) == GetSpeciesAbility(SPECIES_GARDEVOIR, 0), "mega ability", 0, 0);
    Check(GetSpeciesType(SPECIES_UNOWN_B, 0) == GetSpeciesType(SPECIES_UNOWN, 0), "cosmetic type", 0, 0);
    Check(GetSpeciesAbility(SPECIES_UNOWN_B, 0) == GetSpeciesAbility(SPECIES_UNOWN, 0), "cosmetic ability", 0, 0);
    Check(GetSpeciesGrowthRate(SPECIES_HOOPA_UNBOUND) == GetSpeciesGrowthRate(SPECIES_HOOPA), "form growth", 0, 0);
    Check(GetSpeciesGrowthRate(SPECIES_LATIOS_MEGA) == GetSpeciesGrowthRate(SPECIES_LATIOS), "mega growth", 0, 0);
    {
        u16 sp;
        FOR_POOL(sp, 1)
        {
            u32 s;
            for (s = 0; s < 3; s++)
            {
                u16 a = GetSpeciesAbility(sp, s);
                Check(a != ABILITY_AURA_GUARD && (a == ABILITY_NONE || gAbilitiesInfo[a].name[0] != CHAR_HYPHEN), "placeholder ability", sp, a);
            }
        }
    }
    S->typesFollowMegas = S->abilitiesFollowMegas = FALSE;
    RH_InvalidateSettingsHash();
    Check(GetSpeciesType(SPECIES_UNOWN_B, 0) == GetSpeciesType(SPECIES_UNOWN, 0), "cosmetic always", 0, 0);
    End();
}

static void TestEvoLevels(void)
{
    u16 sp;
    {
        u32 start;
        Reset();
        S->evolutions = 1;
        S->evoNoConvergence = TRUE;
        S->evoSameTyping = TRUE;
        S->evoForceGrowth = TRUE;
        Begin("bench no-convergence table");
        start = gMain.vblankCounter1;
        GetSpeciesEvolutions(SPECIES_BULBASAUR);
        Note("first lookup frames", gMain.vblankCounter1 - start);
        start = gMain.vblankCounter1;
        GetSpeciesEvolutions(SPECIES_CHARMANDER);
        GetSpeciesEvolutions(SPECIES_SQUIRTLE);
        Note("next two", gMain.vblankCounter1 - start);
        S->evolutions = 2;
        RH_InvalidateSettingsHash();
        start = gMain.vblankCounter1;
        GetSpeciesEvolutions(SPECIES_BULBASAUR);
        Note("every level first", gMain.vblankCounter1 - start);
        End();
    }
    Reset();
    S->evoAdjustLevels = TRUE;
    Begin("adjust evo levels");
    FOR_POOL(sp, 1)
    {
        const struct Evolution *e = GetSpeciesEvolutions(sp);
        u32 i;
        for (i = 0; e != NULL && e[i].method != EVOLUTIONS_END; i++)
            if (e[i].method == EVO_LEVEL)
                Check(e[i].param <= 1 || (e[i].param >= 2 && e[i].param <= 100), "level range", sp, e[i].param);
    }
    {
        const struct Evolution *a = GetSpeciesEvolutions(SPECIES_DRATINI), *b = GetSpeciesEvolutions(SPECIES_DRAGONAIR);
        Note("dratini", a[0].param);
        Note("dragonair", b[0].param);
        Check(b[0].param * 4 >= a[0].param * 5, "25% apart", a[0].param, b[0].param);
        Check(b[0].param >= 40, "dragonite late", b[0].param, 0);
    }
    End();

    Reset();
    S->evolutions = 1;
    S->speciesPool = RH_POOL_ALL_FORMS;
    Begin("region-locked evos");
    {
        u16 t[12];
        Check(Targets(SPECIES_PIKACHU, t) == 1, "pikachu one target", Targets(SPECIES_PIKACHU, t), 0);
        Check(Targets(SPECIES_CUBONE, t) == 1, "cubone one target", Targets(SPECIES_CUBONE, t), 0);
    }
    End();

    Reset();
    Begin("premature evos");
    Check(!RH_IsLegalEvolutionAtLevel(SPECIES_DRAGONITE, 20), "dragonite 20", 0, 0);
    Check(RH_IsLegalEvolutionAtLevel(SPECIES_DRAGONITE, 60), "dragonite 60", 0, 0);
    Check(RH_IsLegalEvolutionAtLevel(SPECIES_PIDGEY, 2), "basic", 0, 0);
    Check(!RH_IsLegalEvolutionAtLevel(SPECIES_VAPOREON, 5), "stone evo estimated", 0, 0);
    {
        u32 id, bad = 0;
        S->trainers = 1;
        S->noPrematureEvos = TRUE;
        RH_InvalidateSettingsHash();
        for (id = 1; id < 200; id += 3)
        {
            const struct Trainer *t = GetTrainerStructFromId(id);
            struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
            u32 i;
            if (t->partySize == 0 || t->party == NULL || t->trainerClass == TRAINER_CLASS_RIVAL_EARLY_FRLG
             || t->trainerClass == TRAINER_CLASS_RIVAL_LATE_FRLG || t->trainerClass == TRAINER_CLASS_CHAMPION_FRLG)
                continue;
            CreateNPCTrainerPartyFromTrainer(party, t);
            for (i = 0; i < PARTY_SIZE; i++)
            {
                u16 s = GetMonData(&party[i], MON_DATA_SPECIES);
                if (s != SPECIES_NONE && !RH_IsLegalEvolutionAtLevel(s, GetMonData(&party[i], MON_DATA_LEVEL)))
                    bad++;
            }
        }
        Check(bad == 0, "trainer premature", bad, 0);
    }
    End();
}

static void TestMoveNamesUnique(void)
{
    u32 m, k, n = 0;
    u16 *list = Alloc(MOVES_COUNT_GEN9 * 2);
    Reset();
    S->moveNames = TRUE;
    Begin("move names unique");
    for (m = 1; m < MOVES_COUNT_GEN9; m++)
        if (InMovePool(m))
            list[n++] = m;
    for (m = 0; m < n; m++)
    {
        u8 a[MOVE_NAME_LENGTH + 1];
        StringCopy(a, GetMoveName(list[m]));
        for (k = m + 1; k < n; k++)
            if (StringCompare(a, GetMoveName(list[k])) == 0)
            {
                Check(FALSE, "same name", list[m], list[k]);
                break;
            }
    }
    Note("punch", 0);
    Log("[");
    {
        u8 buf[MOVE_NAME_LENGTH + 1];
        u32 i;
        StringCopy(buf, GetMoveName(MOVE_FIRE_PUNCH));
        for (i = 0; buf[i] != EOS; i++)
            if (gRhSelfTestLog != NULL && sLogLen < LOG_SIZE - 2)
                gRhSelfTestLog[sLogLen++] = (buf[i] >= CHAR_A && buf[i] <= CHAR_Z) ? 'A' + buf[i] - CHAR_A : (buf[i] >= CHAR_a && buf[i] <= CHAR_z) ? 'a' + buf[i] - CHAR_a : ' ';
    }
    Log("] ");
    Free(list);
    End();
}

static void TestGoodDamagingCounts(void)
{
    u32 i, good = 0;
    Reset();
    S->tmMoves = 1;
    S->tmGoodDamagingOn = TRUE;
    S->tmGoodDamaging = 50;
    S->tmKeepFieldMoves = FALSE;
    Begin("tm good damaging exact");
    for (i = 1; i <= NUM_TECHNICAL_MACHINES; i++)
        good += RH_IsGoodDamagingMove(GetTMHMMoveId(i));
    Check(good >= 25, "at least 50%", good, 0);
    End();

    Reset();
    S->movesets = 2;
    S->movesetGoodDamagingOn = TRUE;
    S->movesetGoodDamaging = 100;
    Begin("moveset good damaging 100%");
    {
        u16 sp;
        FOR_POOL(sp, 9)
        {
            const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(sp);
            u32 k, n = 0, g = 0;
            for (k = 0; l[k].move != LEVEL_UP_MOVE_END && k < 64; k++, n++)
                g += RH_IsGoodDamagingMove(l[k].move);
            Check(g * 10 >= n * 9, "all good", sp, g);
        }
    }
    End();
}

static void TestStartersCustomBlank(void)
{
    u16 st[3];
    u32 i, j;
    Reset();
    S->starters = 1;
    S->customStarters[0] = SPECIES_PIKACHU;
    S->customStarters[1] = SPECIES_NONE;
    S->customStarters[2] = SPECIES_NONE;
    S->starterNoLegends = TRUE;
    S->starterTypes = 3;
    Begin("custom starters + blank");
    for (i = 0; i < 3; i++)
        st[i] = RH_StarterForSlot(i);
    Check(st[0] == SPECIES_PIKACHU, "custom kept", st[0], 0);
    for (i = 1; i < 3; i++)
    {
        Check(!RH_IsLegendary(st[i]), "legend", st[i], 0);
        for (j = 0; j < i; j++)
            Check(!SharesType(st[i], st[j]), "unique", st[i], st[j]);
    }
    End();
}

static void TestFieldTMsKept(void)
{
    u32 mode, i, j;
    for (mode = 2; mode <= 3; mode++)
    {
        Reset();
        S->fieldItems = mode;
        Begin(mode == 2 ? "field TMs random: none lost" : "field TMs even: none lost");
        for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
        {
            u16 o = sRhFieldItems[i].item;
            bool32 found = FALSE;
            if (GetItemTMHMIndex(o) == 0 || GetItemTMHMIndex(o) > NUM_TECHNICAL_MACHINES)
                continue;
            for (j = 0; j < RH_FIELD_ITEM_COUNT && !found; j++)
                found = (RH_FieldItem(sRhFieldItems[j].item, sRhFieldItems[j].flag) == o);
            Check(found, "tm lost", o, 0);
        }
        End();
    }
}

static void TestWildDistinct(void)
{
    u32 zone;
    for (zone = 0; zone <= 2; zone++)
    {
        u16 *map = AllocZeroed(NUM_SPECIES * 2);
        u32 h;
        Reset();
        S->wild = TRUE;
        S->wildZone = zone;
        S->wildMegas = FALSE;
        Begin(zone == 0 ? "wild 1-to-1 whole game" : zone == 1 ? "wild 1-to-1 per location" : "wild 1-to-1 per set");
        for (h = 0; gWildMonHeaders[h].mapGroup != MAP_GROUP(MAP_UNDEFINED); h++)
        {
            const struct WildPokemonHeader *hd = &gWildMonHeaders[h];
            const struct WildPokemonInfo *land = hd->encounterTypes[0].landMonsInfo;
            u32 s, k;
            if (land == NULL)
                continue;
            for (s = 0; s < 12; s++)
            {
                u16 v = land->wildPokemon[s].species, r = RH_WildSpeciesAt(hd->mapGroup, hd->mapNum, land, s, 0, 10);
                for (k = 0; k < s; k++)
                {
                    u16 v2 = land->wildPokemon[k].species;
                    if (v2 != v)
                        Check(RH_WildSpeciesAt(hd->mapGroup, hd->mapNum, land, k, 0, 10) != r, "two species same result", v, r);
                }
                if (zone == 0)
                {
                    if (map[r] == 0)
                        map[r] = v;
                    Check(map[r] == v, "whole game injective", v, r);
                }
            }
        }
        Free(map);
        End();
    }
}

static void TestPalettesAndText(void)
{
    u16 sp;
    u32 changed = 0;
    Reset();
    S->paletteMode = 1;
    S->paletteFollowTypes = TRUE;
    S->paletteShinyFromNormal = TRUE;
    Begin("palettes");
    FOR_POOL(sp, 13)
    {
        const u16 *v = gSpeciesInfo[sp].palette;
        const u16 *p = GetMonSpritePalFromSpecies(sp, FALSE, FALSE);
        u32 i, diff = 0;
        if (v == NULL)
            continue;
        Check(p[0] == v[0], "transparent kept", sp, 0);
        for (i = 1; i < 16; i++)
            diff += (p[i] != v[i]);
        changed += (diff > 0);
        Check(GetMonSpritePalFromSpecies(sp, TRUE, FALSE) == v, "shiny from normal", sp, 0);
    }
    Check(changed > 20, "changed", changed, 0);
    End();

    Reset();
    S->tmMoves = 1;
    Begin("tm text + description");
    {
        static const u8 sText[] = _("TM39 contains ROCK TOMB.");
        u8 buf[80];
        u16 m = GetTMHMMoveId(39);
        StringCopy(buf, sText);
        RH_FixTMText(buf);
        Check(StringCompare(buf, sText) != 0 || m == MOVE_ROCK_TOMB, "text fixed", m, 0);
        Check(GetItemDescription(ITEM_TM39) == GetMoveDescription(m), "tm description", m, 0);
    }
    End();

    Reset();
    S->randomIntroMon = TRUE;
    Begin("intro mon");
    Check(RH_IntroSpecies(SPECIES_NIDORAN_F) != SPECIES_NONE, "valid", 0, 0);
    Note("intro", RH_IntroSpecies(SPECIES_NIDORAN_F));
    End();
}

static void TestRivalSameTeam(void)
{
    u32 id, k, battles = 0, mons = 0;
    u16 roots[6];
    Reset();
    S->trainers = 1;
    S->starters = 2;
    S->rivalCarriesTeam = TRUE;
    S->rivalSameTeam = TRUE;
    S->trainersEvolveOn = TRUE;
    S->additionalMons[1] = 1;
    RH_InvalidateSettingsHash();
    Begin("rival same team");
    for (k = 0; k < 6; k++)
    {
        roots[k] = RH_FamilyRoot(RH_RivalRosterSpecies(k));
        Check(roots[k] != SPECIES_NONE, "roster", k, 0);
    }
    for (id = 1; id < TRAINERS_COUNT; id++)
    {
        const struct Trainer *t = GetTrainerStructFromId(id);
        struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
        u32 i;
        if (t->partySize < 2 || t->party == NULL || (t->trainerClass != TRAINER_CLASS_RIVAL_EARLY_FRLG
         && t->trainerClass != TRAINER_CLASS_RIVAL_LATE_FRLG && t->trainerClass != TRAINER_CLASS_CHAMPION_FRLG))
            continue;
        CreateNPCTrainerPartyFromTrainer(party, t);
        battles++;
        for (i = 0; i < PARTY_SIZE; i++)
        {
            u16 sp = GetMonData(&party[i], MON_DATA_SPECIES), r;
            bool32 ok = FALSE, starter = FALSE;
            if (sp == SPECIES_NONE)
                continue;
            r = RH_FamilyRoot(sp);
            for (k = 0; k < 3; k++)
                if (r == RH_FamilyRoot(RH_StarterForSlot(k)))
                    ok = starter = TRUE;     // his starter keeps the vanilla stage
            for (k = 0; k < 6; k++)
                if (r == roots[k])
                    ok = TRUE;
            Check(ok, "off roster", id, sp);
            Check(RH_IsLegalEvolutionAtLevel(sp, GetMonData(&party[i], MON_DATA_LEVEL)) || r == sp || starter, "too evolved", id, sp);
            mons++;
        }
    }
    Check(battles > 5, "battles", battles, 0);
    Note("mons", mons);
    End();
}

u32 RH_DebugBalancePrice(u32 price);
bool32 RH_DebugIsVivillonPattern(u16 species);
void RH_DebugSetBallShopOpen(bool32 open);

static bool32 SellOnly(u16 it)
{
    enum ItemSortType t = gItemsInfo[it].sortType;
    if (it == ITEM_BOTTLE_CAP || it == ITEM_GOLD_BOTTLE_CAP || it == ITEM_TINY_MUSHROOM || it == ITEM_BIG_MUSHROOM)
        return FALSE;
    return t == ITEM_TYPE_SELLABLE || t == ITEM_TYPE_RELIC || t == ITEM_TYPE_SHARD;
}

// v0.6 bag: one of every item in the game fits (bag pockets, overflow into the PC), and nothing says "bag full".
static void TestBagHoldsEverything(void)
{
    u32 it, added = 0, failed = 0, inBag = 0, inPc = 0;
    Reset();
    Begin("bag holds every item");
    ClearBag();
    CpuFill16(0, gPokemonStoragePtr->pcItems, sizeof(gPokemonStoragePtr->pcItems));
    for (it = 1; it < ITEMS_COUNT; it++)
    {
        if (GetItemPocket(it) >= POCKETS_COUNT || gItemsInfo[it].name[0] == 0)
            continue;
        Check(CheckBagHasSpace(it, 1), "space", it, 0);
        if (AddBagItem(it, 1))
            added++;
        else
            failed++;
    }
    for (it = 1; it < ITEMS_COUNT; it++)
    {
        if (GetItemPocket(it) >= POCKETS_COUNT || gItemsInfo[it].name[0] == 0)
            continue;
        if (CheckBagHasItem(it, 1))
            inBag++;
        else if (CheckPCHasItem(it, 1))
            inPc++;
        else
            Check(FALSE, "lost", it, 0);
    }
    Note("added", added);
    Note("in bag", inBag);
    Note("in PC", inPc);
    Check(failed == 0, "add failed", failed, 0);
    ClearBag();
    CpuFill16(0, gPokemonStoragePtr->pcItems, sizeof(gPokemonStoragePtr->pcItems));
    End();
}

// v0.6 shop rules: price curve, Poke Ball seller prices, sell-only items out of the pools, Nuzlocke without Rare
// Candy, Master Ball in the special-shop pool, and the one-time Master Ball in unrandomized games.
static void TestShopRulesV06(void)
{
    u16 *buf = Alloc(128 * 2);
    u32 list, i, c, m, prev = 0;
    static const u16 sCandy[] = { ITEM_POTION, ITEM_RARE_CANDY, ITEM_MASTER_BALL, ITEM_NONE };
    const u16 *f;

    Reset();
    Begin("v0.6 shop rules");
    // Balance curve: cheap items unchanged, monotonic, the expensive ones cut hard.
    Check(RH_DebugBalancePrice(200) == 200 && RH_DebugBalancePrice(1000) == 1000, "cheap", RH_DebugBalancePrice(200), 0);
    for (i = 1000; i <= 400000; i += 2500)
    {
        u32 p = RH_DebugBalancePrice(i);
        Check(p >= prev && p <= i, "monotonic", i, p);
        prev = p;
    }
    c = RH_DebugBalancePrice(10000);  Check(c >= 6000 && c <= 7500, "10k", c, 0);
    c = RH_DebugBalancePrice(20000);  Check(c >= 9500 && c <= 11500, "20k", c, 0);
    c = RH_DebugBalancePrice(250000); Check(c >= 27000 && c <= 32000, "250k", c, 0);
    Check(GetItemPrice(ITEM_MASTER_BALL) == 100000, "master price", GetItemPrice(ITEM_MASTER_BALL), 0);
    Check(GetItemPrice(ITEM_CHERISH_BALL) != 0 && GetItemPrice(ITEM_BEAST_BALL) != 0, "ball prices", 0, 0);
    S->shopBalancePrices = TRUE;
    RH_InvalidateSettingsHash();
    c = GetItemPrice(ITEM_MASTER_BALL);  Check(c >= 20000 && c <= 25000, "master balanced", c, 0);
    c = GetItemPrice(ITEM_ABILITY_PATCH); Check(c >= 27000 && c <= 32000, "patch balanced", c, 0);
    Check(GetItemPrice(ITEM_POKE_BALL) == 200, "poke ball", GetItemPrice(ITEM_POKE_BALL), 0);

    // Randomized pools: no sell-only items; the Master Ball is in the special-shop pool.
    Reset();
    S->shopItems = 2;
    S->shopSpecial = TRUE;
    S->nuzlocke = TRUE;
    RH_InvalidateSettingsHash();
    Check(RH_DebugSpecialItemOk(ITEM_MASTER_BALL), "master in pool", 0, 0);
    Check(!RH_DebugSpecialItemOk(ITEM_NUGGET) && !RH_DebugSpecialItemOk(ITEM_PEARL), "sell-only pool", 0, 0);
    Check(!RH_DebugSpecialItemOk(ITEM_RARE_CANDY), "nuzlocke candy pool", 0, 0);
    for (list = 0; list <= RH_SPECIAL_BALLS; list++)
    {
        c = RH_BuildSpecialShop(list, buf, 128);
        for (i = 0; i < c; i++)
            Check(!SellOnly(buf[i]) && buf[i] != ITEM_RARE_CANDY, "special item", list, buf[i]);
    }
    for (m = 0; m < 14; m++)
        for (i = 0; i < 24; i++)
        {
            u16 r = RH_ShopItem(ITEM_X_ATTACK, m, i);
            Check(!SellOnly(r) && r != ITEM_RARE_CANDY, "shop item", m, r);
        }
    f = RH_FilterShopList(sCandy);
    for (i = 0; f[i] != ITEM_NONE; i++)
        Check(f[i] != ITEM_RARE_CANDY, "nuzlocke filter", i, f[i]);

    // One-time Master Ball: only at the ball seller, only without shop randomization.
    Reset();
    FlagClear(FLAG_RH_BOUGHT_MASTER_BALL);
    RH_DebugSetBallShopOpen(TRUE);
    Check(RH_ShopItemIsOneTime(ITEM_MASTER_BALL) && !RH_ShopItemSoldOut(ITEM_MASTER_BALL), "one-time", 0, 0);
    Check(!RH_ShopItemIsOneTime(ITEM_ULTRA_BALL), "only master", 0, 0);
    RH_ShopOnPurchase(ITEM_MASTER_BALL);
    Check(RH_ShopItemSoldOut(ITEM_MASTER_BALL), "sold out", 0, 0);
    f = RH_FilterShopList(sCandy);
    for (i = 0; f[i] != ITEM_NONE; i++)
        Check(f[i] != ITEM_MASTER_BALL, "sold out hidden", i, f[i]);
    S->shopItems = 1;
    RH_InvalidateSettingsHash();
    Check(!RH_ShopItemIsOneTime(ITEM_MASTER_BALL) && !RH_ShopItemSoldOut(ITEM_MASTER_BALL), "shop items: repeatable", 0, 0);
    S->shopItems = 0;
    S->shopSpecial = TRUE;
    RH_InvalidateSettingsHash();
    Check(!RH_ShopItemIsOneTime(ITEM_MASTER_BALL) && !RH_ShopItemSoldOut(ITEM_MASTER_BALL), "special: repeatable", 0, 0);
    RH_DebugSetBallShopOpen(FALSE);
    S->shopSpecial = FALSE;
    RH_InvalidateSettingsHash();
    Check(!RH_ShopItemIsOneTime(ITEM_MASTER_BALL), "other shop", 0, 0);
    FlagClear(FLAG_RH_BOUGHT_MASTER_BALL);
    End();
    Free(buf);
}

// "Rival Carries Starter": every rival battle uses exactly the Pokemon he took in Oak's lab, also after the seed
// (and so the starters) changed mid-run.
static void TestRivalStarterFromLab(void)
{
    u32 slot, id, pass;
    static const u16 sBase[3] = { SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE };
    Reset();
    S->trainers = 1;
    S->starters = 2;
    S->rivalCarriesTeam = TRUE;
    S->trainersEvolveOn = TRUE;
    RH_InvalidateSettingsHash();
    Begin("rival starter = lab");
    for (slot = 0; slot < 3; slot++)
    {
        u16 lab = RH_StarterSpecies(sBase[slot]);            // what the lab ball holds (RH_RemapStarterVars)
        VarSet(VAR_RH_RIVAL_STARTER, lab);
        for (pass = 0; pass < 2; pass++)
        {
            if (pass == 1)
            {
                S->seed ^= 0x5A5A5;                          // settings changed mid-run: still the same Pokemon
                RH_InvalidateSettingsHash();
            }
            for (id = 1; id < TRAINERS_COUNT; id++)
            {
                const struct Trainer *t = GetTrainerStructFromId(id);
                struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
                u32 i, stage;
                bool32 has = FALSE, found = FALSE;
                if (t->party == NULL || (t->trainerClass != TRAINER_CLASS_RIVAL_EARLY_FRLG
                 && t->trainerClass != TRAINER_CLASS_RIVAL_LATE_FRLG && t->trainerClass != TRAINER_CLASS_CHAMPION_FRLG))
                    continue;
                for (i = 0; i < t->partySize; i++)
                    if (RH_StarterFamily(t->party[i].species, &stage) == (s32)slot)
                        has = TRUE;
                if (!has)
                    continue;
                CreateNPCTrainerPartyFromTrainer(party, t);
                for (i = 0; i < PARTY_SIZE; i++)
                {
                    u16 sp = GetMonData(&party[i], MON_DATA_SPECIES);
                    if (sp != SPECIES_NONE && RH_FamilyRoot(sp) == RH_FamilyRoot(lab))
                        found = TRUE;
                }
                Check(found, "rival starter", id, lab);
            }
        }
        S->seed ^= 0x5A5A5;
        RH_InvalidateSettingsHash();
    }
    VarSet(VAR_RH_RIVAL_STARTER, SPECIES_NONE);
    End();
}

// Scatterbug / Spewpa / Vivillon: one entry per stage in the pool; with forms on, a pick rolls the wing pattern.
static void TestVivillonPool(void)
{
    struct RhFilter f = {0};
    u32 i, line = 0, patterns = 0, n = 2000;
    Reset();
    S->speciesPool = RH_POOL_ALL_FORMS;
    RH_InvalidateSettingsHash();
    Begin("vivillon one pokemon");
    for (i = 0; i < RH_PoolCount(); i++)
    {
        u16 sp = RH_PoolSpecies(i);
        struct RhFilter one = {0};
        if (RH_DebugIsVivillonPattern(sp))
            Check(!RH_FilterAccepts(&one, i), "pattern pickable", sp, 0);
    }
    for (i = 0; i < n; i++)
    {
        u16 sp = RH_PickWithFilter(&f, RH_Hash(0xB0B, i, 0));
        u16 root = RH_FamilyRoot(sp);
        if (root == RH_FamilyRoot(SPECIES_SCATTERBUG))
        {
            line++;
            if (sp != SPECIES_SCATTERBUG && sp != SPECIES_SPEWPA && sp != SPECIES_VIVILLON)
                patterns++;
        }
    }
    Note("line picks", line);
    Note("patterned", patterns);
    Check(line < 30, "line weight", line, n);                // 3 of ~1260 entries, not 60
    End();
}

u32 RH_DebugSettingsToCode(const struct RhSettings *st, u8 *chars);
bool32 RH_DebugCodeToSettings(struct RhSettings *st, const u8 *chars, u32 n);
u32 RH_DebugSettingsCodeMaxChars(void);
void RH_DebugRandomSettings(struct RhSettings *st, u32 salt);
u32 RH_DebugCompareSettings(const struct RhSettings *a, const struct RhSettings *b);

static void TestSettingsCodes(void)
{
    struct RhSettings *a = AllocZeroed(sizeof(*a)), *b = AllocZeroed(sizeof(*b));
    u8 *code = Alloc(RH_DebugSettingsCodeMaxChars() + 8);
    u32 it, n, maxLen = 0;
    Begin("settings codes");
    for (it = 0; it < 24; it++)
    {
        RH_SetDefaultSettings(a);
        if (it > 0 && it < 12)
            RH_DebugRandomSettings(a, it);
        else if (it >= 12)
        {
            a->trainers = it % 6;
            a->wild = TRUE;
            a->wildLevelMod = -50 + it;
            a->instantText = TRUE;
            a->rivalSameTeam = it & 1;
        }
        if (it & 1)
        {
            StringCopy(a->seedText, COMPOUND_STRING("Hello Wrld"));
            a->seedText[RH_SEED_TEXT_LENGTH] = EOS;
            a->seed = 0xDEADBEE0 + it;
        }
        else
        {
            StringCopy(a->seedText, COMPOUND_STRING("K7QX2MZA"));
            a->seed = 12345;       // hash of the text is used for code-style seed texts
        }
        if (it >= 16)
        {
            n = RH_DebugSettingsToCode(a, code);
            Note("typical", n);
            RH_SetDefaultSettings(b);
            Check(RH_DebugCodeToSettings(b, code, n) && RH_DebugCompareSettings(a, b) == 0, "sparse", it, n);
            continue;
        }
        a->customStarters[0] = SPECIES_PIKACHU + it;
        a->customStarters[2] = (it & 2) ? SPECIES_MEW : SPECIES_NONE;
        a->starterSingleType = it % 18 + 1;
        a->starterBstMin = 300 + it;
        a->starterBstMax = 600 - it;
        n = RH_DebugSettingsToCode(a, code);
        maxLen = max(maxLen, n);
        Check(n > 0 && n <= RH_DebugSettingsCodeMaxChars(), "length", it, n);
        RH_SetDefaultSettings(b);
        Check(RH_DebugCodeToSettings(b, code, n), "decode", it, n);
        Check(RH_DebugCompareSettings(a, b) == 0, "rows differ", it, RH_DebugCompareSettings(a, b));
        if (it & 1)
            Check(b->seed == a->seed, "raw seed", it, b->seed);
        else
            Check(StringCompare(b->seedText, a->seedText) == 0, "seed text", it, 0);
        Check(b->customStarters[0] == a->customStarters[0] && b->customStarters[2] == a->customStarters[2], "starters", it, b->customStarters[0]);
        Check(b->starterSingleType == a->starterSingleType && b->starterBstMin == a->starterBstMin && b->starterBstMax == a->starterBstMax, "fields", it, 0);
        // a typo is caught
        code[n / 2] = (code[n / 2] + 1 + it % 30) & 31;
        Check(!RH_DebugCodeToSettings(b, code, n), "typo accepted", it, 0);
        Check(!RH_DebugCodeToSettings(b, code, n - 1), "short accepted", it, 0);
    }
    Note("code chars", maxLen);
    End();
    Free(a);
    Free(b);
    Free(code);
}

// ---------------------------------------------------------------------------
static void RunSuite(void)
{
#ifdef RH_SELFTEST_NEW_ONLY
    TestBagHoldsEverything();
    TestShopRulesV06();
    TestRivalStarterFromLab();
    TestVivillonPool();
    TestSettingsCodes();
    TestRivalSameTeam();
    TestSpecialShops();
    TestBstModes();
    TestEvoLevels();
    TestGoodDamagingCounts();
    TestStartersCustomBlank();
    TestFieldTMsKept();
    TestWildDistinct();
    if (sBasePool == RH_POOL_ALL)
    {
        TestMenuText();
        TestFormsFollow();
        TestMoveNamesUnique();
        TestPalettesAndText();
    }
    return;
#endif
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
    TestRivalSameTeam();
    TestSettingsCodes();
    TestPrices();
    TestShopRulesV06();
    TestRivalStarterFromLab();
    if (sBasePool == RH_POOL_ALL)
    {
        TestVivillonPool();
        TestBagHoldsEverything();
    }
    TestFieldItems();
    TestShopItems();
    TestMiscItems();
    TestLevelsAndRates();
    TestNames();
    TestBattleStyle();
    TestTraitsMore();
    TestTutorsAndEggs();
    TestBstModes();
    TestEvoLevels();
    TestGoodDamagingCounts();
    TestStartersCustomBlank();
    TestFieldTMsKept();
    TestWildDistinct();
    if (sBasePool == RH_POOL_ALL)
    {
        TestMenuText();
        TestFormsFollow();
        TestMoveNamesUnique();
        TestPalettesAndText();
        TestNewGameItems();
        TestEVsAndDex();
    }

}

void RH_SelfTest(void)
{
    struct RhSettings saved = *S;
    // Debug only: the last LOG_SIZE bytes of the heap (rarely reached; not reset by the InitHeap after new game).
    gRhSelfTestLog = (char *)gHeap + HEAP_SIZE - LOG_SIZE;
    static const u8 sPools[] = { RH_POOL_ALL, RH_POOL_GEN1, RH_POOL_ALL_FORMS };
    static const char *const sPoolNames[] = { "=== pool: all 1025\n", "=== pool: Kanto 151\n", "=== pool: all + forms\n" };
    u32 pool;
    sLogLen = 0;
    sTotalFails = 0;
    gRhSelfTestLog[0] = 0;
    Log("RH SELF TEST\n");
    for (pool = 0; pool < ARRAY_COUNT(sPools); pool++)
    {
#ifdef RH_SELFTEST_POOL
        if (pool != RH_SELFTEST_POOL)
            continue;
#endif
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
