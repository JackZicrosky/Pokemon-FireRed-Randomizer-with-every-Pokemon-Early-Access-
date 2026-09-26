// Type effectiveness options (ported from UPR FVX's TypeEffectivenessRandomizer) and the Battle Mechanics generation.
#include "global.h"
#include "battle_main.h"
#include "rh_internal.h"
#include "constants/battle.h"

enum { EFF_ZERO, EFF_HALF, EFF_NEUTRAL, EFF_DOUBLE, EFF_COUNT };

#define NT 18

struct RhTypeChart
{
    u32 key;
    u8 eff[NT][NT];          // [attacker][defender], indices into gRhMonTypes
};

static EWRAM_DATA struct RhTypeChart sChart = {0};
static EWRAM_DATA s8 sTypeToIndex[NUMBER_OF_MON_TYPES] = {0};
static EWRAM_DATA u32 sRng = 0;

u32 RH_MechanicsGenConfig(void)
{
    u32 gen = gSaveBlock3Ptr->rhSettings.mechanicsGen;
    if (gen == 0 || gen > 9)
        return GEN_LATEST;
    return GEN_1 + gen - 1;
}

static u32 NextRand(void)
{
    sRng ^= sRng << 13;
    sRng ^= sRng >> 17;
    sRng ^= sRng << 5;
    return sRng;
}

static u32 RandBelow(u32 n)
{
    return ((NextRand() >> 16) * n) >> 16;   // no division (slow on the GBA)
}

static u8 EffOfModifier(uq4_12_t m)
{
    if (m == UQ_4_12(0.0))
        return EFF_ZERO;
    if (m < UQ_4_12(1.0))
        return EFF_HALF;
    if (m > UQ_4_12(1.0))
        return EFF_DOUBLE;
    return EFF_NEUTRAL;
}

static const uq4_12_t sEffModifier[EFF_COUNT] = { UQ_4_12(0.0), UQ_4_12(0.5), UQ_4_12(1.0), UQ_4_12(2.0) };

static u8 Idx(u8 type)
{
    return RH_TypeIndexOf(type);
}

// The chart the game starts from: the Gen 9 chart, or the chart of the chosen mechanics generation when
// "Update Type Effectiveness" is off. Fairy always exists.
static void LoadBaseChart(u8 eff[NT][NT])
{
    u32 a, d, gen;
    for (a = 0; a < NT; a++)
        for (d = 0; d < NT; d++)
            eff[a][d] = EffOfModifier(gTypeEffectivenessTable[gRhMonTypes[a]][gRhMonTypes[d]]);
    if (S->updateTypeChart)
        return;
    gen = S->mechanicsGen ? S->mechanicsGen : 9;
    if (gen < 6)
    {
        eff[Idx(TYPE_GHOST)][Idx(TYPE_STEEL)] = EFF_HALF;
        eff[Idx(TYPE_DARK)][Idx(TYPE_STEEL)] = EFF_HALF;
    }
    if (gen < 2)
    {
        eff[Idx(TYPE_BUG)][Idx(TYPE_POISON)] = EFF_DOUBLE;
        eff[Idx(TYPE_POISON)][Idx(TYPE_BUG)] = EFF_DOUBLE;
        eff[Idx(TYPE_GHOST)][Idx(TYPE_PSYCHIC)] = EFF_ZERO;
        eff[Idx(TYPE_ICE)][Idx(TYPE_FIRE)] = EFF_NEUTRAL;
    }
}

static u32 CountAttacking(u8 eff[NT][NT], u32 a, u32 e)
{
    u32 d, n = 0;
    for (d = 0; d < NT; d++)
        n += (eff[a][d] == e);
    return n;
}

static u32 CountDefending(u8 eff[NT][NT], u32 d, u32 e)
{
    u32 a, n = 0;
    for (a = 0; a < NT; a++)
        n += (eff[a][d] == e);
    return n;
}

// "Random" / "Random (balanced)": same number of each effectiveness as the base chart, placed randomly.
static bool32 RandomizeChart(u8 eff[NT][NT], bool32 balanced)
{
    static const u8 sOrder[] = { EFF_ZERO, EFF_HALF, EFF_DOUBLE };
    u8 base[NT][NT];
    u32 counts[EFF_COUNT] = {0};
    u8 maxAtk[EFF_COUNT] = {0}, maxDef[EFF_COUNT] = {0};
    u32 a, d, i, tries = 0;

    memcpy(base, eff, sizeof(base));
    for (a = 0; a < NT; a++)
        for (d = 0; d < NT; d++)
            counts[base[a][d]]++;
    if (balanced)
    {
        for (i = 0; i < EFF_COUNT; i++)
            for (a = 0; a < NT; a++)
            {
                maxAtk[i] = max(maxAtk[i], CountAttacking(base, a, i));
                maxDef[i] = max(maxDef[i], CountDefending(base, a, i));
            }
    }
    memset(eff, EFF_NEUTRAL, NT * NT);
    for (i = 0; i < ARRAY_COUNT(sOrder); i++)
    {
        u32 e = sOrder[i];
        while (counts[e] > 0)
        {
            if (++tries > 20000)
                return FALSE;
            a = RandBelow(NT);
            d = RandBelow(NT);
            if (eff[a][d] != EFF_NEUTRAL)
                continue;
            if (balanced && (CountAttacking(eff, a, e) >= maxAtk[e] || CountDefending(eff, d, e) >= maxDef[e]))
                continue;
            eff[a][d] = e;
            counts[e]--;
        }
    }
    return TRUE;
}

// "Keep Type Identities": every type keeps its number of weaknesses / resistances / immunities offensively (rows)
// and defensively (columns). Checkerboard swaps (a,c)=(b,d)=X, (a,d)=(b,c)=Y -> Y/X preserve every row and column
// count (the same invariant as FVX's chunk swaps, but cheap enough to run on a GBA).
static void KeepIdentities(u8 eff[NT][NT])
{
    u32 swaps = 0, guard = 0;
    while (swaps < 600 && guard < 4000)
    {
        u32 a = RandBelow(NT), b = RandBelow(NT), c = RandBelow(NT), d, n = 0;
        u8 x, y, cand[NT];
        guard++;
        if (a == b)
            continue;
        x = eff[a][c];
        y = eff[b][c];
        if (x == y)
            continue;
        // columns d where the 2x2 square (a,b) x (c,d) is a checkerboard
        for (d = 0; d < NT; d++)
            if (d != c && eff[a][d] == y && eff[b][d] == x)
                cand[n++] = d;
        if (n == 0)
            continue;
        d = cand[RandBelow(n)];
        eff[a][c] = y;
        eff[b][c] = x;
        eff[a][d] = x;
        eff[b][d] = y;
        swaps++;
    }
}

static void Invert(u8 eff[NT][NT], bool32 randomImmunities)
{
    u32 a, d, immunities = 0, sePairs = 0;
    u16 pairs[NT * NT];
    for (a = 0; a < NT; a++)
    {
        for (d = 0; d < NT; d++)
        {
            switch (eff[a][d])
            {
            case EFF_ZERO:
                immunities++;
                // fallthrough
            case EFF_HALF:
                eff[a][d] = EFF_DOUBLE;
                break;
            case EFF_DOUBLE:
                eff[a][d] = EFF_HALF;
                pairs[sePairs++] = a * NT + d;
                break;
            }
        }
    }
    if (randomImmunities)
    {
        while (immunities-- && sePairs)
        {
            u32 i = RandBelow(sePairs);
            eff[pairs[i] / NT][pairs[i] % NT] = EFF_ZERO;
            pairs[i] = pairs[--sePairs];
        }
    }
}

static u32 ChartKey(void)
{
    return S->seed ^ (S->typeChart << 24) ^ (S->updateTypeChart << 27) ^ (S->inverseRandomImmunities << 28)
         ^ (S->mechanicsGen << 16) ^ (S->enabled << 30) ^ 0x5A5A;
}

static void BuildChart(void)
{
    u32 i;
    for (i = 0; i < NUMBER_OF_MON_TYPES; i++)
        sTypeToIndex[i] = -1;
    for (i = 0; i < NT; i++)
        sTypeToIndex[gRhMonTypes[i]] = i;

    sRng = RH_Hash(SALT_TYPE_CHART, 0, 0) | 1;
    LoadBaseChart(sChart.eff);
    switch (S->typeChart)
    {
    case 1:
    case 2:
    {
        u8 backup[NT][NT];
        memcpy(backup, sChart.eff, sizeof(backup));
        for (i = 0; i < 32; i++)
        {
            if (RandomizeChart(sChart.eff, S->typeChart == 2))
                break;
            memcpy(sChart.eff, backup, sizeof(backup));
        }
        break;
    }
    case 3:
        KeepIdentities(sChart.eff);
        break;
    case 4:
        Invert(sChart.eff, S->inverseRandomImmunities);
        break;
    }
    sChart.key = ChartKey();
}

static bool32 ChartIsVanilla(void)
{
    return !S->enabled || (S->typeChart == 0 && S->updateTypeChart);
}

uq4_12_t RH_TypeModifier(enum Type atkType, enum Type defType, uq4_12_t vanilla)
{
    s32 a, d;
    if (ChartIsVanilla())
        return vanilla;
    if (sChart.key != ChartKey())
        BuildChart();
    a = sTypeToIndex[atkType];
    d = sTypeToIndex[defType];
    if (a < 0 || d < 0)
        return vanilla;
    return sEffModifier[sChart.eff[a][d]];
}
